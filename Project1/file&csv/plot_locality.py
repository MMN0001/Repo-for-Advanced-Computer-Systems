import argparse, csv, os
from collections import defaultdict
import numpy as np
import matplotlib.pyplot as plt  

def bytes_per_elem(kernel: str, dtype: str, use_rfo: bool) -> int:
    size = 4 if dtype == "f32" else 8
    if kernel in ("saxpy", "elemul"):
        return (4*size) if use_rfo else (3*size)
    elif kernel == "dot":
        return 2*size
    else:
        return 3*size

def flops_per_elem(kernel: str) -> int:
    if kernel in ("saxpy", "dot"):
        return 2  # mul + add
    elif kernel == "elemul":
        return 1  # mul
    return 0

def load_and_aggregate(csv_path):
    groups = defaultdict(list)  # key = (kernel, build, dtype, M)
    with open(csv_path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            kernel = row["kernel"].strip().lower()
            build  = row["build"].strip().lower()
            dtype  = row["dtype"].strip().lower()
            M      = int(row["M"])
            t      = float(row["time_s"])
            groups[(kernel, build, dtype, M)].append(t)
    return {k: float(np.median(ts)) for k, ts in groups.items()}

MARKERS = {
    ("saxpy", "scalar"): "o",   
    ("saxpy", "simd"):   "s",   
    ("elemul","scalar"): "D",   
    ("elemul","simd"):   "^",   
    ("dot",   "scalar"): "v",   
    ("dot",   "simd"):   "P",   
}
LINESTYLES = {"scalar": "--", "simd": "-"}

def plot_one(ax, xs, ys, label, *, marker="o", linestyle="-", alpha=0.9):
    ax.plot(xs, ys, label=label, marker=marker, linestyle=linestyle, alpha=alpha)  # 不指定颜色
    ax.set_xscale('log')
    ax.grid(True, which='both', linestyle='--', alpha=0.4)

def _add_one_marker(ax, xval, text, *, alpha=0.8):
    xmin, xmax = ax.get_xbound()
    ymin, ymax = ax.get_ybound()
    if xval < 1 or xval < xmin or xval > xmax:
        return
    ytext = ymax - (ymax - ymin) * 0.05
    ax.axvline(xval, linestyle=':', linewidth=1.2, alpha=alpha)
    ax.text(xval, ytext, text, rotation=90, va='top', ha='right', fontsize=9, alpha=alpha)

def add_cache_markers_single(ax, bpe, l1d_kb, l2_kb, l3_mb):
    l1d = int(l1d_kb * 1024)
    l2  = int(l2_kb  * 1024)
    l3  = int(l3_mb  * 1024 * 1024)
    _add_one_marker(ax, l1d // bpe, f"L1d ≈ {int(l1d_kb)}KB")
    _add_one_marker(ax, l2  // bpe, f"L2  ≈ {int(l2_kb)}KB")
    _add_one_marker(ax, l3  // bpe, f"L3  ≈ {int(l3_mb)}MB")

def add_cache_markers_multi(ax, bpe_dict, l1d_kb, l2_kb, l3_mb):
    l1d = int(l1d_kb * 1024)
    l2  = int(l2_kb  * 1024)
    l3  = int(l3_mb  * 1024 * 1024)
    for kname, bpe in bpe_dict.items():
        if bpe <= 0: continue
        _add_one_marker(ax, l1d // bpe, f"L1d[{kname}]", alpha=0.5)
        _add_one_marker(ax, l2  // bpe, f"L2[{kname}]",  alpha=0.5)
        _add_one_marker(ax, l3  // bpe, f"L3[{kname}]",  alpha=0.5)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True, help="locality.csv")
    ap.add_argument("--cpu-hz", type=float, required=True, help="CPU frequency in Hz, e.g., 3.91e9")
    ap.add_argument("--use-rfo", action="store_true", help="count RFO for stores (for cache markers only)")
    ap.add_argument("--outdir", default="figs")
    ap.add_argument("--l1d-kb", type=float, default=32.0)
    ap.add_argument("--l2-kb",  type=float, default=1024.0)
    ap.add_argument("--l3-mb",  type=float, default=32.0)
    ap.add_argument("--combine-only", action="store_true",
                    help="only emit combined (all kernels in one figure) plots")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    agg = load_and_aggregate(args.csv)

    by_kd = defaultdict(lambda: defaultdict(list))
    for (kernel, build, dtype, M), tmed in agg.items():
        by_kd[(kernel, dtype)][build].append((M, tmed))
    for (kernel, dtype), build_map in by_kd.items():
        for b in build_map:
            build_map[b].sort(key=lambda x: x[0])

    if not args.combine_only:
        for (kernel, dtype), build_map in by_kd.items():
            fpe = flops_per_elem(kernel)
            bpe = bytes_per_elem(kernel, dtype, args.use_rfo)  
            data = {}
            for b, arr in build_map.items():
                Ms = np.array([m for m,_ in arr], dtype=np.int64)
                Ts = np.array([t for _,t in arr], dtype=float)
                gflops = (Ms * fpe) / Ts / 1e9
                cpe    = (Ts * args.cpu_hz) / Ms
                data[b] = dict(M=Ms, T=Ts, GF=gflops, CPE=cpe)

            fig, ax = plt.subplots(figsize=(7,4.2))
            for build in sorted(data.keys()):
                style = dict(
                    marker=MARKERS.get((kernel, build), "o"),
                    linestyle=LINESTYLES.get(build, "-"),
                    alpha=0.9 if build=="simd" else 0.8
                )
                plot_one(ax, data[build]["M"], data[build]["GF"], label=build, **style)
            add_cache_markers_single(ax, bpe=bpe,
                                     l1d_kb=args.l1d_kb, l2_kb=args.l2_kb, l3_mb=args.l3_mb)
            ax.set_title(f"{kernel.upper()} ({dtype}) – GFLOP/s vs N")
            ax.set_xlabel("N (elements)")
            ax.set_ylabel("GFLOP/s")
            ax.legend()
            fig.tight_layout()
            fig.savefig(os.path.join(args.outdir, f"{kernel}_{dtype}_gflops.png"), dpi=150)
            plt.close(fig)

            fig, ax = plt.subplots(figsize=(7,4.2))
            for build in sorted(data.keys()):
                style = dict(
                    marker=MARKERS.get((kernel, build), "o"),
                    linestyle=LINESTYLES.get(build, "-"),
                    alpha=0.9 if build=="simd" else 0.8
                )
                plot_one(ax, data[build]["M"], data[build]["CPE"], label=build, **style)
            add_cache_markers_single(ax, bpe=bpe,
                                     l1d_kb=args.l1d_kb, l2_kb=args.l2_kb, l3_mb=args.l3_mb)
            ax.set_title(f"{kernel.upper()} ({dtype}) – CPE vs N")
            ax.set_xlabel("N (elements)")
            ax.set_ylabel("Cycles per Element (CPE)")
            ax.legend()
            fig.tight_layout()
            fig.savefig(os.path.join(args.outdir, f"{kernel}_{dtype}_cpe.png"), dpi=150)
            plt.close(fig)

    grouped = defaultdict(list)  
    for (kernel, build, dtype, M), tmed in agg.items():
        grouped[(dtype, kernel, build)].append((M, tmed))

    all_kernels = ["saxpy", "elemul", "dot"]
    for dtype in sorted(set([k[0] for k in grouped.keys()])):
        for metric, ylab, suffix in [
            ("GF", "GFLOP/s", "gflops"),
            ("CPE","Cycles per Element (CPE)", "cpe"),
        ]:
            fig, ax = plt.subplots(figsize=(8.0, 4.6))
            bpe_dict = {k: bytes_per_elem(k, dtype, args.use_rfo) for k in all_kernels}  # 仅用于缓存竖线

            for kernel in all_kernels:
                for build in ["scalar", "simd"]:
                    arr = grouped.get((dtype, kernel, build), [])
                    if not arr:
                        continue
                    arr.sort(key=lambda x: x[0])
                    Ms = np.array([m for m,_ in arr], dtype=np.int64)
                    Ts = np.array([t for _,t in arr], dtype=float)

                    fpe = flops_per_elem(kernel)
                    gflops = (Ms * fpe) / Ts / 1e9
                    cpe    = (Ts * args.cpu_hz) / Ms

                    ys = gflops if metric=="GF" else cpe
                    label = f"{kernel}-{build}"
                    style = dict(
                        marker=MARKERS.get((kernel, build), "o"),
                        linestyle=LINESTYLES.get(build, "-"),
                        alpha=0.9 if build=="simd" else 0.85
                    )
                    ax.plot(Ms, ys, label=label, **style)

            add_cache_markers_multi(ax, bpe_dict=bpe_dict,
                                    l1d_kb=args.l1d_kb, l2_kb=args.l2_kb, l3_mb=args.l3_mb)

            ax.set_xscale('log')
            ax.grid(True, which='both', linestyle='--', alpha=0.4)
            ax.set_title(f"ALL kernels ({dtype}) – {ylab} vs N")
            ax.set_xlabel("N (elements)")
            ax.set_ylabel(ylab)
            ax.legend(ncols=3)
            fig.tight_layout()
            fig.savefig(os.path.join(args.outdir, f"ALL_{dtype}_{suffix}.png"), dpi=150)
            plt.close(fig)

    print(f"[DONE] Figures written to: {args.outdir}")

if __name__ == "__main__":
    main()
