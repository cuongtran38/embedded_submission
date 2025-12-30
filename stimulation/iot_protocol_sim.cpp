#include <systemc.h> 
#include <iostream> 
#include <string> 
#include <sstream> // Thư viện để ghép chuỗi (JSON/CSV) 
#include <iomanip> // Để format giờ 

using namespace std; 
// -------------------------------------------------------------------------- 
// MODULE: IoT_QueueSystem (Mô phỏng ESP32 + WiFi + RTC) 
// -------------------------------------------------------------------------- 
SC_MODULE(IoT_QueueSystem) { 
    // INPUTS (Từ phần cứng) 
    sc_in<bool> btn_next;    // Nút Next (Gửi dữ liệu) 
    sc_in<bool> btn_reset;   // Nút Reset 
    // OUTPUTS (Mô phỏng LED/LCD) 
    sc_out<bool> wifi_led;   // LED báo trạng thái WiFi 

    // INTERNAL VARIABLES 
    int current_ticket;
    bool wifi_connected; 
    // Giả lập thời gian bắt đầu (Ví dụ: 08:30:00) 
    const int START_HOUR = 8; 
    const int START_MIN = 30; 
    // Hàm giả lập RTC: Trả về chuỗi giờ hiện tại dựa trên thời gian mô phỏng 
    string get_rtc_time() { 

        // Lấy thời gian mô phỏng hiện tại (tính bằng giây) 
        double sim_time = sc_time_stamp().to_seconds(); 
        // Cộng dồn vào thời gian gốc 

        int total_seconds = (START_HOUR * 3600) + (START_MIN * 60) + (int)sim_time; 
        int h = (total_seconds / 3600) % 24;
        int m = (total_seconds % 3600) / 60; 
        int s = total_seconds % 60; 

        stringstream ss; 
        ss << setfill('0') << setw(2) << h << ":"  
           << setfill('0') << setw(2) << m << ":"  
           << setfill('0') << setw(2) << s; 
        return ss.str(); 
    } 
    // Hàm giả lập kết nối WiFi 
    void wifi_process() { 
        cout << "@" << sc_time_stamp() << " [WiFi Kernel]: Starting WiFi..." << endl; 
        wifi_connected = false; 
        wifi_led.write(false); 
        // Giả lập độ trễ kết nối (2 giây) 
        wait(2, SC_SEC); 
        wifi_connected = true; 
        wifi_led.write(true); 
        cout << "@" << sc_time_stamp() << " [WiFi Kernel]: CONNECTED! IP: 192.168.1.15" << endl; 
        cout << "---------------------------------------------------" << endl; 

    } 
    // Hàm xử lý logic chính (Nút bấm -> Tạo JSON/CSV) 
    void main_logic() { 
        while (true) { 
            // Chờ sự kiện nút bấm hoặc reset 
            wait();  
            // 1. XỬ LÝ NÚT NEXT (Gửi dữ liệu IoT) 
            if (btn_next.read() == 0) { // Active Low (Nhấn nút) 
                // Chờ debounce (mô phỏng) 
                wait(0.1, SC_SEC);  
                if (wifi_connected) { 
                    string time_str = get_rtc_time(); 
                    // --- MÔ PHỎNG TẠO JSON (CHO WEB) --- 
                    stringstream json_ss; 
                    json_ss << "{\"ticket\": " << current_ticket  
                            << ", \"time\": \"" << time_str << "\"}"; 
                    cout << "@" << sc_time_stamp() << " [WEBSOCKET TX]: "  
                         << json_ss.str() << " --> Sent to Web Dashboard" << endl; 
                    // --- MÔ PHỎNG TẠO CSV (CHO EXCEL) --- 
                    stringstream csv_ss; 
                    csv_ss << current_ticket << "," << time_str; 
                    cout << "@" << sc_time_stamp() << " [SERIAL CSV]  : "  
                         << csv_ss.str() << " --> Ready for Excel Log" << endl; 
                    // Tăng số thứ tự 
                    current_ticket++; 
                } else { 
                    cout << "@" << sc_time_stamp() << " [ERROR]: WiFi not ready!" << endl; 
                } 
            } 
            // 2. XỬ LÝ NÚT RESET 
            if (btn_reset.read() == 0) { 
                current_ticket = 1; 
                cout << "@" << sc_time_stamp() << " [SYSTEM]: RESET TICKET TO #1" << endl; 
                wait(1, SC_SEC); // Tránh lặp 
            } 
        } 
    } 
    SC_CTOR(IoT_QueueSystem) { 
        current_ticket = 1; 
        wifi_connected = false; 
        SC_THREAD(wifi_process); // Chạy tiến trình WiFi riêng 
        SC_THREAD(main_logic);   // Chạy logic chính 
        sensitive << btn_next.neg();  // Nhạy với sườn xuống (Nhấn nút) 
        sensitive << btn_reset.neg(); 
    } 
}; 
// -------------------------------------------------------------------------- 
// MODULE: Testbench (Giả lập người dùng & Môi trường) 
// -------------------------------------------------------------------------- 
SC_MODULE(Testbench) { 
    sc_out<bool> b_next, b_reset; 
    sc_in<bool> w_led; 
    void generate_scenario() { 
        // Trạng thái ban đầu: Nút nhả (High) 
        b_next.write(1);  
        b_reset.write(1); 

        // 1. Chờ WiFi kết nối (Hệ thống tự chạy process WiFi) 
        wait(3, SC_SEC);  

        // 2. KHÁCH 1 ĐẾN: Ấn nút phục vụ 
        cout << "\n--- SCENARIO: KHACH HANG SO 1 ---" << endl; 
        b_next.write(0); // Nhấn 
        wait(0.2, SC_SEC); 
        b_next.write(1); // Nhả 
        wait(5, SC_SEC); // Thời gian phục vụ giả lập 
        // 3. KHÁCH 2 ĐẾN: Ấn nút next 
        cout << "\n--- SCENARIO: KHACH HANG SO 2 ---" << endl; 
        b_next.write(0); 
        wait(0.2, SC_SEC); 
        b_next.write(1); 
        wait(10, SC_SEC); // Phục vụ lâu hơn chút 
        // 4. KHÁCH 3 ĐẾN 
        cout << "\n--- SCENARIO: KHACH HANG SO 3 ---" << endl; 
        b_next.write(0); wait(0.2, SC_SEC); b_next.write(1); 
        wait(2, SC_SEC); 
     
        // 5. CUỐI CA: Reset hệ thống 
        cout << "\n--- SCENARIO: RESET HE THONG ---" << endl; 
        b_reset.write(0); wait(0.5, SC_SEC); b_reset.write(1); 
        // 6. KHÁCH MỚI (Sau khi reset) 
        cout << "\n--- SCENARIO: KHACH MOI (SAU RESET) ---" << endl; 
        b_next.write(0); wait(0.2, SC_SEC); b_next.write(1); 
        wait(2, SC_SEC); 
        sc_stop(); 
    } 
    SC_CTOR(Testbench) { 
        SC_THREAD(generate_scenario); 
    } 
}; 
// -------------------------------------------------------------------------- 
// MAIN 
// -------------------------------------------------------------------------- 
int sc_main(int argc, char* argv[]) { 
    // Tín hiệu nối dây 
    sc_signal<bool> sig_next, sig_reset; 
    sc_signal<bool> sig_wifi_led; 
    // Khởi tạo module 
    IoT_QueueSystem iot_system("ESP32_IoT"); 
    Testbench tb("Testbench"); 

    // Nối dây 
    tb.b_next(sig_next); 
     tb.b_reset(sig_reset); 
    tb.w_led(sig_wifi_led); 
    iot_system.btn_next(sig_next); 
    iot_system.btn_reset(sig_reset); 
    iot_system.wifi_led(sig_wifi_led); 
    // Chạy mô phỏng 
    cout << "===================================================" << endl; 
    cout << "   SYSTEMC IOT SIMULATION (WEEK 5: WEB & LOGS)     " << endl; 
    cout << "===================================================" << endl; 
    sc_start(); 

    return 0; 

} 
