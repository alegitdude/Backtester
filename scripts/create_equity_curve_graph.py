#!/usr/bin/env python3
import csv
from datetime import datetime
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.ticker as mtick

EQUITY_CSV = './reports/equity_curve.csv'
TRADE_CSV = './reports/trade_log.csv'   # optional
OUTPUT = './reports/equity_curve_plot.png'

def load_equity(path):
    rows = []
    with open(path, newline='') as f:
        reader = csv.DictReader(f)
        for r in reader:
            ts = int(r['timestamp'])
            rows.append({
                'ts': ts,
                'dt': datetime.fromtimestamp(ts / 1e9),
                'equity': float(r['equity']),
                'qty': float(r['open_position_qty']),
            })
    rows.sort(key=lambda r: r['ts'])
    return rows

def load_trades(path):
    trades = []
    try:
        with open(path, newline='') as f:
            reader = csv.DictReader(f)
            for r in reader:
                ts = int(r['timestamp'])
                trades.append({
                    'dt': datetime.fromtimestamp(ts / 1e9),
                    'side': r['side'].upper(),
                    'price': float(r['price']),
                })
        trades.sort(key=lambda t: t['dt'])
    except FileNotFoundError:
        pass
    return trades

equity = load_equity(EQUITY_CSV)
trades = load_trades(TRADE_CSV)

# Zoom: first time equity moves or qty != 0, through end
start_eq = equity[0]['equity']
i0 = 0
for i, r in enumerate(equity):
    if r['equity'] != start_eq or r['qty'] != 0:
        i0 = max(0, i - 2)
        break
equity = equity[i0:]

xs = [r['dt'] for r in equity]
ys = [r['equity'] for r in equity]
qtys = [r['qty'] for r in equity]

# Signed inventory transitions
open_long_x, open_long_y = [], []
open_short_x, open_short_y = [], []
flat_x, flat_y = [], []
prev = 0.0
for r in equity:
    q = r['qty']
    if prev == 0 and q > 0:
        open_long_x.append(r['dt']); open_long_y.append(r['equity'])
    elif prev == 0 and q < 0:
        open_short_x.append(r['dt']); open_short_y.append(r['equity'])
    elif prev != 0 and q == 0:
        flat_x.append(r['dt']); flat_y.append(r['equity'])
    prev = q

fig, ax = plt.subplots(figsize=(14, 7))
ax.plot(xs, ys, linewidth=0.8, color='steelblue', alpha=0.9, label='Equity')

if open_long_x:
    ax.scatter(open_long_x, open_long_y, c='green', s=2, zorder=5, label='Open long')
if open_short_x:
    ax.scatter(open_short_x, open_short_y, c='orange', s=2, zorder=5, label='Open short')
if flat_x:
    ax.scatter(flat_x, flat_y, c='red', s=2, zorder=5, label='Flat')

# Optional: mark fills from trade log (cleaner for MM)
if trades:
    # map trade times onto equity for y-values (nearest)
    # simple: use equity at end of series scale — skip if too heavy
    pass

ax.yaxis.set_major_formatter(mtick.StrMethodFormatter('${x:,.2f}'))
ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
ax.xaxis.set_major_locator(mdates.AutoDateLocator())
fig.autofmt_xdate()
ax.set_xlabel('Time (HH:MM:SS)')
ax.set_ylabel('Equity')
ax.set_title('Equity Curve')
ax.grid(True, linestyle=':', alpha=0.6)
ax.legend(loc='best')

# Secondary axis: inventory
# ax2 = ax.twinx()
# #ax2.plot(xs, qtys, color='gray', linewidth=0.6, alpha=0.5, label='Inventory')
# ax2.step(xs, qtys, where='post', color='gray', linewidth=0.8, alpha=0.7)
# ax2.set_ylim(-1.5, 1.5)
# ax2.set_ylabel('Inventory (contracts)')
# ax2.axhline(0, color='gray', linewidth=0.4, alpha=0.5)

fig.tight_layout()
fig.savefig(OUTPUT, dpi=200, bbox_inches='tight')
print(f'Saved {OUTPUT} ({len(equity)} points after zoom)')