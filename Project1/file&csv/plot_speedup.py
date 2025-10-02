#!/usr/bin/env python3
import argparse, csv, os
import numpy as np
from collections import defaultdict
import matplotlib.pyplot as plt

# ---------- FLOPs per element ----------
def flops_per_elem(kernel: str) -> int:
    k = kernel.lower()
    if k in ("saxpy", "dot"):
        return 2   # mul + add
    elif k == "elemul":
        return 1   # mul
    return 0

# ---------- Bytes per element ----------
def bytes_per_elem(kernel: str, dtype: str, use_rfo: bool) -> int:
    size = 4 if dtype == "f32" else 8
    if kernel in ("saxpy", "elemul"):
        return (4*size) if use_rfo else (3*size)
    elif kernel == "dot":
        return 2*size
    else:
        return 3*size

# ---------- CSV loader ----------
def load(csv_path):
    groups = defaultdict(list)  # key=(kernel, build, dtype, M) -> [time_s]
    with open(csv_path, newline="") as f:
        rd = csv.DictReader(f)
        for r in rd:
            k = (r["kernel"].strip().lower(),
                 r["build"].strip().lower(),
                 r["dtype"].strip().lower(),
                 int(r["M"]))
            groups[k].append(float(r["time_s"]))
    return groups

# ---------- Cache markers ----------
def _add_one_marker(ax, xval, text, *, alpha=0.7):
    xmin, xmax = ax.get_xbound()
    ymin, ymax = ax.get_ybound()
    if xval < xmin or xval > xmax:
        return
    ytext = ymax - (ymax - ymin) * 0.05
    ax.axvline(xval, linestyle=':', linewidth=1.2, alpha=alpha, color="gray")
    ax.text(xval, ytext, text, rotation=90, va='top', ha='right', fontsize=9, alpha=alpha, color="gray")

def add_cache_markers_multi(ax, bpe_dict, l1d_kb, l2_kb, l3_mb):
    l1d = int(l1d_kb * 1024)
    l2  = int(l2_kb  * 1024)
    l3  = int(l3_mb  * 1024 * 1024)
    for kname, bpe in bpe_dict.items():
        if bpe <= 0: continue
        _add_one_marker(ax, l1d // bpe, f"L1d[{kname}]", alpha=0.5)
        _add_one_marker(ax, l2  // bpe, f"L2[{kname}]",  alpha=0.5)
        _add_one_marker(ax, l3  // bpe, f"L3[{kname}]",  alpha=0.5)

# ---------- Main ----------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True)
    ap.add_argument("--outdir", default="figs")
    ap.add_argument("--use-rfo", action="store_true", help="count RFO for stores")
    ap.add_argument("--l1d-kb", type=float, default=32.0)
    ap.add_argument("--l2-kb",  type=float, default=1024.0)
    ap.add_argument("--l3-mb",  type=float, default=32.0)
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    groups = load(args.csv)

    # data[(kernel,dtype)][M][build] = [..times..]
    data = defaultdict(lambda: defaultdict(lambda: {"scalar": [], "simd": []}))
    for (kernel, build, dtype, M), times in groups.items():
        data[(kernel, dtype)][M][build].extend(times)

    kernels = ["saxpy","elemul","dot"]
    dtypes  = sorted({dt for (_,dt) in {k for k in data}})

    # ----------- Speedup plot -----------
    fig, axes = plt.subplots(1, len(dtypes), figsize=(6.5*len(dtypes), 4.2), squeeze=False)
    for ci, dtype in enumerate(dtypes):
        ax = axes[0][ci]

        # bpe dict for cache markers
        bpe_dict = {k: bytes_per_elem(k, dtype, args.use_rfo) for k in kernels}

        for kernel in kernels:
            Ms, Sp_mean, Sp_err = [], [], []
            for M in sorted(data[(kernel,dtype)].keys()):
                t_s_list = data[(kernel,dtype)][M]["scalar"]
                t_v_list = data[(kernel,dtype)][M]["simd"]
                if not t_s_list or not t_v_list:
                    continue
                t_s = np.array(t_s_list)
                t_v = np.array(t_v_list)
                sp = t_s / t_v
                Ms.append(M)
                Sp_mean.append(np.mean(sp))
                Sp_err.append(np.std(sp))
            if Ms:
                ax.errorbar(Ms, Sp_mean, yerr=Sp_err, marker='o', capsize=4, label=f"{kernel}")

        add_cache_markers_multi(ax, bpe_dict=bpe_dict,
                                l1d_kb=args.l1d_kb, l2_kb=args.l2_kb, l3_mb=args.l3_mb)

        ax.set_xscale('log')
        ax.grid(True, which='both', linestyle='--', alpha=0.4)
        ax.set_xlabel("N (elements)")
        ax.set_ylabel("Speedup = scalar_time / simd_time")
        ax.set_title(f"Speedup vs N ({dtype})")
        ax.legend()

    fig.tight_layout()
    fig.savefig(f"{args.outdir}/speedup_all.png", dpi=150)
    print(f"[DONE] saved {args.outdir}/speedup_all.png")

if __name__ == "__main__":
    main()
