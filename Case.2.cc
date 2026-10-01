import csv
import os
import random
import time
import matplotlib.pyplot as plt

# ==========================================
# Thông số Case Study 2: Long Fat Network (LFN)
# ==========================================
BANDWIDTH_MBPS = 1000.0  # Băng thông 1 Gbps
RTT_MS = 600.0           # RTT vệ tinh = 600 ms
RTT_SECONDS = RTT_MS / 1000.0
PACKET_LOSS = 0.02       # Nhiễu sóng mất gói 2%
BDP_MB = (BANDWIDTH_MBPS * RTT_SECONDS) / 8.0  # BDP = 75 MB
FILE_SIZE_GB = 50.0      # Dung lượng file giả định

# Biến trạng thái mô phỏng
g_tcp_cwnd_mb = 5.0
g_tcp_throughput = 100.0
g_udp_throughput = 980.0
g_bbr_throughput = 990.0
g_tcp_eff = 10.0
g_udp_eff = 98.0
g_bbr_eff = 99.0
g_currentTime = 0.0

# Lưu trữ dữ liệu để vẽ biểu đồ realtime
time_data = []
tcp_tp_data = []
udp_tp_data = []
bbr_tp_data = []
tcp_eff_data = []
udp_eff_data = []
bbr_eff_data = []
tcp_cwnd_data = []
loss_status_data = []

