import sys, json, glob, re, os
import matplotlib.pyplot as plt

def parse_bs_to_bytes(bs_str: str) -> int:
    s = bs_str.strip().lower()
    if s.endswith('k'):
        return int(s[:-1]) * 1024
    if s.endswith('m'):
        return int(s[:-1]) * 1024 * 1024
    return int(s)

def load_series(prefix: str, mode: str, root: str):
    """prefix in {'RandWrite','SeqWrite'}, mode='write' """
    rows = []
    for path in sorted(glob.glob(os.path.join(root, f"{prefix}_*.json"))):
        m = re.search(r"_([0-9]+[kKmM])\.json$", os.path.basename(path))
        if not m:
            continue
        bs_label = m.group(1).lower()
        bs_bytes = parse_bs_to_bytes(bs_label)
        with open(path, "r") as f:
            j = json.load(f)
        job = j["jobs"][0]
        side = job[mode]
        bw_bytes = float(side.get("bw_bytes", 0.0))
        iops     = float(side.get("iops", 0.0))
        if "clat_ns" in side and "mean" in side["clat_ns"]:
            lat_ns = float(side["clat_ns"]["mean"])
        else:
            lat_ns = float(job.get("lat_ns", {}).get("mean", 0.0))
        rows.append({
            "bs_bytes": bs_bytes,
            "bs_label": bs_label,
            "mbps": bw_bytes / 1e6,
            "iops": iops,
            "lat_us": lat_ns / 1000.0
        })
    rows.sort(key=lambda r: r["bs_bytes"])
    return rows

def format_xticks_kib_mib(ax):
    ticks = ax.get_xticks()
    labels = []
    for t in ticks:
        if t <= 0:
            labels.append("")
        elif t < 1024*1024:
            labels.append(f"{int(round(t/1024))} KiB")
        else:
            labels.append(f"{int(round(t/(1024*1024)))} MiB")
    ax.set_xticklabels(labels)

root = sys.argv[1] if len(sys.argv) > 1 else "."

randw = load_series("RandWrite", "write", root)
seqw  = load_series("SeqWrite",  "write", root)

if not randw and not seqw:
    raise SystemExit("No RandWrite_*.json or SeqWrite_*.json found.")

fig1, ax1 = plt.subplots(figsize=(8,5))

if randw:
    ax1.plot([r["bs_bytes"] for r in randw], [r["mbps"] for r in randw], "-o", label="Random Write (MB/s)")
if seqw:
    ax1.plot([r["bs_bytes"] for r in seqw],  [r["mbps"] for r in seqw],  "-s", label="Sequential Write (MB/s)")

ax1.set_xscale("log", base=2)
ax1.set_xlabel("Block size")
ax1.set_ylabel("Throughput (MB/s)")
ax1.grid(True, which="both", linestyle=":", linewidth=0.6)

for v in [64*1024, 128*1024]:
    ax1.axvline(v, color="gray", linestyle=":", linewidth=1)

ax1b = ax1.twinx()
if randw:
    ax1b.plot([r["bs_bytes"] for r in randw], [r["iops"] for r in randw], "--o", alpha=0.8, label="Random Write (IOPS)")
if seqw:
    ax1b.plot([r["bs_bytes"] for r in seqw],  [r["iops"] for r in seqw],  "--s", alpha=0.8, label="Sequential Write (IOPS)")
ax1b.set_ylabel("IOPS")

lines, labels = ax1.get_legend_handles_labels()
lines2, labels2 = ax1b.get_legend_handles_labels()
ax1.legend(lines+lines2, labels+labels2, loc="best", fontsize=9)

format_xticks_kib_mib(ax1)
fig1.tight_layout()
fig1.savefig("write_throughput.png", dpi=200)

fig2, ax2 = plt.subplots(figsize=(8,5))
if randw:
    ax2.plot([r["bs_bytes"] for r in randw], [r["lat_us"] for r in randw], "-o", label="Random Write")
if seqw:
    ax2.plot([r["bs_bytes"] for r in seqw],  [r["lat_us"] for r in seqw],  "-s", label="Sequential Write")

ax2.set_xscale("log", base=2)
ax2.set_xlabel("Block size")
ax2.set_ylabel("Avg Latency (μs)")
ax2.grid(True, which="both", linestyle=":", linewidth=0.6)
ax2.legend(loc="best", fontsize=9)

format_xticks_kib_mib(ax2)
fig2.tight_layout()
fig2.savefig("write_latency.png", dpi=200)

print("Saved: write_throughput.png, write_latency.png")
