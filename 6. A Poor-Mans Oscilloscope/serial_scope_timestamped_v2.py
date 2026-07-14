import serial
import collections
from datetime import datetime
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.dates as mdates
import matplotlib.ticker as mticker

# Configuration
PORT = 'COM9'
BAUDRATE = 9600
NUM_POINTS = 200          # Raise this to widen the visible time span

# Parallel buffers: time on x, value on y
times = collections.deque(maxlen=NUM_POINTS)
values = collections.deque(maxlen=NUM_POINTS)

# Plot setup
fig, ax = plt.subplots()
line, = ax.plot([], [], color='g')
ax.set_ylim(0, 255)
ax.set_title("Real-Time Serial Oscilloscope")
ax.set_facecolor('black')
fig.patch.set_facecolor('black')
ax.tick_params(colors='white')
ax.xaxis.grid(True, color='gray', linestyle='--')
ax.yaxis.grid(True, color='gray', linestyle='--')


def fmt_time(x, pos):
    dt = mdates.num2date(x)
    return dt.strftime('%H:%M:%S.') + f"{dt.microsecond // 1000:03d}"


ax.xaxis.set_major_formatter(mticker.FuncFormatter(fmt_time))
for lbl in ax.get_xticklabels():
    lbl.set_rotation(30)
    lbl.set_horizontalalignment('right')

# Serial port
serialPort = serial.Serial(PORT, BAUDRATE, timeout=1)
serialPort.flush()


def update(frame):
    while serialPort.in_waiting > 0:
        val = ord(serialPort.read(1))
        if val not in (13, 10):
            times.append(mdates.date2num(datetime.now()))  # Host read time
            values.append(val)
    if times:
        line.set_data(times, values)
        if len(times) > 1:
            ax.set_xlim(times[0], times[-1])
    return line,


# blit=False: x-limits and tick labels change each frame
ani = animation.FuncAnimation(fig, update, interval=10, blit=False, cache_frame_data=False)

try:
    plt.show()
finally:
    serialPort.close()
