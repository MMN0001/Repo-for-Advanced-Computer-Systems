import sys, os, glob, json, re
import matplotlib.pyplot as plt

def read_ratio(job):
    opts = job.get("job options", {})
    rw   = opts.get("rw","").lower()
    if rw in ("randread","read"):
        return 100
    if rw in ("randwrite","write"):
        return 0
    if rw == "randrw":
        try:
            return int(opts.get("rwmixread"))
        except Exception:
            pass
    m = re.search(r"(\d+)\s*R", job.get("jobname",""), re.I)
    if m:
        return int(m.group(1))
    r_iops = job.get("read", {}).get("iops", 0.0)
    w_iops = job.get("write", {}).get("iops", 0.0)
    if r_iops > 0 and w_iops == 0: return 100
    if w_iops > 0 and r_iops == 0: return 0
    return -1

def mean_lat_us(side, job):
    if "clat_ns" in side and "mean" in side["clat_ns"]:
        return float(side["clat_ns"]["mean"]) / 1000.0
    return float(job.get("lat_ns",{}).get("mean",0.0)) / 1000.0

def load_mix(root):
    files = sorted(glob.glob(os.path.join(root, "Mix_*.json"))) \
          + sorted(glob.glob(os.path.join(root, "*randread*.json"))) \
          + sorted(glob.glob(os.path.join(root, "*randwrite*.json")))
    rows = []
    for fp in files:
        with open(fp, "r") as f:
            j = json.load(f)
        job = j["jobs"][0]
        pct = read_ratio(job)
        if pct < 0: 
            continue

        r = job["read"]; w = job["write"]
        r_ios = float(r.get("total_ios", 0.0))
        w_ios = float(w.get("total_ios", 0.0))
        total_ios = r_ios + w_ios

        r_lat = mean_lat_us(r, job) if r_ios>0 else None
        w_lat = mean_lat_us(w, job) if w_ios>0 else None

        if r_ios>0 and w_ios>0:
            avg_lat = (r_lat*r_ios + w_lat*w_ios) / total_ios
        elif r_ios>0:
            avg_lat = r_lat
        elif w_ios>0:
            avg_lat = w_lat
        else:
            avg_lat = None

        rows.append({
            "pct": pct,
            "avg_lat_us": avg_lat,
            "throughput_MBps": (float(r.get("bw_bytes",0.0)) + float(w.get("bw_bytes",0.0))) / 1e6,
            "bs": job.get("job options",{}).get("bs","4k"),
            "qd": job.get("job options",{}).get("iodepth","1"),
        })

    uniq = {}
    for r in rows:
        uniq.setdefault(r["pct"], r)
    return uniq

def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    data = load_mix(root)
    if not data:
        raise SystemExit("No mix results found.")

    order  = [100, 70, 50, 0]
    labels = {100:"R100", 70:"R70W30", 50:"R50W50", 0:"R0"}

    xs, lat, thr = [], [], []
    any_item = next(iter(data.values()))
    bs = any_item.get("bs","4k").upper()
    qd = any_item.get("qd","1")

    for p in order:
        if p in data:
            xs.append(labels[p])
            lat.append(data[p]["avg_lat_us"])
            thr.append(data[p]["throughput_MBps"])

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10,4), sharex=False)

    ax1.plot(xs, lat, "-o")
    ax1.set_title(f"{bs} Random, QD={qd}\nLatency vs Mix")
    ax1.set_ylabel("Latency (μs)")
    ax1.set_xlabel("Mix")
    ax1.grid(True, linestyle=":", linewidth=0.6)

    ax2.plot(xs, thr, "-o", color="orange")
    ax2.set_title(f"{bs} Random, QD={qd}\nThroughput vs Mix")
    ax2.set_ylabel("Throughput (MB/s)")
    ax2.set_xlabel("Mix")
    ax2.grid(True, linestyle=":", linewidth=0.6)

    fig.tight_layout()
    fig.savefig("mix_compact_side.png", dpi=200)
    print("Saved: mix_compact_side.png")

if __name__ == "__main__":
    main()
