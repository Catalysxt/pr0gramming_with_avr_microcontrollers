#!/usr/bin/env python3
"""Dump the timestamped temperature log from logging_with_ds3231 over serial.

The firmware's `[p]` menu command prints one line per stored reading in the
form "YYYY-MM-DD HH:MM:SS, XX.X degrees". This script drives the menu,
captures those lines, and writes them out as CSV for scripts/plot_log.py.

Usage:
    python scripts/dump_log.py <serial_port> [output_csv]

Example:
    python scripts/dump_log.py COM5 docs/overnight_log.csv
"""
import csv
import re
import sys
import time

import serial  # pyserial: pip install pyserial

BAUD = 9600
LINE_PATTERN = re.compile(
    r"^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}), (\d+\.\d) degrees$"
)
IDLE_TIMEOUT_S = 2.0  # stop reading once no new line has arrived for this long


def capture_log(port, baud=BAUD):
    with serial.Serial(port, baud, timeout=1) as ser:
        input("Reset the AVR now, then press Enter here (must be within 5s of reset): ")
        ser.write(b"m")

        # Give the board a moment to print the menu before requesting the dump.
        time.sleep(0.5)
        ser.reset_input_buffer()
        ser.write(b"p")

        rows = []
        last_line_time = time.time()
        while time.time() - last_line_time < IDLE_TIMEOUT_S:
            raw = ser.readline()
            if not raw:
                continue
            last_line_time = time.time()
            line = raw.decode("ascii", errors="replace").strip()
            match = LINE_PATTERN.match(line)
            if match:
                rows.append((match.group(1), match.group(2)))

        return rows


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <serial_port> [output_csv]")
        sys.exit(1)

    port = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else "overnight_log.csv"

    rows = capture_log(port)

    if not rows:
        print("No log lines captured -- check the port, baud rate, and that [m] was pressed in time.")
        sys.exit(1)

    with open(out_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp", "temp_c"])
        writer.writerows(rows)

    print(f"Wrote {len(rows)} readings to {out_path}")


if __name__ == "__main__":
    main()
