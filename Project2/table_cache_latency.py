import re
import pandas as pd
import matplotlib.pyplot as plt

pat = re.compile(r"size\s*=\s*([0-9]+)\s*([KMG]?)(?:B)?\b.*?avg\s*=\s*([0-9.]+)\s*ns", re.I)

def to_bytes(val, unit):
    val = int(val)
    u = unit.upper() if unit else ""
    if u == "K": return val << 10
    if u == "M": return val << 20
    if u == "G": return val << 30
    return val

sizes = {}
with open("bench_result.txt", "r", encoding="utf-8", errors="ignore") as f:
    for line in f:
        m = pat.search(line)
        if m:
            b = to_bytes(m.group(1), m.group(2))
            ns = float(m.group(3))
            sizes[b] = ns

targets = {
    "L1 (32KB)": 32 * 1024,
    "L2 (1MB)": 1 * 1024 * 1024,
    "L3 (32MB)": 32 * 1024 * 1024,
    "DRAM (>32MB)": 512 * 1024 * 1024  
}

rows = []
for name, sz in targets.items():
    if sz in sizes:
        ns = sizes[sz]
    else:
        ns = None
    cycles = None
    if ns:
        cycles = round(ns * 4.691) 
    rows.append([name, f"{sz//1024 if sz<1<<20 else sz//(1<<20)}{'K' if sz<1<<20 else 'M'}", ns, cycles])

df = pd.DataFrame(rows, columns=["Level", "Size", "Latency (ns)", "Latency (cycles)"])
print(df)

fig, ax = plt.subplots(figsize=(6,1.5))
ax.axis("off")
tbl = ax.table(cellText=df.values, colLabels=df.columns, cellLoc="center", loc="center")
tbl.auto_set_font_size(False)
tbl.set_fontsize(9)
tbl.scale(1.2, 1.2)

plt.savefig("cache_latency_table.png", dpi=180, bbox_inches="tight")
plt.show()
