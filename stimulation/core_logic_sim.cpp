#include <systemc.h>  
#include <iostream>  
#include <iomanip>  
 
const double DETECT_THRESHOLD = 8.0; // cm  
const double ALPHA = 0.3;  

SC_MODULE(QueueController) {  

// Cac cong vao (Inputs)  
sc_in<bool> clk;  
sc_in<bool> btn_next; // Nut Next  
sc_in<bool> btn_reset; // Nut Reset  
sc_in<double> s1_dist; // Cam bien 1  
sc_in<double> s2_dist; // Cam bien 2  
sc_in<double> s3_dist; // Cam bien 3  

// Bien noi bo (Internal Variables)  
int current_ticket;  
double avg_service_time;  
int estimated_people;  
bool is_overloaded;  

// Ham xu ly chinh (Tuong duong void loop)  

void process() {  

// --- LOGIC 1: XU LY NUT BAM NEXT ---  

if (btn_next.read() == true) {  
// Gia lap tinh toan thoi gian cho  
double current_duration = 30.0;  
// Cong thuc Adaptive Average  
if (avg_service_time == 0) avg_service_time = current_duration;  
else avg_service_time = (ALPHA * current_duration) + ((1.0 - ALPHA) * avg_service_time);  
current_ticket++;  
cout << "@" << sc_time_stamp() << " [CTRL] >> NUT NEXT BAM -> Ticket #" << current_ticket  
<< " | Avg Time update: " << avg_service_time << "s" << endl;  

}  

// --- LOGIC 2: XU LY NUT RESET ---  

if (btn_reset.read() == true) {  
current_ticket = 1;  
avg_service_time = 0.0;  
cout << "@" << sc_time_stamp() << " [CTRL] >> SYSTEM RESET!" << endl;  

}  

// --- LOGIC 3: UOC LUONG HANG DOI (Zone-based) ---  

double d1 = s1_dist.read();  
double d2 = s2_dist.read();  
double d3 = s3_dist.read();  

estimated_people = 0;  
if (d3 < DETECT_THRESHOLD && d3 > 0) estimated_people = 15; // Overload  
else if (d2 < DETECT_THRESHOLD && d2 > 0) estimated_people = 10; // Busy  
else if (d1 < DETECT_THRESHOLD && d1 > 0) estimated_people = 5; // Active  
is_overloaded = (estimated_people >= 15);  

// --- IN TRANG THAI RA MAN HINH (Mo phong LCD) ---  

cout << "@" << sc_time_stamp() << " [LCD DISPLAY]: "  
<< "Ticket #" << current_ticket  
<< " | Queue: " << estimated_people << " nguoi"  
<< " | Status: " << (is_overloaded ? "OVERLOAD!!!" : "Normal") << endl;  

}  

// Constructor  

SC_CTOR(QueueController) {  
SC_METHOD(process);  
sensitive << clk.pos(); // Chay theo xung nhip dong ho  

// Khoi tao gia tri ban dau  

current_ticket = 1;  
avg_service_time = 0.0;  
estimated_people = 0;  

}  

};  

// =================================================  
// 2. MODULE TESTBENCH  
// =================================================  

SC_MODULE(Testbench) {  

// Cac cong ra (Outputs) - Noi vao Controller  

sc_out<bool> btn_next, btn_reset;  
sc_out<double> s1_dist, s2_dist, s3_dist;  
sc_in<bool> clk;  
void generate_stimulus() {  

// Tinh huong 1: Khoi dong, chua co ai  
btn_next.write(false); btn_reset.write(false);  
s1_dist.write(100.0); s2_dist.write(100.0); s3_dist.write(100.0); // Khong co vat can  
wait();  

// Tinh huong 2: Co 5 nguoi den (Sensor 1 bi che - <8cm)  

cout << "\n--- [TEST]: 5 Nguoi den ---" << endl;  
s1_dist.write(4.0); // 4cm  
wait();  

// Tinh huong 3: Nhan vien bam Next  

cout << "\n--- [TEST]: Nhan vien bam Next ---" << endl;  
btn_next.write(true);  
wait();  
btn_next.write(false); // Nha nut  
wait();  

// Tinh huong 4: Dong nguoi (Sensor 2 bi che)  

cout << "\n--- [TEST]: 10 Nguoi den (Dong duc) ---" << endl;  
s2_dist.write(4.0);  
wait();  

// Tinh huong 5: Qua tai (Sensor 3 bi che)  

cout << "\n--- [TEST]: 15 Nguoi den (QUA TAI) ---" << endl;  
s3_dist.write(4.0);  
wait();  

// Tinh huong 6: Reset he thong  

cout << "\n--- [TEST]: Reset he thong ---" << endl;  
btn_reset.write(true);  
wait();  
btn_reset.write(false);  
sc_stop(); // Dung mo phong  

}  

SC_CTOR(Testbench) {  
SC_THREAD(generate_stimulus);  
sensitive << clk.pos();  

}  

};  

// =================================================  
// 3. MAIN (Ket noi day)  
// =================================================  

int sc_main(int argc, char* argv[]) {  

// Tin hieu ket noi  
sc_signal<bool> btn_next_sig, btn_reset_sig;  
sc_signal<double> s1_sig, s2_sig, s3_sig;  
sc_clock clk("clk", 1, SC_SEC); // Dong ho chu ky 1s  

// Khoi tao Module  

QueueController ctrl("Controller");  
Testbench tb("Testbench");  

 

// Noi day (Wiring)  

ctrl.clk(clk); tb.clk(clk);  
ctrl.btn_next(btn_next_sig); tb.btn_next(btn_next_sig);  
ctrl.btn_reset(btn_reset_sig); tb.btn_reset(btn_reset_sig);  
ctrl.s1_dist(s1_sig); tb.s1_dist(s1_sig);  
ctrl.s2_dist(s2_sig); tb.s2_dist(s2_sig);  
ctrl.s3_dist(s3_sig); tb.s3_dist(s3_sig);  

// Bat dau chay  

cout << "BAT DAU MO PHONG SYSTEMC..." << endl;  
sc_start();  
return 0;  

} 

 

 
