import sys
import matplotlib.pyplot as plt
from collections import defaultdict

paths = sys.argv[1:] or ["stride.csv"]

def parse_line(line: str):
    if "=" not in line:
        return None
    parts = [kv for kv in line.strip().split(",") if "=" in kv]
    d = dict(kv.split("=", 1) for kv in parts)
    try:
        return {
            "pattern": d["pattern"],
            "stride": int(d["stride"]),
            "avg_ns": float(d["avg_ns"]),
            "bw_MBps": float(d["bw_MBps"]),
        }
    except KeyError:
        return None

by_pat = defaultdict(list)
for p in paths:
    with open(p, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            row = parse_line(line)
            if row:
                by_pat[row["pattern"]].append(row)

if not by_pat:
    print("No rows parsed. Make sure lines look like: pattern=...,stride=...,avg_ns=...,bw_MBps=...")
    sys.exit(1)

def mbps_to_gibps(x): 
    return x / 1024.0

for pat, rows in by_pat.items():
    rows = sorted(rows, key=lambda r: r["stride"])
    strides = [r["stride"] for r in rows]
    lat_ns  = [r["avg_ns"] for r in rows]
    bw_gib  = [mbps_to_gibps(r["bw_MBps"]) for r in rows]

    fig, ax1 = plt.subplots(figsize=(8, 4.5))

    ax1.bar(range(len(strides)), bw_gib, width=0.6, label="Bandwidth (GiB/s)")
    ax1.set_ylabel("Bandwidth (GiB/s)")
    ax1.set_xlabel("Stride (Bytes)")
    ax1.set_xticks(range(len(strides)))
    ax1.set_xticklabels([str(s) for s in strides])

    ax2 = ax1.twinx()
    ax2.plot(range(len(strides)), lat_ns, marker="o", label="Latency (ns/access)")
    ax2.set_ylabel("Latency (ns/access)")

    plt.title(f"{pat}: Latency & Bandwidth vs Stride")
    lines, labels = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines + lines2, labels + labels2, loc="upper left")

    fig.tight_layout()
    outname = f"{pat}_lat_bw_vs_stride.png"
    plt.savefig(outname, dpi=180)
    print("Saved ->", outname)
