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

// Thông số Case Study 2: Long Fat Network (LFN)
const double BANDWIDTH_MBPS = 1000.0; // Băng thông 1 Gbps
const double RTT_MS = 600.0;          // RTT vệ tinh = 600 ms
const double RTT_SECONDS = RTT_MS / 1000.0;
const double PACKET_LOSS = 0.02;      // Nhiễu sóng mất gói 2%
const double BDP_MB = (BANDWIDTH_MBPS * RTT_SECONDS) / 8.0; // BDP = 75 MB

// Biến trạng thái mô phỏng
double g_tcp_cwnd_mb = 5.0;
double g_tcp_throughput = 100.0;
double g_udp_throughput = 980.0;
double g_bbr_throughput = 990.0;
double g_tcp_eff = 10.0;
double g_udp_eff = 98.0;
double g_bbr_eff = 99.0;
double g_currentTime = 0.0;

std::ofstream g_csvFile;
std::ofstream g_datFile;
Ptr<UniformRandomVariable> g_uv;
FILE *g_gnuplotPipe = NULL;

void SimulateStepCase2() {
    g_currentTime += 1.0;

    // 1. Kiểm tra rớt gói ngẫu nhiên do nhiễu sóng vệ tinh (2%)
    bool packet_lost = (g_uv->GetValue(0.0, 1.0) < PACKET_LOSS);

    // 2. Mô phỏng TCP Reno (Sawtooth: tăng tuyến tính, giảm một nửa khi mất gói)
    if (packet_lost) {
        g_tcp_cwnd_mb = g_tcp_cwnd_mb / 2.0;
        if (g_tcp_cwnd_mb < 2.0) g_tcp_cwnd_mb = 2.0;
    } else {
        g_tcp_cwnd_mb += 4.0;
        if (g_tcp_cwnd_mb > BDP_MB) g_tcp_cwnd_mb = BDP_MB;
    }
    g_tcp_throughput = (g_tcp_cwnd_mb * 8.0) / RTT_SECONDS;
    if (g_tcp_throughput > BANDWIDTH_MBPS) g_tcp_throughput = BANDWIDTH_MBPS;

    // 3. UDP và TCP BBR
    g_udp_throughput = BANDWIDTH_MBPS * (1.0 - (packet_lost ? PACKET_LOSS : 0.005));
    g_bbr_throughput = BANDWIDTH_MBPS * (packet_lost ? 0.98 : 0.99);

    // Tính % hiệu suất
    g_tcp_eff = (g_tcp_throughput / BANDWIDTH_MBPS) * 100.0;
    g_udp_eff = (g_udp_throughput / BANDWIDTH_MBPS) * 100.0;
    g_bbr_eff = (g_bbr_throughput / BANDWIDTH_MBPS) * 100.0;

    // Ghi file DAT cho Gnuplot (Cấu trúc cột: Time, TCP_TP, UDP_TP, BBR_TP, TCP_Eff, UDP_Eff, BBR_Eff, TCP_Cwnd, Loss_Status)
    g_datFile << std::fixed << std::setprecision(1) << g_currentTime << " "
              << g_tcp_throughput << " " << g_udp_throughput << " " << g_bbr_throughput << " "
              << g_tcp_eff << " " << g_udp_eff << " " << g_bbr_eff << " "
              << g_tcp_cwnd_mb << " " << (packet_lost ? 1 : 0) << std::endl;
    g_datFile.flush();

    // Ghi file CSV xuất sang LibreOffice Calc
    g_csvFile << std::fixed << std::setprecision(1) << g_currentTime << ","
              << RTT_MS << "," << BANDWIDTH_MBPS << ","
              << (packet_lost ? "Co" : "Khong") << ","
              << std::setprecision(2) << g_tcp_throughput << "," 
              << g_udp_throughput << "," << g_bbr_throughput << ","
              << g_tcp_cwnd_mb << std::endl;
    g_csvFile.flush();

    // In Terminal theo dõi
    std::cout << "-> Time: " << std::setprecision(0) << g_currentTime 
              << "s | TCP Reno: " << std::setprecision(1) << g_tcp_throughput << " Mbps"
              << " | UDP/UDT: " << g_udp_throughput << " Mbps"
              << " | TCP BBR: " << g_bbr_throughput << " Mbps" << std::endl;

    // Gửi lệnh vẽ giao diện 4 ô (Multiplot 2x2) sang Gnuplot
    if (g_gnuplotPipe) {
        double maxX = (g_currentTime < 5.0) ? 5.0 : g_currentTime;
        
        fprintf(g_gnuplotPipe, "set multiplot layout 2,2 title 'CASE STUDY 2 - LONG FAT NETWORK (1 Gbps - 600ms RTT - Loss 2%%)' font ',11'\n");
        
        // Ô 1: So sánh thông lượng
        fprintf(g_gnuplotPipe, "set size 0.5, 0.5; set origin 0.0, 0.5;\n");
        fprintf(g_gnuplotPipe, "set title 'So sánh thông lượng' font ',9'; set xlabel 'Thời gian (s)'; set ylabel 'Throughput (Mbps)'; set xrange [0:%.1f]; set yrange [0:1100]; set grid\n", maxX);
        fprintf(g_gnuplotPipe, "plot 'realtime_case2.dat' u 1:2 w l lw 1.5 lc rgb '#1f77b4' t 'TCP Reno', "
                               "'realtime_case2.dat' u 1:3 w l lw 1.5 lc rgb '#2ca02c' t 'UDP/UDT-like', "
                               "'realtime_case2.dat' u 1:4 w l lw 1.5 lc rgb '#ff7f0e' t 'TCP BBR-like', "
                               "1000 w l dt 2 lc rgb 'gray' t 'Bandwidth max'\n");

        // Ô 2: Hiệu suất sử dụng đường truyền
        fprintf(g_gnuplotPipe, "set size 0.5, 0.5; set origin 0.5, 0.5;\n");
        fprintf(g_gnuplotPipe, "set title 'Hiệu suất sử dụng đường truyền 1 Gbps' font ',9'; set xlabel 'Thời gian (s)'; set ylabel 'Hiệu suất (%%)'; set xrange [0:%.1f]; set yrange [0:110]; set grid\n", maxX);
        fprintf(g_gnuplotPipe, "plot 'realtime_case2.dat' u 1:5 w l lw 1.5 lc rgb '#1f77b4' t 'TCP Reno', "
                               "'realtime_case2.dat' u 1:6 w l lw 1.5 lc rgb '#2ca02c' t 'UDP/UDT-like', "
                               "'realtime_case2.dat' u 1:7 w l lw 1.5 lc rgb '#ff7f0e' t 'TCP BBR-like', "
                               "100 w l dt 2 lc rgb 'gray' t '100%%'\n");

        // Ô 3: TCP Reno - Cửa sổ truyền và BDP
        fprintf(g_gnuplotPipe, "set size 0.5, 0.5; set origin 0.0, 0.0;\n");
        fprintf(g_gnuplotPipe, "set title 'TCP Reno - Cửa sổ truyền và BDP' font ',9'; set xlabel 'Thời gian (s)'; set ylabel 'Cửa sổ truyền (MB)'; set xrange [0:%.1f]; set yrange [0:85]; set grid\n", maxX);
        fprintf(g_gnuplotPipe, "plot 'realtime_case2.dat' u 1:8 w l lw 1.5 lc rgb '#1f77b4' t 'TCP Reno cwnd', "
                               "%.2f w l dt 2 lc rgb 'gray' t 'BDP = 75MB'\n", BDP_MB);

        // Ô 4: Nhiễu / mất gói 2%
        fprintf(g_gnuplotPipe, "set size 0.5, 0.5; set origin 0.5, 0.0;\n");
        fprintf(g_gnuplotPipe, "set title 'Nhiễu / mất gói 2%%' font ',9'; set xlabel 'Thời gian (s)'; set ylabel 'Trạng thái'; set xrange [0:%.1f]; set yrange [-0.2:1.2]; set ytics ('Không' 0, 'Có' 1); set grid\n", maxX);
        fprintf(g_gnuplotPipe, "plot 'realtime_case2.dat' u 1:9 w lp pt 7 ps 0.8 lc rgb '#1f77b4' t 'Sự kiện mất gói'\n");

        fprintf(g_gnuplotPipe, "unset multiplot\n");
        fflush(g_gnuplotPipe);
    }

    usleep(250000); // Tốc độ chạy mượt mà từng giây

    if (g_currentTime < 100.0) {
        Simulator::Schedule(Seconds(1.0), &SimulateStepCase2);
    }
}

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    g_uv = CreateObject<UniformRandomVariable>();

    g_datFile.open("realtime_case2.dat", std::ios::out | std::ios::trunc);
    g_csvFile.open("ket_qua_Case2_LFN.csv", std::ios::out | std::ios::trunc);
    g_csvFile << "Thoi gian,RTT (ms),Bandwidth (Mbps),Mat goi,TCP Reno Throughput (Mbps),UDP UDT Throughput (Mbps),TCP BBR Throughput (Mbps),TCP cwnd (MB)\n";

    g_gnuplotPipe = popen("gnuplot -persistent", "w");
    if (g_gnuplotPipe) {
        fprintf(g_gnuplotPipe, "set terminal qt size 1100,700 title 'Case 2 LFN Realtime Multiplot (0 -> 100s)'\n");
    }

    std::cout << "==========================================================" << std::endl;
    std::cout << "  BAT DAU MO PHONG CASE STUDY 2: LFN (0s -> 100s)" << std::endl;
    std::cout << "==========================================================" << std::endl;

    Simulator::Schedule(Seconds(1.0), &SimulateStepCase2);
    Simulator::Run();
    Simulator::Destroy();

    g_csvFile.close();
    g_datFile.close();

    if (g_gnuplotPipe) {
        pclose(g_gnuplotPipe);
    }

    std::cout << "\nMo phong Case 2 hoan tat! Dang mo file Excel ket qua..." << std::endl;
    int unused_res = std::system("localc ket_qua_Case2_LFN.csv &");
    (void)unused_res;

    return 0;
}