#!/usr/bin/env python3
import glob, json, re, statistics as stats
import matplotlib.pyplot as plt
from collections import defaultdict

# ---------- 1) Collect & parse ----------
files = sorted(glob.glob("NJ_*_run*.json"))
pat = re.compile(r"NJ_(\d+)_run(\d+)\.json")

by_nj = defaultdict(list)
qd_seen = set()

def safe_get(d, path, default=None):
    cur = d
    try:
        for k in path:
            cur = cur[k]
        return cur
    except Exception:
        return default

for f in files:
    m = pat.search(f)
    if not m:
        continue
    nj = int(m.group(1))

    with open(f, "r") as fp:
        data = json.load(fp)
    job = data["jobs"][0]

    # record fixed QD
    qd = int(job["job options"]["iodepth"])
    qd_seen.add(qd)

    # metrics (bytes/s, iops, p50 latency ns)
    bw_Bps = safe_get(job, ["read", "bw_bytes"], 0.0)
    iops   = safe_get(job, ["read", "iops"], 0.0)
    p50_ns = safe_get(job, ["read", "clat_ns", "percentile", "50.000000"], None)
    if p50_ns is None:
        p50_ns = safe_get(job, ["read", "clat_ns", "mean"], 0.0)

    by_nj[nj].append({"bw_Bps": bw_Bps, "iops": iops, "p50_ns": p50_ns})

if not by_nj:
    raise SystemExit("No matching files like NJ_<numjobs>_run<r>.json found.")

# ---------- 2) Aggregate per numjobs ----------
rows = []
for nj, L in by_nj.items():
    # means
    bw_mean   = stats.mean(x["bw_Bps"] for x in L)
    iops_mean = stats.mean(x["iops"]   for x in L)
    p50_mean  = stats.mean(x["p50_ns"] for x in L)

    # stddevs (population stdev if >=2 runs; else 0)
    bw_std   = stats.pstdev([x["bw_Bps"]  for x in L]) if len(L) > 1 else 0.0
    iops_std = stats.pstdev([x["iops"]    for x in L]) if len(L) > 1 else 0.0
    p50_std  = stats.pstdev([x["p50_ns"]  for x in L]) if len(L) > 1 else 0.0

    rows.append((nj, bw_mean, bw_std, iops_mean, iops_std, p50_mean, p50_std))

rows.sort(key=lambda x: x[0])

# unpack & unit conversion
njs        = [r[0] for r in rows]
bw_MBps    = [r[1] / 1e6 for r in rows]         # bytes/s → MB/s
bw_MBps_e  = [r[2] / 1e6 for r in rows]         # stddev (MB/s)
iops_mean  = [r[3]       for r in rows]
iops_e     = [r[4]       for r in rows]
p50_us     = [r[5] / 1e3 for r in rows]         # ns → µs
p50_us_e   = [r[6] / 1e3 for r in rows]         # stddev (µs)

qd_list = sorted(qd_seen)
qd_label = f"QD={qd_list[0]}" if len(qd_list) == 1 else f"QD={qd_list}"

plt.figure()
plt.errorbar(
    p50_us, bw_MBps,
    xerr=p50_us_e, yerr=bw_MBps_e,
    fmt='-o', capsize=3
)
for x, y, nj in zip(p50_us, bw_MBps, njs):
    plt.annotate(f"NJ={nj}", (x, y), textcoords="offset points", xytext=(6, 6), fontsize=8)

plt.xlabel("Latency (µs, p50)")
plt.ylabel("Bandwidth (MB/s)")
plt.title(f"4K randread, direct=1, {qd_label}")
plt.grid(True, linestyle="--", alpha=0.4)
plt.tight_layout()
plt.savefig("throughput_vs_latency.png", dpi=150)
plt.show()

plt.figure()
plt.errorbar(
    p50_us, iops_mean,
    xerr=p50_us_e, yerr=iops_e,
    fmt='-o', capsize=3
)
for x, y, nj in zip(p50_us, iops_mean, njs):
    plt.annotate(f"NJ={nj}", (x, y), textcoords="offset points", xytext=(6, 6), fontsize=8)

plt.xlabel("Latency (µs, p50)")
plt.ylabel("IOPS")
plt.title(f"4K randread, direct=1, {qd_label} (IOPS vs Latency)")
plt.grid(True, linestyle="--", alpha=0.4)
plt.tight_layout()
plt.savefig("iops_vs_latency.png", dpi=150)
plt.show()
