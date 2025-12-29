#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h> // [MỚI] Thư viện để lưu dữ liệu vào Flash

#include "web_interface.h"

// --- CẤU HÌNH WIFI ---
const char* ssid = "HE_THONG_HANG_DOI"; 
const char* password = "12345678";

// --- CẤU HÌNH PHẦN CỨNG ---
LiquidCrystal_I2C lcd(0x27, 16, 2); 
RTC_DS3231 rtc;
Preferences preferences; // [MỚI] Khởi tạo đối tượng Preferences

// Định nghĩa chân
#define BUTTON_NEXT_PIN    15  
#define BUTTON_ALARM_PIN  13  
#define BUTTON_RESET_PIN  14  
#define LED_ALARM_PIN     2   

#define TRIG_1  5   
#define ECHO_1  18

#define TRIG_2  25  
#define ECHO_2  26  

#define TRIG_3  17  
#define ECHO_3  19  

// --- WEB SERVER ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); 

// --- BIẾN TOÀN CỤC ---
const float DETECT_THRESHOLD = 8.0; 
const float SLOW_SERVICE_LIMIT = 120.0; 

float avg_service_time = 0.0;       
const float ALPHA = 0.3;             
unsigned long last_press_time = 0;   
int current_ticket = 1;
bool staff_acknowledged = false; 

unsigned long lastDebounceTime1 = 0;  
int lastButtonState1 = HIGH;

char jsonBuffer[100]; 
char timeStr[10];

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    IPAddress ip = client->remoteIP();
    Serial.printf("\n[IoT]: Client %d.%d.%d.%d da ket noi!\n", ip[0], ip[1], ip[2], ip[3]);
    
    // Gửi ngay trạng thái hiện tại cho client mới kết nối
    DateTime now = rtc.now();
    sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    snprintf(jsonBuffer, sizeof(jsonBuffer), "{\"ticket\":%d,\"time\":\"%s\",\"avg\":%.1f}", current_ticket, timeStr, avg_service_time);
    client->text(jsonBuffer);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); 

  // [MỚI] KHÔI PHỤC DỮ LIỆU TỪ FLASH
  // Mở namespace tên là "queue_data", false = chế độ đọc/ghi
  preferences.begin("queue_data", false); 
  
  // Lấy dữ liệu cũ ra. Nếu không có (lần đầu hoặc sau reset) thì lấy mặc định
  current_ticket = preferences.getInt("ticket", 1); 
  avg_service_time = preferences.getFloat("avg_time", 0.0);

  Serial.println("--- KHOI PHUC DU LIEU ---");
  Serial.print("Ticket: "); Serial.println(current_ticket);
  Serial.print("Avg Time: "); Serial.println(avg_service_time);

  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  lcd.init(); lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Khoi phuc...");
  lcd.setCursor(0,1); lcd.print("Ticket: #"); lcd.print(current_ticket);
  delay(1500); // Dừng chút để nhìn thấy số đã khôi phục

  // Khởi tạo chân GPIO
  pinMode(TRIG_1, OUTPUT); pinMode(ECHO_1, INPUT);
  pinMode(TRIG_2, OUTPUT); pinMode(ECHO_2, INPUT);
  pinMode(TRIG_3, OUTPUT); pinMode(ECHO_3, INPUT);
  
  pinMode(BUTTON_NEXT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_ALARM_PIN, INPUT_PULLUP);
  pinMode(BUTTON_RESET_PIN, INPUT_PULLUP);
  pinMode(LED_ALARM_PIN, OUTPUT);

  if (!rtc.begin()) { Serial.println("[ERR]: RTC Error"); }
  if (rtc.lostPower()) { rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); }

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", index_html); });
  server.on("/khach", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", client_html); });
  server.begin();

  lcd.clear(); lcd.print("Ready!"); delay(500);
  last_press_time = millis(); // Reset lại thời điểm bắt đầu đếm ngược cho phiên hiện tại
}

float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 6000); 
  if (duration == 0) return 999.0;
  return duration * 0.0343 / 2;
}

