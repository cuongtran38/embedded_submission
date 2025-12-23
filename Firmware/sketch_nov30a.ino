#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "web_interface.h"

// --- CẤU HÌNH WIFI ---
const char* ssid = "HE_THONG_HANG_DOI"; 
const char* password = "12345678";

// --- CẤU HÌNH PHẦN CỨNG ---
LiquidCrystal_I2C lcd(0x27, 16, 2); 
RTC_DS3231 rtc;

// Định nghĩa chân
#define BUTTON_SVC_PIN    15  
#define BUTTON_ALARM_PIN  13  
#define BUTTON_RESET_PIN  14  
#define LED_ALARM_PIN     2   

#define TRIG_1  5   
#define ECHO_1  18

// Chân Sensor 2 (giữ nguyên chân 25, 26 như đã sửa trước đó)
#define TRIG_2  25  
#define ECHO_2  26  

#define TRIG_3  17  
#define ECHO_3  19  

// --- WEB SERVER ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); 

// --- BIẾN TOÀN CỤC ---
const float DETECT_THRESHOLD = 8.0; // Ngưỡng phát hiện 8cm
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
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); 

  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  lcd.init(); lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Khoi dong...");
  
  // Khởi tạo chân GPIO
  pinMode(TRIG_1, OUTPUT); pinMode(ECHO_1, INPUT);
  pinMode(TRIG_2, OUTPUT); pinMode(ECHO_2, INPUT);
  pinMode(TRIG_3, OUTPUT); pinMode(ECHO_3, INPUT);
  
  pinMode(BUTTON_SVC_PIN, INPUT_PULLUP);
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

  lcd.clear(); lcd.print("Ready!"); delay(1000);
  last_press_time = millis();
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

  // --- NÚT NEXT ---
  int read1 = digitalRead(BUTTON_SVC_PIN);
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
  }
  lastButtonState1 = read1;

  // --- NÚT ALARM ---
  if (digitalRead(BUTTON_ALARM_PIN) == LOW) {
    staff_acknowledged = true;
    if (avg_service_time > SLOW_SERVICE_LIMIT) avg_service_time = 30.0; 
  }

  // --- NÚT RESET ---
  if (digitalRead(BUTTON_RESET_PIN) == LOW) {
    staff_acknowledged = true;
    current_ticket = 1;     
    avg_service_time = 0.0; 
    ws.textAll("{\"type\":\"RESET\"}");
    lcd.clear(); lcd.print("SYSTEM RESET!"); delay(1000);
    last_press_time = millis(); 
  }

  // --- ĐỌC CẢM BIẾN ---
  float d1 = getDistance(TRIG_1, ECHO_1); delay(20);
  float d2 = getDistance(TRIG_2, ECHO_2); delay(20);
  float d3 = getDistance(TRIG_3, ECHO_3);
  

  int estimated_people = 0;
  String status_msg = "IDLE";
  
  // Ưu tiên kiểm tra từ xa đến gần
  if (d3 < DETECT_THRESHOLD && d3 > 0) { 
      estimated_people = 15; // Sensor 3 chạm -> 15 người
      status_msg = "OVERLOAD"; 
  }
  else if (d2 < DETECT_THRESHOLD && d2 > 0) { 
      estimated_people = 10; // Sensor 2 chạm -> 10 người
      status_msg = "BUSY"; 
  }
  else if (d1 < DETECT_THRESHOLD && d1 > 0) { 
      estimated_people = 10;  // Sensor 1 chạm -> 5 người
      status_msg = "ACTIVE"; 
  }

  // --- ĐÈN BÁO ---
  bool isSlow = (avg_service_time > SLOW_SERVICE_LIMIT && avg_service_time > 0); 
  // Cập nhật điều kiện Overload là >= 15 người
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
      Serial.print("|S1:"); Serial.print(d1);
      Serial.print("|S2:"); Serial.print(d2);
      Serial.println("|S3:"); Serial.println(d3);
  }
  
  delay(10); 
}