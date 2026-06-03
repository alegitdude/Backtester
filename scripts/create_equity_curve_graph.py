#!/usr/bin/env python3
import csv
from datetime import datetime
import matplotlib
matplotlib.use('Agg') 
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.ticker as mtick

#CSV_FILE_NAME = '../docs/sample_movAvg_report/equity_curve.csv'
CSV_FILE_NAME = '../reports/equity_curve.csv'
OUTPUT_IMAGE_NAME = '../reports/equity_curve_plot.png'

x_data = []
y_data = []
open_x, open_y = [], []
close_x, close_y = [], []
print(f"-> Opening '{CSV_FILE_NAME}'...")

try:
    with open(CSV_FILE_NAME, mode='r') as file:
        reader = csv.reader(file)
        next(reader)
        prev_qty = 0.0
        for row in reader:
            if not row:
                continue
                
            epoch_ns = float(row[0])
            epoch_seconds = epoch_ns / 1_000_000_000
            dt_object = datetime.fromtimestamp(epoch_seconds)
            
            y_value = float(row[1])
            
            current_qty = float(row[6])
            if prev_qty == 0.0 and current_qty > 0.0:
                # Opened a position: Log the point for a green marker
                open_x.append(dt_object)
                open_y.append(y_value)
                
            elif prev_qty > 0.0 and current_qty == 0.0:
                # Closed a position: Log the point for a red marker
                close_x.append(dt_object)
                close_y.append(y_value)
                
            prev_qty = current_qty
            x_data.append(dt_object)
            y_data.append(y_value)

    print(f"-> Successfully loaded {len(x_data)} data rows.")
    print("-> Configuring plot parameters...")

    plt.figure(figsize=(14, 7))

    plt.plot(x_data, y_data, linewidth=0.2, linestyle='-', color='blue', alpha=0.7, label='Price Track')

    if open_x:
        plt.scatter(open_x, open_y, color='green', marker='o', s=40, zorder=5, label='Open Position')

    if close_x:
        plt.scatter(close_x, close_y, color='red', marker='o', s=40, zorder=5, label='Close Position')

    plt.gca().yaxis.set_major_formatter(mtick.StrMethodFormatter('${x:,.2f}'))

    time_formatter = mdates.DateFormatter('%H:%M:%S')
    plt.gca().xaxis.set_major_formatter(time_formatter)
    plt.gca().xaxis.set_major_locator(mdates.HourLocator(interval=1))
    plt.gcf().autofmt_xdate()

    plt.xlabel('Time (HH:MM:SS) CST')
    plt.ylabel('Equity')
    plt.title('MovAvgCross Equity Curve 11/5')
    plt.grid(True, linestyle=':', alpha=0.6)
    plt.legend()
    plt.gca().yaxis.set_major_formatter(mtick.StrMethodFormatter('${x:,.2f}'))
    
    print(f"-> Rendering and saving to '{OUTPUT_IMAGE_NAME}'...")
    plt.savefig(OUTPUT_IMAGE_NAME, dpi=600, bbox_inches='tight')
    print("-> Success! Execution completed cleanly.")

except FileNotFoundError:
    print(f"\n[ERROR] Could not find the file '{CSV_FILE_NAME}'. Verify the filename match.")
except Exception as e:
    print(f"\n[ERROR] Script failed due to: {e}")
