import random
import time
import subprocess
import os
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque

# ============================================================
# CẤU HÌNH CASE STUDY 1
# ============================================================
BANDWIDTH_MBPS = 100.0  # Băng thông đường truyền 100 Mbps
RTT_MS = 50.0           # RTT = 50 ms
RTT_SECONDS = RTT_MS / 1000.0
PACKET_LOSS = 0.02      # Tỷ lệ rớt gói 2%

# Biến quản lý TCP (Netflix / VoD)
g_tcp_cwnd_mb = 1.0
g_tcp_latency_ms = RTT_MS

# Biến quản lý UDP (Zoom / Video Conference)
g_udp_latency_ms = RTT_MS

g_current_time = 0.0
MAX_POINTS = 40

# Khởi tạo file CSV
CSV_FILE = "ket_qua_Case1_Video.csv"
csv_file_handle = open(CSV_FILE, "w", encoding="utf-8")
csv_file_handle.write("Thoi gian,RTT,Bandwidth,Mat goi,TCP Throughput,TCP Latency (ms),UDP Throughput,UDP Latency (ms)\n")

# Dữ liệu phục vụ vẽ biểu đồ realtime
times = deque(maxlen=MAX_POINTS)
tcp_throughput_data = deque(maxlen=MAX_POINTS)
udp_throughput_data = deque(maxlen=MAX_POINTS)
tcp_latency_data = deque(maxlen=MAX_POINTS)
udp_latency_data = deque(maxlen=MAX_POINTS)

# ============================================================
# HÀM MÔ PHỎNG TỪNG BƯỚC (STEP)
# ============================================================
def simulate_step(frame):
    global g_current_time, g_tcp_cwnd_mb, g_tcp_latency_ms, g_udp_latency_ms
    
    g_current_time += 1.0

    # 1. Kiểm tra rớt gói ngẫu nhiên 2%
    packet_lost = (random.random() < PACKET_LOSS)

    # 2. Mô phỏng TCP Reno (Phản ứng với rớt gói & Head-of-Line Blocking)
    if packet_lost:
        g_tcp_cwnd_mb = g_tcp_cwnd_mb / 2.0
        if g_tcp_cwnd_mb < 0.5:
            g_tcp_cwnd_mb = 0.5
        
        # Độ trễ TCP tăng vọt vượt 150ms do Retransmission (HoL Blocking)
        g_tcp_latency_ms = 180.0 + random.uniform(50.0, 120.0)
    else:
        g_tcp_cwnd_mb += 0.35
        if g_tcp_cwnd_mb > 5.0:
            g_tcp_cwnd_mb = 5.0
        
        g_tcp_latency_ms = RTT_MS + random.uniform(0.0, 8.0)

    # Tính thông lượng TCP thực tế (Mbps)
    tcp_throughput = (g_tcp_cwnd_mb * 8.0) / RTT_SECONDS
    if tcp_throughput > BANDWIDTH_MBPS:
        tcp_throughput = BANDWIDTH_MBPS

    # 3. Mô phỏng UDP (Chấp nhận mất gói 2%, duy trì tốc độ và độ trễ cực thấp)
    udp_throughput = BANDWIDTH_MBPS * (1.0 - PACKET_LOSS)  # 98 Mbps
    g_udp_latency_ms = RTT_MS + random.uniform(-2.0, 3.0)   # Ổn định ~50ms

    loss_str = "Co" if packet_lost else "Khong"

    # Ghi dữ liệu ra file CSV
    csv_file_handle.write(
        f"{g_current_time:.1f},{RTT_MS},{BANDWIDTH_MBPS},{loss_str},"
        f"{tcp_throughput:.2f},{g_tcp_latency_ms:.1f},{udp_throughput:.2f},{g_udp_latency_ms:.1f}\n"
    )
    csv_file_handle.flush()

    # In tiến trình ra Terminal
    print(
        f"-> Time: {int(g_current_time)}s | "
        f"TCP (Netflix): {tcp_throughput:.1f} Mbps (Latency: {int(g_tcp_latency_ms)}ms) | "
        f"UDP (Zoom): {udp_throughput:.1f} Mbps (Latency: {int(g_udp_latency_ms)}ms)"
    )

    # Cập nhật dữ liệu cho biểu đồ
    times.append(g_current_time)
    tcp_throughput_data.append(tcp_throughput)
    udp_throughput_data.append(udp_throughput)
    tcp_latency_data.append(g_tcp_latency_ms)
    udp_latency_data.append(g_udp_latency_ms)

    # Cập nhật đường vẽ trên 4 ô đồ họa
    line_tp_tcp.set_data(list(times), list(tcp_throughput_data))
    line_tp_udp.set_data(list(times), list(udp_throughput_data))

    line_lat_tcp.set_data(list(times), list(tcp_latency_data))
    line_lat_udp.set_data(list(times), list(udp_latency_data))

    line_conf_udp.set_data(list(times), list(udp_latency_data))
    line_netflix_tcp.set_data(list(times), list(tcp_throughput_data))

    # Điều chỉnh trục X trượt theo thời gian thực
    xmin = 0 if g_current_time <= 5 else g_current_time - 20
    xmax = g_current_time + 2 if g_current_time <= 5 else g_current_time + 2

    for ax in [ax1, ax2, ax3, ax4]:
        ax.set_xlim(xmin, xmax)

    # Dừng mô phỏng khi đạt 100 giây
    if g_current_time >= 100.0:
        ani.event_source.stop()
        csv_file_handle.close()
        print("\nMo phong hoan tat! Dang mo file Excel ket qua...")
        try:
            subprocess.Popen(["localc", CSV_FILE])
        except Exception:
            pass

    return line_tp_tcp, line_tp_udp, line_lat_tcp, line_lat_udp, line_conf_udp, line_netflix_tcp


