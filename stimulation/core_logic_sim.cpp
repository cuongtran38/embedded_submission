#include <systemc.h> 

#include <iostream> 

#include <iomanip> // Thư viện để format in ấn cho đẹp 

 

using namespace std; 

 

// -------------------------------------------------------------------------- 

// MODULE: QueueMonitor (Mô phỏng ESP32) 

// -------------------------------------------------------------------------- 

SC_MODULE(QueueMonitor) { 

// INPUTS 

sc_in<double> dist_s1, dist_s2, dist_s3; // 3 Cảm biến 

sc_in<bool> btn_service; // Nút 1: Khách xong (Active LOW) 

sc_in<bool> btn_staff; // Nút 2: Nhân viên (Active LOW) 

 

// OUTPUTS 

sc_out<bool> led_alarm; // Đèn báo động 

 

// INTERNAL VARIABLES 

double avg_service_time; 

bool staff_acknowledged; 

bool last_btn_svc_state; 

bool last_btn_staff_state; 

 

// CONSTANTS 

const double DETECT_THRESHOLD = 10.0; 

const double SLOW_LIMIT = 120.0; 

 

void process_logic() { 

// 1. ĐỌC CẢM BIẾN & TÍNH HÀNG ĐỢI 

double d1 = dist_s1.read(); 

double d2 = dist_s2.read(); 

double d3 = dist_s3.read(); 

int queue_len = 0; 

 

if (d3 < DETECT_THRESHOLD) queue_len = 10; 

else if (d2 < DETECT_THRESHOLD) queue_len = 5; 

else if (d1 < DETECT_THRESHOLD) queue_len = 1; 

 

// 2. XỬ LÝ NÚT SERVICE (Mô phỏng EMA) 

bool btn_svc_curr = btn_service.read(); 

if (btn_svc_curr == 0 && last_btn_svc_state == 1) { // Nhấn xuống (Falling edge) 

// Giả lập: Mỗi lần nhấn là đo được 1 khoảng thời gian ngẫu nhiên 

// Để test logic, ta giả định lần này phục vụ mất 25s 

double measured = 25.0;  

// Công thức EMA: New = 0.3 * Measured + 0.7 * Old 

avg_service_time = (0.3 * measured) + (0.7 * avg_service_time); 

// In log sự kiện 

cout << "\n>>> EVENT: Khach hang bam nut 'Xong' (Avg moi: " << avg_service_time << "s)" << endl; 

} 

last_btn_svc_state = btn_svc_curr; 

 

// 3. XỬ LÝ NÚT STAFF (Reset lỗi) 

bool btn_staff_curr = btn_staff.read(); 

if (btn_staff_curr == 0 && last_btn_staff_state == 1) { // Nhấn xuống 

staff_acknowledged = true; 

if (avg_service_time > SLOW_LIMIT) { 

avg_service_time = 30.0; // Reset về chuẩn 

cout << "\n>>> EVENT: Nhan vien da RESET thoi gian phuc vu!" << endl; 

} 

} 

last_btn_staff_state = btn_staff_curr; 

 

// 4. LOGIC ĐÈN BÁO ĐỘNG (ALARM) 

bool isOverloaded = (queue_len >= 10); 

bool isSlow = (avg_service_time > SLOW_LIMIT); 

 

if (!isOverloaded && !isSlow) { 

staff_acknowledged = false; // Tự động reset cờ khi hết lỗi 

led_alarm.write(false); // Tắt đèn 

}  

else if ((isOverloaded || isSlow) && !staff_acknowledged) { 

led_alarm.write(true); // Bật đèn 

}  

else { 

led_alarm.write(false); // Tắt đèn (Do nhân viên đã xác nhận) 

} 

 

// 5. MÔ PHỎNG HIỂN THỊ LCD & LED 

print_simulation_ui(queue_len, avg_service_time, isOverloaded, isSlow, led_alarm.read()); 

} 

 

// Hàm vẽ giao diện LCD giả lập ra màn hình console 

void print_simulation_ui(int q, double avg, bool ovr, bool slow, bool led) { 

// Chỉ in khi có sự thay đổi lớn để đỡ rối mắt 

cout << "@" << sc_time_stamp() << " -------------------------" << endl; 

// Mô phỏng dòng 1 LCD 

cout << "| LCD L1: "; 

if (ovr) cout << "OVERLOAD! Call.. |" << endl; 

else if (slow) cout << "SLOW! Call Staff |" << endl; 

else cout << "Queue: " << q << " pers |" << endl; 

 

// Mô phỏng dòng 2 LCD 

int wait = q * avg; 

cout << "| LCD L2: Wait: " << wait/60 << "m " << wait%60 << "s |" << endl; 

cout << "-------------------------" << endl; 

// Trạng thái LED 

cout << "[LED ALARM]: " << (led ? "ON (!!!)" : "OFF (...)") << endl; 

cout << endl; 

} 

 

SC_CTOR(QueueMonitor) { 

avg_service_time = 30.0; // Mặc định 

staff_acknowledged = false; 

last_btn_svc_state = 1; 

last_btn_staff_state = 1; 

 

SC_METHOD(process_logic); 

sensitive << dist_s1 << dist_s2 << dist_s3 << btn_service << btn_staff; 

} 

}; 

 

// -------------------------------------------------------------------------- 

// MODULE: Testbench (Kịch bản kiểm thử) 

// -------------------------------------------------------------------------- 

SC_MODULE(Testbench) { 

sc_out<double> s1, s2, s3; 

sc_out<bool> b_svc, b_staff; 

 

void generate_scenario() { 

// Mặc định ban đầu: Không có người, Nút nhả (High - Pullup logic) 

s1.write(200); s2.write(200); s3.write(200); 

b_svc.write(1); b_staff.write(1); 

wait(5, SC_SEC); 

 

// KỊCH BẢN 1: Người vào đông dần -> QUÁ TẢI 

cout << "\n--- SCENARIO 1: KHACH VAO DONG ---" << endl; 

s1.write(50); wait(2, SC_SEC); // 1 người 

s2.write(50); wait(2, SC_SEC); // 5 người 

s3.write(50); wait(2, SC_SEC); // 10 người -> LED phải sáng! 

 

// KỊCH BẢN 2: Nhân viên đến bấm nút xác nhận -> Đèn tắt 

cout << "\n--- SCENARIO 2: NHAN VIEN XAC NHAN ---" << endl; 

wait(2, SC_SEC); 

b_staff.write(0); // Nhấn nút Staff 

wait(0.5, SC_SEC); 

b_staff.write(1); // Nhả nút 

wait(5, SC_SEC); // Đèn phải tắt dù hàng vẫn đông 

 

// KỊCH BẢN 3: Khách vãn bớt -> Hệ thống về bình thường 

cout << "\n--- SCENARIO 3: KHACH VE BOT ---" << endl; 

s3.write(200); // Hết người ở cuối 

wait(2, SC_SEC); 

s2.write(200); // Hết người ở giữa 

wait(5, SC_SEC); 

 

// KỊCH BẢN 4: Phục vụ khách (Ấn nút Service) 

cout << "\n--- SCENARIO 4: PHUC VU KHACH ---" << endl; 

b_svc.write(0); wait(0.5, SC_SEC); b_svc.write(1); // Ấn lần 1 

wait(2, SC_SEC); 

sc_stop(); 

} 

 

SC_CTOR(Testbench) { 

SC_THREAD(generate_scenario); 

} 

}; 

 