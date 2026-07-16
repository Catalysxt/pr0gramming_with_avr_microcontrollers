#!/usr/bin/env python3
"""Plot temperature vs. real wall-clock time from a CSV made by dump_log.py.

Usage:
    python scripts/plot_log.py <input_csv> [output_png]

If output_png is omitted, the plot is shown interactively instead of saved.
"""
import csv
import sys
from datetime import datetime

import matplotlib.pyplot as plt

TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S"


def load_readings(path):
    timestamps = []
    temps = []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            timestamps.append(datetime.strptime(row["timestamp"], TIMESTAMP_FORMAT))
            temps.append(float(row["temp_c"]))
    return timestamps, temps


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <input_csv> [output_png]")
        sys.exit(1)

    in_path = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else None

    timestamps, temps = load_readings(in_path)
    if not timestamps:
        print(f"No readings found in {in_path}")
        sys.exit(1)

    fig, ax = plt.subplots()
    ax.plot(timestamps, temps, marker=".", linestyle="-")
    ax.set_title("Temperature over time")
    ax.set_xlabel("Date / Time")
    ax.set_ylabel("Temperature (degrees C)")
    fig.autofmt_xdate()
    fig.tight_layout()

    if out_path:
        fig.savefig(out_path)
        print(f"Saved plot to {out_path}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
