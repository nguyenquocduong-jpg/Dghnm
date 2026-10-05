import random
import time
import subprocess
import os
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque

# ============================================================
# CẤU HÌNH CASE STUDY 3: HTTP/2 (TCP) vs HTTP/3 (QUIC/UDP)
# ============================================================
BANDWIDTH_MBPS = 100.0  # Băng thông tối đa 100 Mbps
RTT_MS = 50.0           # RTT cơ sở = 50 ms
RTT_SECONDS = RTT_MS / 1000.0
PACKET_LOSS = 0.02      # Tỷ lệ rớt gói 2%

# Biến trạng thái HTTP/2 (TCP - Bị ảnh hưởng HoL Blocking)
g_tcp_cwnd_mb = 2.0
g_tcp_latency_ms = RTT_MS

# Biến trạng thái HTTP/3 (QUIC/UDP - Đa luồng độc lập)
g_quic_latency_ms = RTT_MS

g_current_time = 0.0
MAX_POINTS = 50

# Khởi tạo file CSV ghi log kết quả
CSV_FILE = "ket_qua_Case3_HTTP3_QUIC.csv"
csv_file_handle = open(CSV_FILE, "w", encoding="utf-8")
csv_file_handle.write("Thoi gian,RTT,Bandwidth,Mat goi,HTTP2_Throughput,HTTP2_Latency(ms),HTTP3_Throughput,HTTP3_Latency(ms)\n")

# Buffer lưu trữ dữ liệu đồ thị
times = deque(maxlen=MAX_POINTS)
http2_tp_data = deque(maxlen=MAX_POINTS)
http3_tp_data = deque(maxlen=MAX_POINTS)
http2_lat_data = deque(maxlen=MAX_POINTS)
http3_lat_data = deque(maxlen=MAX_POINTS)

# ============================================================
# HÀM MÔ PHỎNG TỪNG BƯỚC - DỪNG VÀ MỞ FILE EXCEL Ở 100S
# ============================================================
def simulate_step(frame):
    global g_current_time, g_tcp_cwnd_mb, g_tcp_latency_ms, g_quic_latency_ms
    
    g_current_time += 1.0

    # 1. Kiểm tra sự kiện mất gói ngẫu nhiên trên đường truyền (2%)
    packet_lost = (random.random() < PACKET_LOSS)

    # 2. Mô phỏng HTTP/2 qua TCP (Giảm cwnd & tăng vọt độ trễ khi mất gói)
    if packet_lost:
        g_tcp_cwnd_mb = max(0.8, g_tcp_cwnd_mb / 2.0)
        g_tcp_latency_ms = 190.0 + random.uniform(40.0, 90.0)  # HoL Blocking
    else:
        g_tcp_cwnd_mb = min(6.0, g_tcp_cwnd_mb + 0.3)
        g_tcp_latency_ms = RTT_MS + random.uniform(0.0, 6.0)

    http2_throughput = (g_tcp_cwnd_mb * 8.0) / RTT_SECONDS
    if http2_throughput > BANDWIDTH_MBPS:
        http2_throughput = BANDWIDTH_MBPS

    # 3. Mô phỏng HTTP/3 qua QUIC/UDP (Xử lý độc lập từng luồng)
    http3_throughput = BANDWIDTH_MBPS * (1.0 - (PACKET_LOSS * 0.3))  # ~99.4 Mbps
    if packet_lost:
        g_quic_latency_ms = RTT_MS + random.uniform(2.0, 15.0)
    else:
        g_quic_latency_ms = RTT_MS + random.uniform(-1.0, 3.0)

    loss_str = "Co" if packet_lost else "Khong"

    # Ghi dữ liệu ra tệp CSV
    csv_file_handle.write(
        f"{g_current_time:.1f},{RTT_MS},{BANDWIDTH_MBPS},{loss_str},"
        f"{http2_throughput:.2f},{g_tcp_latency_ms:.1f},{http3_throughput:.2f},{g_quic_latency_ms:.1f}\n"
    )
    csv_file_handle.flush()

    # Tính toán thông số xuất định dạng bảng chuẩn Terminal
    bdp_mb = (BANDWIDTH_MBPS * RTT_SECONDS) / 8.0
    http2_eff = (http2_throughput / BANDWIDTH_MBPS) * 100.0
    http3_eff = (http3_throughput / BANDWIDTH_MBPS) * 100.0
    
    tcp_action = "Tăng cwnd dần" if not packet_lost else "Giảm cwnd & HoL Blocking"
    quic_action = "Duy trì tốc độ truyền cao" if not packet_lost else "Phục hồi luồng độc lập"

    # In thông tin cập nhật liên tục ra Terminal
    print("\033[H\033[J", end="")  # Xóa màn hình Terminal
    print(f"BDP                    : {bdp_mb:.2f} MB")
    print(f"Nhiễu / mất gói        : {loss_str.upper()}")
    print("------------------------------------------------------------")
    print("HTTP/2 (TCP RENO / HoL Blocking)")
    print(f"  cwnd                 : {g_tcp_cwnd_mb:.2f} MB")
    print(f"  Thông Lượng          : {http2_throughput:.2f} Mbps")
    print(f"  Hiệu suất            : {http2_eff:.2f}%")
    print(f"  Phản Ứng             : {tcp_action}")
    print("------------------------------------------------------------")
    print("HTTP/3 (QUIC / UDP - Multi-stream)")
    print(f"  Thông Lượng          : {http3_throughput:.2f} Mbps")
    print(f"  Hiệu suất            : {http3_eff:.2f}%")
    print(f"  Phản Ứng             : {quic_action}")
    print("------------------------------------------------------------")
    print(f"Đã lưu Excel           : {CSV_FILE}")
    print("============================================================\n")

    # ĐẠT 100 GIÂY: DỪNG MÔ PHỎNG VÀ TỰ ĐỘNG MỞ FILE EXCEL/CSV
    if g_current_time >= 100.0:
        ani.event_source.stop()
        csv_file_handle.close()
        print("Mo phong Case Study 3 hoan tat (Da du 100s)!")
        print(f"Dang tu dong mo file ket qua Excel/CSV: {CSV_FILE} ...")
        
        # Mở file CSV bằng phần mềm xem bảng tính mặc định trên Ubuntu (Không làm treo Terminal)
        try:
            subprocess.Popen(["xdg-open", CSV_FILE])
        except Exception as e:
            print(f"Khong the tu dong mo file: {e}")

    # Cập nhật buffer dữ liệu cho đồ thị
    times.append(g_current_time)
    http2_tp_data.append(http2_throughput)
    http3_tp_data.append(http3_throughput)
    http2_lat_data.append(g_tcp_latency_ms)
    http3_lat_data.append(g_quic_latency_ms)

    # Cập nhật các đường vẽ trên đồ thị
    line_tp_h2.set_data(list(times), list(http2_tp_data))
    line_tp_h3.set_data(list(times), list(http3_tp_data))

    line_lat_h2.set_data(list(times), list(http2_lat_data))
    line_lat_h3.set_data(list(times), list(http3_lat_data))

    line_hol_h2.set_data(list(times), list(http2_lat_data))
    line_quic_stable.set_data(list(times), list(http3_lat_data))

    # Điều chỉnh trục X cuộn tự động theo thời gian
    xmin = 0 if g_current_time <= 5 else g_current_time - 25
    xmax = g_current_time + 2 if g_current_time <= 5 else g_current_time + 2

    for ax in [ax1, ax2, ax3, ax4]:
        ax.set_xlim(xmin, xmax)

    return line_tp_h2, line_tp_h3, line_lat_h2, line_lat_h3, line_hol_h2, line_quic_stable