# ============================================================
# THIẾT KẾ GIAO DIỆN BIỂU ĐỒ (2x2 SUBPLOTS)
# ============================================================
fig, axes = plt.subplots(2, 2, figsize=(12, 8))
fig.suptitle("CASE 1: TCP VS UDP VIDEO STREAMING REALTIME (PYTHON)", fontsize=13, fontweight='bold')

ax1, ax2, ax3, ax4 = axes[0, 0], axes[0, 1], axes[1, 0], axes[1, 1]

# Ô 1: Thông lượng
ax1.set_title("1. Thong luong TCP vs UDP (Mbps)")
ax1.set_xlabel("Thoi gian (s)")
ax1.set_ylabel("Mbps")
ax1.set_ylim(0, 120)
ax1.grid(True)
line_tp_tcp, = ax1.plot([], [], lw=2, color='#1E90FF', label='TCP Throughput (VoD)')
line_tp_udp, = ax1.plot([], [], lw=2, color='#228B22', label='UDP Throughput (Zoom)')
ax1.axhline(y=100, color='red', linestyle='--', label='Max 100 Mbps')
ax1.legend(loc='upper left', fontsize=8)

# Ô 2: Độ trễ Latency
ax2.set_title("2. Do tre Latency / Jitter (ms)")
ax2.set_xlabel("Thoi gian (s)")
ax2.set_ylabel("ms")
ax2.set_ylim(0, 350)
ax2.grid(True)
line_lat_tcp, = ax2.plot([], [], lw=2, color='#DC143C', label='TCP Latency (HoL Blocking)')
line_lat_udp, = ax2.plot([], [], lw=2, color='#228B22', label='UDP Latency (~50ms)')
ax2.legend(loc='upper left', fontsize=8)

# Ô 3: Ngưỡng trễ Video Conference
ax3.set_title("3. Nguong Do tre Video Conference (<150ms)")
ax3.set_xlabel("Thoi gian (s)")
ax3.set_ylabel("ms")
ax3.set_ylim(0, 350)
ax3.grid(True)
line_conf_udp, = ax3.plot([], [], lw=2, color='#228B22', label='UDP Latency (Dat yeu cau)')
ax3.axhline(y=150, color='red', linestyle='--', label='Nguong Hoi thoai (150ms)')
ax3.legend(loc='upper left', fontsize=8)

# Ô 4: Netflix Buffer
ax4.set_title("4. Hieu suat Xem phim Netflix (TCP Buffer)")
ax4.set_xlabel("Thoi gian (s)")
ax4.set_ylabel("Mbps")
ax4.set_ylim(0, 120)
ax4.grid(True)
line_netflix_tcp, = ax4.plot([], [], lw=2, color='#1E90FF', label='TCP Throughput (Dao dong do loss)')
ax4.legend(loc='upper left', fontsize=8)

plt.tight_layout(rect=[0, 0, 1, 0.93])

print("==========================================================")
print("  BAT DAU MO PHONG CASE STUDY 1: VIDEO STREAMING (Python)")
print("==========================================================")

# Chạy animation mô phỏng (interval = 250ms tương ứng usleep trong C++)
ani = FuncAnimation(fig, simulate_step, interval=500, cache_frame_data=False)

plt.show()
