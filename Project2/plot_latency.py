import sys, re, math
import matplotlib.pyplot as plt

if len(sys.argv) < 2:
    print("Usage: python3 plot_latency.py bench_result.txt")
    sys.exit(1)

path = sys.argv[1]
pat = re.compile(r"size\s*=\s*([0-9]+)\s*([KMG]?)(?:B)?\b.*?avg\s*=\s*([0-9.]+)\s*ns", re.I)

def to_bytes(val, unit):
    val = int(val)
    u = unit.upper() if unit else ""
    if u == "K": return val << 10
    if u == "M": return val << 20
    if u == "G": return val << 30
    return val 

sizes_b, lat_ns = [], []
with open(path, "r", encoding="utf-8", errors="ignore") as f:
    for line in f:
        m = pat.search(line)
        if m:
            b = to_bytes(m.group(1), m.group(2))
            ns = float(m.group(3))
            sizes_b.append(b); lat_ns.append(ns)

# sort by size
pairs = sorted(zip(sizes_b, lat_ns))
if not pairs:
    print("No data parsed. Ensure lines look like: size=32KB ... avg=1.23 ns")
    sys.exit(1)
sizes_b, lat_ns = zip(*pairs)

def human(n):
    if n >= 1<<30: return f"{n//(1<<30)}G"
    if n >= 1<<20: return f"{n//(1<<20)}M"
    if n >= 1<<10: return f"{n//(1<<10)}K"
    return str(n)

plt.figure(figsize=(7.5,4.5))
plt.plot([math.log2(x) for x in sizes_b], lat_ns, marker="o")
plt.xlabel("Working-set size (bytes, log₂ scale)")
plt.ylabel("Average access latency (ns)")
plt.title("Latency vs Working-set Size (stride = 64B)")

xticks = [math.log2(x) for x in sizes_b]
plt.xticks(xticks, [human(x) for x in sizes_b], rotation=0)

L1, L2, L3 = 32<<10, 1<<20, 32<<20
for ref, name in [(L1,"L1 (32KB)"), (L2,"L2 (1MB)"), (L3,"L3 (32MB)")]:
    plt.axvline(math.log2(ref), linestyle="--")
    y = max(lat_ns)*0.95
    plt.text(math.log2(ref)+0.05, y, name, rotation=90, va="top")

plt.tight_layout()
plt.savefig("latency_vs_workingset.png", dpi=180)
print("Saved figure -> latency_vs_workingset.png")
