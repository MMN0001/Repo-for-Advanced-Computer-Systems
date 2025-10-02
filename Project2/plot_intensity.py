import sys, argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", help="CSV path (sizeB,strideB,iters,threads,streams,intensity,avg_ns,BW_GBps)")
    ap.add_argument("--rel", type=float, default=0.5)
    ap.add_argument("--abs", type=float, default=0.5)
    ap.add_argument("--peak", type=float, default=None)
    args = ap.parse_args()

    df = pd.read_csv(args.csv)
    for col in ["threads","avg_ns","BW_GBps"]:
        if col not in df.columns:
            raise SystemExit(f"Missing column '{col}' in CSV")

    thr_df = df.sort_values("threads").reset_index(drop=True)
    T = thr_df["threads"].to_numpy(dtype=int)
    BW = thr_df["BW_GBps"].to_numpy()
    dBW = np.diff(BW)

    if len(dBW) == 0:
        knee_T = int(T[0])
    else:
        init_gain = dBW[0]
        rel_bad = np.where(dBW < args.rel * init_gain)[0]
        abs_bad = np.where(dBW < args.abs)[0]
        bad = rel_bad if rel_bad.size else abs_bad
        if bad.size:
            idx = bad[0]
            knee_T = int(T[idx])
        else:
            knee_T = int(T[np.argmax(BW)])

    plot_df = df.sort_values("avg_ns").reset_index(drop=True)
    x = plot_df["avg_ns"].to_numpy()
    y = plot_df["BW_GBps"].to_numpy()
    T_plot = plot_df["threads"].to_numpy(dtype=int)

    knee_row = plot_df.loc[plot_df["threads"] == knee_T].iloc[0]
    kx, ky = float(knee_row["avg_ns"]), float(knee_row["BW_GBps"])

    plt.figure(figsize=(8,4.8))
    plt.plot(x, y, marker="o", linewidth=2, zorder=1)

    plt.scatter([kx], [ky], s=160, marker="*", zorder=5)
    plt.text(kx, ky*1.06, f"knee (T={knee_T})", ha="center", fontsize=10)

    for xi, yi, ti in zip(x, y, T_plot):
        plt.text(xi, yi*0.98, f"T={int(ti)}", ha="center", va="top", fontsize=9)

    plt.xlabel("Latency (ns)")
    plt.ylabel("Throughput (GiB/s)")
    plt.title("Throughput–Latency Curve")
    plt.grid(alpha=0.3)
    plt.tight_layout()
    plt.savefig("throughput_latency_curve.png", dpi=300)
    plt.show()

    peak_obs = float(np.max(y))
    peak = args.peak if args.peak is not None else peak_obs
    pct = ky / peak * 100.0 if peak > 0 else float("nan")

    ki_thr_idx = np.where(T == knee_T)[0][0]
    left_gain  = (BW[ki_thr_idx] - BW[ki_thr_idx-1]) if ki_thr_idx-1 >= 0 else np.nan
    right_gain = (BW[ki_thr_idx+1] - BW[ki_thr_idx]) if ki_thr_idx+1 < len(BW) else np.nan

    print(f"Knee @ T={knee_T}: latency={kx:.2f} ns, throughput={ky:.2f} GiB/s, "
          f"{pct:.1f}% of peak ({peak:.2f} GiB/s)")

if __name__ == "__main__":
    main()
