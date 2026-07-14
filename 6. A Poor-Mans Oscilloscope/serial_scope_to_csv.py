import serial
import collections
import csv
from datetime import datetime
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# Configuration
PORT = 'COM9'
BAUDRATE = 9600
NUM_POINTS = 200          # Points shown on screen at once
LOGFILE = 'ldr_log.csv'

# Data buffer for plotting
data_buffer = collections.deque([0] * NUM_POINTS, maxlen=NUM_POINTS)

# Plot setup
fig, ax = plt.subplots()
line, = ax.plot(data_buffer, color='g')
ax.set_ylim(0, 255)
ax.set_title("Real-Time Serial Oscilloscope")
ax.set_facecolor('black')
fig.patch.set_facecolor('black')
ax.tick_params(colors='white')
ax.xaxis.grid(True, color='gray', linestyle='--')
ax.yaxis.grid(True, color='gray', linestyle='--')

# Serial port
serialPort = serial.Serial(PORT, BAUDRATE, timeout=1)
serialPort.flush()

# CSV log: header + one row per reading
logfile = open(LOGFILE, 'w', newline='')
writer = csv.writer(logfile)
writer.writerow(['timestamp', 'value'])


def update(frame):
    while serialPort.in_waiting > 0:
        val = ord(serialPort.read(1))
        if val not in (13, 10):                  # Filter \r and \n
            ts = datetime.now()                  # Host read time, not ADC sample time
            data_buffer.append(val)
            writer.writerow([ts.isoformat(timespec='microseconds'), val])
    logfile.flush()                              # Persist even if interrupted
    line.set_ydata(data_buffer)
    return line,


ani = animation.FuncAnimation(fig, update, interval=10, blit=True, cache_frame_data=False)

try:
    plt.show()
finally:
    serialPort.close()
    logfile.close()
