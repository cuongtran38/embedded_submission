#include <systemc.h>
#include <iostream>
#include <iomanip>

// --- THAM S? C?U HÌNH (Gi?ng h?t file .ino) ---
const double DETECT_THRESHOLD = 8.0; // cm
const double ALPHA = 0.3;

// =================================================
// 1. MODULE B? ?I?U KHI?N TRUNG TÂM (Gi?ng ESP32)
// =================================================
SC_MODULE(QueueController) {
// Các c?ng vào (Inputs)
sc_in<bool> clk;
sc_in<bool> btn_next; // Nút Next
sc_in<bool> btn_reset; // Nút Reset
sc_in<double> s1_dist; // C?m bi?n 1
sc_in<double> s2_dist; // C?m bi?n 2
sc_in<double> s3_dist; // C?m bi?n 3

// Bi?n n?i b? (Internal Variables)
int current_ticket;
double avg_service_time;
int estimated_people;
bool is_overloaded;

// Hàm x? lý chính (T??ng ???ng void loop)
void process() {
// --- LOGIC 1: X? LÝ NÚT B?M NEXT ---
if (btn_next.read() == true) {
// Gi? l?p tính toán th?i gian ph?c v? (random nh? ?? mô ph?ng)
double current_duration = 30.0; // Gi? s? l?n này làm m?t 30s
// Công th?c Adaptive Average
if (avg_service_time == 0) avg_service_time = current_duration;
else avg_service_time = (ALPHA * current_duration) + ((1.0 - ALPHA) * avg_service_time);
current_ticket++;
cout << "@" << sc_time_stamp() << " [CTRL] >> NUT NEXT BAM -> Ticket #" << current_ticket
<< " | Avg Time update: " << avg_service_time << "s" << endl;
}

// --- LOGIC 2: X? LÝ NÚT RESET ---
if (btn_reset.read() == true) {
current_ticket = 1;
avg_service_time = 0.0;
cout << "@" << sc_time_stamp() << " [CTRL] >> SYSTEM RESET!" << endl;
}

// --- LOGIC 3: ??C L??NG HÀNG ??I (Zone-based) ---
double d1 = s1_dist.read();
double d2 = s2_dist.read();
double d3 = s3_dist.read();

estimated_people = 0;
if (d3 < DETECT_THRESHOLD && d3 > 0) estimated_people = 15; // Overload
else if (d2 < DETECT_THRESHOLD && d2 > 0) estimated_people = 10; // Busy
else if (d1 < DETECT_THRESHOLD && d1 > 0) estimated_people = 5; // Active

is_overloaded = (estimated_people >= 15);

// --- IN TR?NG THÁI RA MÀN HÌNH (Mô ph?ng LCD) ---
cout << "@" << sc_time_stamp() << " [LCD DISPLAY]: "
<< "Ticket #" << current_ticket
<< " | Queue: " << estimated_people << " nguoi"
<< " | Status: " << (is_overloaded ? "OVERLOAD!!!" : "Normal") << endl;
}

// Constructor
SC_CTOR(QueueController) {
SC_METHOD(process);
sensitive << clk.pos(); // Ch?y theo xung nh?p ??ng h?
// Kh?i t?o giá tr? ban ??u
current_ticket = 1;
avg_service_time = 0.0;
estimated_people = 0;
}
};

// =================================================
// 2. MODULE TESTBENCH (T?o tình hu?ng gi? l?p)
// =================================================
SC_MODULE(Testbench) {
// Các c?ng ra (Outputs) - N?i vào Controller
sc_out<bool> btn_next, btn_reset;
sc_out<double> s1_dist, s2_dist, s3_dist;
sc_in<bool> clk;

void generate_stimulus() {
// Tình hu?ng 1: Kh?i ??ng, ch?a có ai
btn_next.write(false); btn_reset.write(false);
s1_dist.write(100.0); s2_dist.write(100.0); s3_dist.write(100.0); // Không có v?t c?n
wait();

// Tình hu?ng 2: Có 5 ng??i ??n (Sensor 1 b? che - <8cm)
cout << "\n--- [TEST]: 5 Nguoi den ---" << endl;
s1_dist.write(4.0); // 4cm
wait();

// Tình hu?ng 3: Nhân viên b?m Next
cout << "\n--- [TEST]: Nhan vien bam Next ---" << endl;
btn_next.write(true);
wait();
btn_next.write(false); // Nh? nút
wait();

// Tình hu?ng 4: ?ông ng??i (Sensor 2 b? che)
cout << "\n--- [TEST]: 10 Nguoi den (Dong duc) ---" << endl;
s2_dist.write(4.0);
wait();

// Tình hu?ng 5: Quá t?i (Sensor 3 b? che)
cout << "\n--- [TEST]: 15 Nguoi den (QUA TAI) ---" << endl;
s3_dist.write(4.0);
wait();

// Tình hu?ng 6: Reset h? th?ng
cout << "\n--- [TEST]: Reset he thong ---" << endl;
btn_reset.write(true);
wait();
btn_reset.write(false);
sc_stop(); // D?ng mô ph?ng
}

SC_CTOR(Testbench) {
SC_THREAD(generate_stimulus);
sensitive << clk.pos();
}
};

// =================================================
// 3. MAIN (K?t n?i dây)
// =================================================
int sc_main(int argc, char* argv[]) {
// Tín hi?u k?t n?i
sc_signal<bool> btn_next_sig, btn_reset_sig;
sc_signal<double> s1_sig, s2_sig, s3_sig;
sc_clock clk("clk", 1, SC_SEC); // ??ng h? chu k? 1s

// Kh?i t?o Module
QueueController ctrl("Controller");
Testbench tb("Testbench");

// N?i dây (Wiring)
ctrl.clk(clk); tb.clk(clk);
ctrl.btn_next(btn_next_sig); tb.btn_next(btn_next_sig);
ctrl.btn_reset(btn_reset_sig); tb.btn_reset(btn_reset_sig);
ctrl.s1_dist(s1_sig); tb.s1_dist(s1_sig);
ctrl.s2_dist(s2_sig); tb.s2_dist(s2_sig);
ctrl.s3_dist(s3_sig); tb.s3_dist(s3_sig);

// B?t ??u ch?y
cout << "BAT DAU MO PHONG SYSTEMC..." << endl;
sc_start();

return 0;
}

