import json, glob, os
import numpy as np
import pandas as pd

def extract_percentiles(job):
    clat = job["read"]["clat_ns"]["percentile"]
    return {
        "p50": clat.get("50.000000", 0) / 1000.0,
        "p95": clat.get("95.000000", 0) / 1000.0,
        "p99": clat.get("99.000000", 0) / 1000.0,
        "p99.9": clat.get("99.900000", 0) / 1000.0,
    }

def parse_qd(fname):
    base = os.path.basename(fname)
    if "NJ_8" in base:
        return 8
    if "NJ_16" in base:
        return 16
    return None

def main(root="."):
    files = sorted(glob.glob(os.path.join(root, "NJ_*.json")))
    results = {}
    for fp in files:
        with open(fp) as f:
            j = json.load(f)
        job = j["jobs"][0]
        qd = parse_qd(fp)
        pct = extract_percentiles(job)
        results.setdefault(qd, []).append(pct)

    summary = []
    for qd, lst in results.items():
        arr = {k: np.mean([d[k] for d in lst]) for k in lst[0]}
        summary.append({"job": qd, **arr})

    df = pd.DataFrame(summary).set_index("job").sort_index()
    print(df.round(2))
    df.to_csv("tail_latency_summary.csv")

if __name__ == "__main__":
    main()
