import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

CSV_FILE = "exp2.csv"
THREADS = 12
WORKLOADS = ["lookup", "insert", "mixed"]
IMPLS = ["baseline", "fine"]
MARKERS = {"baseline": "o", "fine": "s"}
OUT_DIR = "figs_group2"

os.makedirs(OUT_DIR, exist_ok=True)

df = pd.read_csv(CSV_FILE)

df = df[df["threads"] == THREADS]

for wl in WORKLOADS:
    fig, ax = plt.subplots(figsize=(6, 4))

    for impl in IMPLS:
        sub = df[(df["workload"] == wl) & (df["impl"] == impl)]

        g = sub.groupby("nkeys")["ops_per_sec"]
        nkeys = g.mean().index.values
        mean_ops = g.mean().values
        std_ops = g.std().values

        ax.errorbar(
            nkeys,
            mean_ops,
            yerr=std_ops,
            marker=MARKERS[impl],
            linewidth=2,
            capsize=4,
            label=impl,
        )

    ax.set_xscale("log")
    ax.set_xticks(nkeys)
    ax.set_xticklabels([r"$10^4$", r"$10^5$", r"$10^6$"])

    ax.set_xlabel("Number of keys (nkeys)")
    ax.set_ylabel("Throughput (ops/s)")
    ax.set_title(f"Dataset Size Sensitivity | workload={wl}, threads=12")
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)

    plt.tight_layout()

    out_path = os.path.join(OUT_DIR, f"group2_nkeys_{wl}_t12.png")
    plt.savefig(out_path, dpi=300)
    plt.close()

    print(f"Saved: {out_path}")