void loop() {
  ws.cleanupClients(); 

  // --- NÚT NEXT (Khách tiếp theo) ---
  int read1 = digitalRead(BUTTON_NEXT_PIN);
  if (read1 == LOW && lastButtonState1 == HIGH && (millis() - lastDebounceTime1) > 250) { 
    lastDebounceTime1 = millis();
    unsigned long current_time = millis();
    float duration = (current_time - last_press_time) / 1000.0;
    
    DateTime now = rtc.now();
    sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    
    snprintf(jsonBuffer, sizeof(jsonBuffer), "{\"ticket\":%d,\"time\":\"%s\",\"avg\":%.1f}", current_ticket, timeStr, avg_service_time);
    ws.textAll(jsonBuffer);

    current_ticket++; 
    last_press_time = current_time; 
    
    if(duration > 2.0) { 
      if (avg_service_time == 0) avg_service_time = duration;
      else avg_service_time = (ALPHA * duration) + ((1.0 - ALPHA) * avg_service_time);
    }

    // [MỚI] LƯU DỮ LIỆU SAU MỖI LẦN TĂNG SỐ
    preferences.putInt("ticket", current_ticket);
    preferences.putFloat("avg_time", avg_service_time);
    Serial.println("[Saved] Data saved to Flash!");
  }
  lastButtonState1 = read1;

  // --- NÚT ALARM (Nhân viên xác nhận chậm) ---
  if (digitalRead(BUTTON_ALARM_PIN) == LOW) {
    staff_acknowledged = true;
    if (avg_service_time > SLOW_SERVICE_LIMIT) {
        avg_service_time = 30.0; 
        // [MỚI] Lưu lại nếu có thay đổi thời gian trung bình
        preferences.putFloat("avg_time", avg_service_time);
    }
  }

  // --- NÚT RESET (Bắt đầu ca mới) ---
  if (digitalRead(BUTTON_RESET_PIN) == LOW) {
    staff_acknowledged = true;
    current_ticket = 1;     
    avg_service_time = 0.0; 
    
    // [MỚI] LƯU TRẠNG THÁI RESET VÀO FLASH
    preferences.putInt("ticket", 1);
    preferences.putFloat("avg_time", 0.0);
    // Hoặc dùng preferences.clear() để xóa sạch cũng được
    
    ws.textAll("{\"type\":\"RESET\"}");
    lcd.clear(); lcd.print("SYSTEM RESET!"); 
    Serial.println("[Reset] Data cleared for new shift.");
    delay(1000);
    last_press_time = millis(); 
  }

  // --- ĐỌC CẢM BIẾN ---
  float d1 = getDistance(TRIG_1, ECHO_1); delay(20);
  float d2 = getDistance(TRIG_2, ECHO_2); delay(20);
  float d3 = getDistance(TRIG_3, ECHO_3);
  
  int estimated_people = 0;
  String status_msg = "IDLE";
  
  if (d3 < DETECT_THRESHOLD && d3 > 0) { 
      estimated_people = 15; 
      status_msg = "OVERLOAD"; 
  }
  else if (d2 < DETECT_THRESHOLD && d2 > 0) { 
      estimated_people = 10; 
      status_msg = "BUSY"; 
  }
  else if (d1 < DETECT_THRESHOLD && d1 > 0) { 
      estimated_people = 5;  
      status_msg = "ACTIVE"; 
  }

  // --- ĐÈN BÁO ---
  bool isSlow = (avg_service_time > SLOW_SERVICE_LIMIT && avg_service_time > 0); 
  bool isOverloaded = (estimated_people >= 15);
  
  if (!isOverloaded && !isSlow) {
    staff_acknowledged = false; 
    digitalWrite(LED_ALARM_PIN, LOW);
  } else if ((isOverloaded || isSlow) && !staff_acknowledged) {
    digitalWrite(LED_ALARM_PIN, HIGH);
  } else digitalWrite(LED_ALARM_PIN, LOW);

  // --- TÍNH TOÁN ĐẾM NGƯỢC ---
  float elapsed_seconds = (millis() - last_press_time) / 1000.0;
  int countdown_next = (int)(avg_service_time - elapsed_seconds);
  if (countdown_next < 0) countdown_next = 0; 

  // --- HIỂN THỊ LCD ---
  lcd.setCursor(0, 0);
  if (isOverloaded) { lcd.print("OVERLOAD! Call.."); } 
  else if (isSlow) { lcd.print("SLOW! Call Staff"); } 
  else {
    lcd.print("Xu ly: #"); lcd.print(current_ticket); lcd.print("     ");
  }
  
  lcd.setCursor(0, 1);
  lcd.print("Next(#"); lcd.print(current_ticket + 1); 
  lcd.print("): "); lcd.print(countdown_next); lcd.print("s   ");
  lcd.setCursor(13, 1); lcd.print("("); lcd.print(estimated_people); lcd.print(")");

  // Debug Serial
  static unsigned long lastSerialTime = 0;
  if (millis() - lastSerialTime > 1000) {
      lastSerialTime = millis();
      Serial.print("Q:"); Serial.print(estimated_people); 
      Serial.print("|Tkt:"); Serial.print(current_ticket);
      Serial.print("|Avg:"); Serial.println(avg_service_time);
  }
  
  delay(10); 
}