# ============================================================
# THIẾT KẾ GIAO DIỆN 4 BIỂU ĐỒ (2x2 SUBPLOTS)
# ============================================================
fig, axes = plt.subplots(2, 2, figsize=(12, 8))
fig.suptitle("CASE STUDY 3: HTTP/2 (TCP) VS HTTP/3 (QUIC/UDP) PERFORMANCE REALTIME", fontsize=13, fontweight='bold')

ax1, ax2, ax3, ax4 = axes[0, 0], axes[0, 1], axes[1, 0], axes[1, 1]

# Ô 1: So sánh Thông lượng
ax1.set_title("1. Thong luong HTTP/2 (TCP) vs HTTP/3 (QUIC)")
ax1.set_xlabel("Thoi gian (s)")
ax1.set_ylabel("Mbps")
ax1.set_ylim(0, 120)
ax1.grid(True)
line_tp_h2, = ax1.plot([], [], lw=2, color='#D9534F', label='HTTP/2 Throughput (TCP Loss impact)')
line_tp_h3, = ax1.plot([], [], lw=2, color='#5CB85C', label='HTTP/3 Throughput (QUIC Multi-stream)')
ax1.axhline(y=100, color='blue', linestyle='--', label='Max Bandwidth (100 Mbps)')
ax1.legend(loc='lower left', fontsize=8)

# Ô 2: So sánh Độ trễ
ax2.set_title("2. Bien dong Do tre Latency (ms)")
ax2.set_xlabel("Thoi gian (s)")
ax2.set_ylabel("ms")
ax2.set_ylim(0, 350)
ax2.grid(True)
line_lat_h2, = ax2.plot([], [], lw=2, color='#D9534F', label='HTTP/2 Latency (HoL Blocking spikes)')
line_lat_h3, = ax2.plot([], [], lw=2, color='#5CB85C', label='HTTP/3 Latency (Stable QUIC)')
ax2.legend(loc='upper left', fontsize=8)

# Ô 3: Hiện tượng Head-of-Line Blocking ở HTTP/2
ax3.set_title("3. Anh huong Head-of-Line Blocking (HTTP/2)")
ax3.set_xlabel("Thoi gian (s)")
ax3.set_ylabel("ms (Latency)")
ax3.set_ylim(0, 350)
ax3.grid(True)
line_hol_h2, = ax3.plot([], [], lw=2, color='#D9534F', label='HTTP/2 Connection Stalling')
ax3.axhline(y=150, color='orange', linestyle='--', label='Nguong canh bao nghen (150ms)')
ax3.legend(loc='upper left', fontsize=8)

# Ô 4: Khả năng duy trì ổn định của HTTP/3 QUIC
ax4.set_title("4. Kha nang duy tri o dinh cua HTTP/3 (QUIC)")
ax4.set_xlabel("Thoi gian (s)")
ax4.set_ylabel("ms (Latency)")
ax4.set_ylim(0, 100)
ax4.grid(True)
line_quic_stable, = ax4.plot([], [], lw=2, color='#5CB85C', label='QUIC Stream Independence')
ax4.legend(loc='upper left', fontsize=8)

plt.tight_layout(rect=[0, 0, 1, 0.93])

# Chạy animation với chu kỳ 800ms
ani = FuncAnimation(fig, simulate_step, interval=800, cache_frame_data=False)

plt.show()