def main():
    global g_tcp_cwnd_mb, g_tcp_throughput, g_udp_throughput, g_bbr_throughput
    global g_tcp_eff, g_udp_eff, g_bbr_eff, g_currentTime

    # Mở file CSV để ghi kết quả
    csv_filename = "case_study2.csv"
    csv_file = open(csv_filename, mode='w', newline='', encoding='utf-8')
    csv_writer = csv.writer(csv_file)
    csv_writer.writerow([
        "Thoi gian", "RTT (ms)", "Bandwidth (Mbps)", "Mat goi",
        "TCP Reno Throughput (Mbps)", "UDP UDT Throughput (Mbps)",
        "TCP BBR Throughput (Mbps)", "TCP cwnd (MB)"
    ])

    # Cấu hình giao diện Matplotlib (Multiplot 2x2)
    plt.ion()
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(12, 7))
    fig.canvas.manager.set_window_title('Case 2 LFN Realtime Multiplot (0 -> 100s)')
    fig.suptitle('CASE STUDY 2 - LONG FAT NETWORK (1 Gbps - 600ms RTT - Loss 2%)', fontsize=11)

    while g_currentTime < 100.0:
        g_currentTime += 1.0

        # 1. Kiểm tra rớt gói ngẫu nhiên do nhiễu sóng vệ tinh (2%)
        packet_lost = (random.uniform(0.0, 1.0) < PACKET_LOSS)

        # 2. Mô phỏng TCP Reno (Sawtooth)
        if packet_lost:
            g_tcp_cwnd_mb = g_tcp_cwnd_mb / 2.0
            if g_tcp_cwnd_mb < 2.0:
                g_tcp_cwnd_mb = 2.0
            tcp_behavior = "Giảm một nửa do mất gói"
        else:
            g_tcp_cwnd_mb += 4.0
            if g_tcp_cwnd_mb > BDP_MB:
                g_tcp_cwnd_mb = BDP_MB
            tcp_behavior = "Tăng cwnd dần"
        
        g_tcp_throughput = (g_tcp_cwnd_mb * 8.0) / RTT_SECONDS
        if g_tcp_throughput > BANDWIDTH_MBPS:
            g_tcp_throughput = BANDWIDTH_MBPS

        # 3. UDP và TCP BBR
        g_udp_throughput = BANDWIDTH_MBPS * (1.0 - (PACKET_LOSS if packet_lost else 0.005))
        g_bbr_throughput = BANDWIDTH_MBPS * (0.98 if packet_lost else 0.99)

        # Tính % hiệu suất
        g_tcp_eff = (g_tcp_throughput / BANDWIDTH_MBPS) * 100.0
        g_udp_eff = (g_udp_throughput / BANDWIDTH_MBPS) * 100.0
        g_bbr_eff = (g_bbr_throughput / BANDWIDTH_MBPS) * 100.0

        # Lưu dữ liệu vào danh sách
        time_data.append(g_currentTime)
        tcp_tp_data.append(g_tcp_throughput)
        udp_tp_data.append(g_udp_throughput)
        bbr_tp_data.append(g_bbr_throughput)
        tcp_eff_data.append(g_tcp_eff)
        udp_eff_data.append(g_udp_eff)
        bbr_eff_data.append(g_bbr_eff)
        tcp_cwnd_data.append(g_tcp_cwnd_mb)
        loss_status_data.append(1 if packet_lost else 0)

        # Ghi file CSV
        csv_writer.writerow([
            f"{g_currentTime:.1f}", RTT_MS, BANDWIDTH_MBPS,
            ("Co" if packet_lost else "Khong"),
            f"{g_tcp_throughput:.2f}", f"{g_udp_throughput:.2f}",
            f"{g_bbr_throughput:.2f}", f"{g_tcp_cwnd_mb:.2f}"
        ])
        csv_file.flush()

        # In Terminal theo định dạng gọn gàng
        print("==========================================================")
        print(f"THỜI GIAN: {g_currentTime:.1f} giây")
        print("==========================================================")
        print("THÔNG SỐ ĐƯỜNG TRUYỀN")
        print(f"  Dung lượng file     : {FILE_SIZE_GB:.0f} GB")
        print(f"  Bandwidth tối đa    : {BANDWIDTH_MBPS:.0f} Mbps (1 Gbps)")
        print(f"  RTT                 : {RTT_MS:.0f} ms")
        print(f"  BDP                 : {BDP_MB:.2f} MB")
        print(f"  Nhiễu / mất gói     : {'CÓ' if packet_lost else 'KHÔNG'}")
        print("----------------------------------------------------------")
        print("TCP RENO")
        print(f"  cwnd                : {g_tcp_cwnd_mb:.2f} MB")
        print(f"  Thông lượng         : {g_tcp_throughput:.2f} Mbps")
        print(f"  Hiệu suất           : {g_tcp_eff:.2f}%")
        print(f"  Phản ứng            : {tcp_behavior}")
        print("----------------------------------------------------------")
        print("UDP / UDT-LIKE")
        print(f"  Thông lượng         : {g_udp_throughput:.2f} Mbps")
        print(f"  Hiệu suất           : {g_udp_eff:.2f}%")
        print(f"  Phản ứng            : Duy trì tốc độ truyền cao")
        print("----------------------------------------------------------")
        print("TCP BBR-LIKE")
        print(f"  Thông lượng         : {g_bbr_throughput:.2f} Mbps")
        print(f"  Hiệu suất           : {g_bbr_eff:.2f}%")
        print(f"  Phản ứng            : Duy trì theo bandwidth/RTT")
        print("----------------------------------------------------------")
        print(f"Đã lưu Excel          : {csv_filename}")
        print("==========================================================")
        print("\n")

        # Cập nhật biểu đồ Realtime
        max_x = 5.0 if g_currentTime < 5.0 else g_currentTime

        # Ô 1: So sánh thông lượng
        ax1.clear()
        ax1.plot(time_data, tcp_tp_data, label='TCP Reno', color='#1f77b4', linewidth=1.5)
        ax1.plot(time_data, udp_tp_data, label='UDP/UDT-like', color='#2ca02c', linewidth=1.5)
        ax1.plot(time_data, bbr_tp_data, label='TCP BBR-like', color='#ff7f0e', linewidth=1.5)
        ax1.axhline(y=1000, color='gray', linestyle='--', label='Bandwidth max')
        ax1.set_title('So sánh thông lượng', fontsize=9)
        ax1.set_xlabel('Thời gian (s)')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.set_xlim(0, max_x)
        ax1.set_ylim(0, 1100)
        ax1.grid(True)
        ax1.legend(loc='lower left', fontsize=8)

        # Ô 2: Hiệu suất sử dụng đường truyền
        ax2.clear()
        ax2.plot(time_data, tcp_eff_data, label='TCP Reno', color='#1f77b4', linewidth=1.5)
        ax2.plot(time_data, udp_eff_data, label='UDP/UDT-like', color='#2ca02c', linewidth=1.5)
        ax2.plot(time_data, bbr_eff_data, label='TCP BBR-like', color='#ff7f0e', linewidth=1.5)
        ax2.axhline(y=100, color='gray', linestyle='--', label='100%')
        ax2.set_title('Hiệu suất sử dụng đường truyền 1 Gbps', fontsize=9)
        ax2.set_xlabel('Thời gian (s)')
        ax2.set_ylabel('Hiệu suất (%)')
        ax2.set_xlim(0, max_x)
        ax2.set_ylim(0, 110)
        ax2.grid(True)

        # Ô 3: TCP Reno - Cửa sổ truyền và BDP
        ax3.clear()
        ax3.plot(time_data, tcp_cwnd_data, label='TCP Reno cwnd', color='#1f77b4', linewidth=1.5)
        ax3.axhline(y=BDP_MB, color='gray', linestyle='--', label=f'BDP = {BDP_MB:.1f}MB')
        ax3.set_title('TCP Reno - Cửa sổ truyền và BDP', fontsize=9)
        ax3.set_xlabel('Thời gian (s)')
        ax3.set_ylabel('Cửa sổ truyền (MB)')
        ax3.set_xlim(0, max_x)
        ax3.set_ylim(0, 85)
        ax3.grid(True)
        ax3.legend(loc='lower left', fontsize=8)

        # Ô 4: Nhiễu / mất gói 2%
        ax4.clear()
        ax4.plot(time_data, loss_status_data, marker='o', markersize=3, linestyle='-', color='#1f77b4', label='Sự kiện mất gói')
        ax4.set_title('Nhiễu / mất gói 2%', fontsize=9)
        ax4.set_xlabel('Thời gian (s)')
        ax4.set_ylabel('Trạng thái')
        ax4.set_xlim(0, max_x)
        ax4.set_ylim(-0.2, 1.2)
        ax4.set_yticks([0, 1])
        ax4.set_yticklabels(['Không', 'Có'])
        ax4.grid(True)

        plt.tight_layout()
        plt.draw()
        plt.pause(0.001)

        # Độ trễ 1.0 giây để chạy chậm rãi
        time.sleep(0.5)

    csv_file.close()
    print("\nMo phong Case 2 hoan tat! Dang mo file Excel ket qua...")
    
    # Tự động mở file kết quả bằng phần mềm bảng tính (LibreOffice Calc trên Ubuntu)
    if os.name == 'posix':
        os.system("localc case_study2.csv &")
    elif os.name == 'nt':
        os.system("start case_study2.csv")

    plt.ioff()
    plt.show()

if __name__ == "__main__":
    main()
