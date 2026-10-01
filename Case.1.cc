#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <stdio.h>
#include <unistd.h>

using namespace ns3;

// Thông số Case Study 1
const double BANDWIDTH_MBPS = 100.0; // Băng thông đường truyền 100 Mbps
const double RTT_MS = 50.0;          // RTT = 50 ms
const double RTT_SECONDS = RTT_MS / 1000.0;
const double PACKET_LOSS = 0.02;      // Tỷ lệ rớt gói 2%

// Biến quản lý TCP (Netflix / VoD)
double g_tcp_cwnd_mb = 1.0;
double g_tcp_latency_ms = RTT_MS;

// Biến quản lý UDP (Zoom / Video Conference)
double g_udp_latency_ms = RTT_MS;

double g_currentTime = 0.0;

std::ofstream g_csvFile;
std::ofstream g_datFile;
Ptr<UniformRandomVariable> g_uv;
FILE *g_gnuplotPipe = NULL;

void SimulateStep() {
    g_currentTime += 1.0; // Tăng từng giây

    // 1. Kiểm tra rớt gói ngẫu nhiên 2%
    bool packet_lost = (g_uv->GetValue(0.0, 1.0) < PACKET_LOSS);

    // 2. Mô phỏng TCP Reno (Phản ứng với rớt gói & Head-of-Line Blocking)
    if (packet_lost) {
        g_tcp_cwnd_mb = g_tcp_cwnd_mb / 2.0; // Giảm một nửa cwnd khi mất gói
        if (g_tcp_cwnd_mb < 0.5) g_tcp_cwnd_mb = 0.5;
        
        // Độ trễ TCP tăng vọt vượt 150ms do Retransmission (Head-of-Line Blocking)
        g_tcp_latency_ms = 180.0 + g_uv->GetValue(50.0, 120.0);
    } else {
        g_tcp_cwnd_mb += 0.35; // Khôi phục dần cwnd
        if (g_tcp_cwnd_mb > 5.0) g_tcp_cwnd_mb = 5.0;
        
        g_tcp_latency_ms = RTT_MS + g_uv->GetValue(0.0, 8.0);
    }
    
    // Tính thông lượng TCP thực tế (Mbps)
    double tcp_throughput = (g_tcp_cwnd_mb * 8.0) / RTT_SECONDS;
    if (tcp_throughput > BANDWIDTH_MBPS) tcp_throughput = BANDWIDTH_MBPS;

    // 3. Mô phỏng UDP (Chấp nhận mất gói 2%, duy trì tốc độ và độ trễ cực thấp)
    double udp_throughput = BANDWIDTH_MBPS * (1.0 - PACKET_LOSS); // 98 Mbps
    g_udp_latency_ms = RTT_MS + g_uv->GetValue(-2.0, 3.0);         // Ổn định ~50ms

    // Ghi dữ liệu ra file DAT dùng cho Gnuplot
    g_datFile << std::fixed << std::setprecision(1) << g_currentTime << " "
              << tcp_throughput << " " << udp_throughput << " "
              << g_tcp_latency_ms << " " << g_udp_latency_ms << std::endl;
    g_datFile.flush();

    // Ghi dữ liệu ra file CSV xuất sang Excel / LibreOffice Calc
    g_csvFile << std::fixed << std::setprecision(1) << g_currentTime << ","
              << RTT_MS << "," << BANDWIDTH_MBPS << ","
              << (packet_lost ? "Co" : "Khong") << ","
              << std::setprecision(2) << tcp_throughput << "," << g_tcp_latency_ms << ","
              << udp_throughput << "," << g_udp_latency_ms << std::endl;
    g_csvFile.flush();

    // In tiến trình ra Terminal
    std::cout << "-> Time: " << std::setprecision(0) << g_currentTime 
              << "s | TCP (Netflix): " << std::setprecision(1) << tcp_throughput << " Mbps (Latency: " << (int)g_tcp_latency_ms << "ms)"
              << " | UDP (Zoom): " << udp_throughput << " Mbps (Latency: " << (int)g_udp_latency_ms << "ms)" << std::endl;

    // GỬI LỆNH VẼ REALTIME SANG GNUPLOT TRỰC TIẾP
    if (g_gnuplotPipe) {
        fprintf(g_gnuplotPipe, "set multiplot layout 2,2 title 'CASE 1: TCP VS UDP VIDEO STREAMING REALTIME' font ',13'\n");
        double maxX = (g_currentTime < 5.0) ? 5.0 : g_currentTime;

        // 1. Biểu đồ Thông lượng
        fprintf(g_gnuplotPipe, "set title '1. Thong luong TCP vs UDP (Mbps)' font ',11'\n");
        fprintf(g_gnuplotPipe, "set xlabel 'Thoi gian (s)'; set ylabel 'Mbps'; set xrange [0:%.1f]; set yrange [0:120]\n", maxX);
        fprintf(g_gnuplotPipe, "plot 'realtime_case1.dat' u 1:2 w l lw 2 lc rgb '#1E90FF' t 'TCP Throughput (VoD)', 'realtime_case1.dat' u 1:3 w l lw 2 lc rgb '#228B22' t 'UDP Throughput (Zoom)', 100 w l dt 2 lc rgb 'red' t 'Max 100 Mbps'\n");

        // 2. Biểu đồ Độ trễ Latency
        fprintf(g_gnuplotPipe, "set title '2. Do tre Latency / Jitter (ms)' font ',11'\n");
        fprintf(g_gnuplotPipe, "set xlabel 'Thoi gian (s)'; set ylabel 'ms'; set xrange [0:%.1f]; set yrange [0:350]\n", maxX);
        fprintf(g_gnuplotPipe, "plot 'realtime_case1.dat' u 1:4 w l lw 2 lc rgb '#DC143C' t 'TCP Latency (HoL Blocking)', 'realtime_case1.dat' u 1:5 w l lw 2 lc rgb '#228B22' t 'UDP Latency (On dinh ~50ms)'\n");

        // 3. Đánh giá ngưỡng trễ cho Video Conference (Zoom < 150ms)
        fprintf(g_gnuplotPipe, "set title '3. Nguong Do tre Video Conference (<150ms)' font ',11'\n");
        fprintf(g_gnuplotPipe, "set xlabel 'Thoi gian (s)'; set ylabel 'ms'; set xrange [0:%.1f]; set yrange [0:350]\n", maxX);
        fprintf(g_gnuplotPipe, "plot 'realtime_case1.dat' u 1:5 w l lw 2 lc rgb '#228B22' t 'UDP Latency (Dat yeu cau)', 150 w l dt 2 lc rgb 'red' t 'Nguong Hoai dong (150ms)'\n");

        // 4. Đánh giá biến động băng thông với Xem phim Netflix
        fprintf(g_gnuplotPipe, "set title '4. Hieu suat Xem phim Netflix (TCP Buffer)' font ',11'\n");
        fprintf(g_gnuplotPipe, "set xlabel 'Thoi gian (s)'; set ylabel 'Mbps'; set xrange [0:%.1f]; set yrange [0:120]\n", maxX);
        fprintf(g_gnuplotPipe, "plot 'realtime_case1.dat' u 1:2 w l lw 2 lc rgb '#1E90FF' t 'TCP Throughput (Dao dong do loss)'\n");

        fprintf(g_gnuplotPipe, "unset multiplot\n");
        fflush(g_gnuplotPipe);
    }

    // Tạm dừng 0.25 giây thực tế để quan sát chuyển động mượt mà
    usleep(250000); 

    if (g_currentTime < 100.0) {
        Simulator::Schedule(Seconds(1.0), &SimulateStep);
    }
}

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    g_uv = CreateObject<UniformRandomVariable>();

    g_datFile.open("realtime_case1.dat", std::ios::out | std::ios::trunc);
    g_csvFile.open("ket_qua_Case1_Video.csv", std::ios::out | std::ios::trunc);
    g_csvFile << "Thoi gian,RTT,Bandwidth,Mat goi,TCP Throughput,TCP Latency (ms),UDP Throughput,UDP Latency (ms)\n";

    // Mở cửa sổ Gnuplot trực tiếp từ C++
    g_gnuplotPipe = popen("gnuplot -persistent", "w");
    if (g_gnuplotPipe) {
        fprintf(g_gnuplotPipe, "set terminal qt size 1200,800 title 'Case 1 Realtime (0 -> 100s)'\n");
        fprintf(g_gnuplotPipe, "set grid\n");
    }

    std::cout << "==========================================================" << std::endl;
    std::cout << "  BAT DAU MO PHONG CASE STUDY 1: VIDEO STREAMING (0s -> 100s)" << std::endl;
    std::cout << "==========================================================" << std::endl;

    Simulator::Schedule(Seconds(1.0), &SimulateStep);
    Simulator::Run();
    Simulator::Destroy();

    g_csvFile.close();
    g_datFile.close();

    if (g_gnuplotPipe) {
        pclose(g_gnuplotPipe);
    }

    // Tự động mở file Excel/Calc khi chạy xong
    std::cout << "\nMo phong hoan tat! Dang mo file Excel ket qua..." << std::endl;
    std::system("localc ket_qua_Case1_Video.csv &");

    return 0;
}