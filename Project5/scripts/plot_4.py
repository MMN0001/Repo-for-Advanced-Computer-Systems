import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

CSV = "ws_summary.csv"
OUT_PREFIX = "ws44"

df = pd.read_csv(CSV).sort_values("N")
N = df["N"].to_numpy()
bw_stream = float(df["bw_stream_GBs"].iloc[0])

# Cache boundaries derived from working-set ≈ 12*N^2 bytes 
N_L1 = 52
N_L2 = 294
N_L3 = 1665

def mean_yerr(mean_col, p05_col, p95_col):
    mean = df[mean_col].to_numpy()
    p05  = df[p05_col].to_numpy()
    p95  = df[p95_col].to_numpy()
    yerr = np.vstack([mean - p05, p95 - mean])  
    return mean, yerr

def draw_cache_lines():
    plt.axvline(N_L1, color="gray", linestyle="--", linewidth=1)
    plt.axvline(N_L2, color="gray", linestyle="--", linewidth=1)
    plt.axvline(N_L3, color="gray", linestyle="--", linewidth=1)

    ylim = plt.gca().get_ylim()
    ytxt = ylim[1] * 0.92

    plt.text(N_L1, ytxt, "L1",  rotation=90, va="top", ha="right", color="gray")
    plt.text(N_L2, ytxt, "L2",  rotation=90, va="top", ha="right", color="gray")
    plt.text(N_L3, ytxt, "LLC", rotation=90, va="top", ha="right", color="gray")
    plt.text(N_L3 * 1.15, ytxt, "DRAM", rotation=90, va="top", ha="left", color="gray")

def plot_err(x, mean, yerr, ylabel, title, out_png, hline=None, hline_label=None):
    plt.figure()
    plt.errorbar(x, mean, yerr=yerr, fmt="o-", capsize=3)

    if hline is not None:
        plt.axhline(hline, color="gray", linestyle="--", linewidth=1)
        if hline_label:
            plt.text(x[-1], hline, f"  {hline_label}", va="center", color="gray")

    draw_cache_lines()

    plt.xscale("log", base=2)
    plt.grid(True, which="both", linestyle="--", linewidth=0.5)
    plt.xlabel("Matrix size N (m=k=n=N)")
    plt.ylabel(ylabel)
    plt.title(title + " (error bars: p05–p95)")
    plt.tight_layout()
    plt.savefig(out_png, dpi=200)
    plt.close()

gemm_g_mean, gemm_g_yerr = mean_yerr("gemm_gflops_mean", "gemm_gflops_p05", "gemm_gflops_p95")
plot_err(
    N, gemm_g_mean, gemm_g_yerr,
    ylabel="GFLOP/s",
    title="Dense GEMM: GFLOP/s",
    out_png=f"{OUT_PREFIX}_gemm_gflops.png"
)

gemm_b_mean, gemm_b_yerr = mean_yerr("gemm_bw_mean", "gemm_bw_p05", "gemm_bw_p95")
plot_err(
    N, gemm_b_mean, gemm_b_yerr,
    ylabel="Effective bandwidth (GB/s)",
    title="Dense GEMM: Effective bandwidth",
    out_png=f"{OUT_PREFIX}_gemm_bw.png",
    hline=bw_stream,
    hline_label=f"STREAM = {bw_stream:.3f} GB/s"
)

spmm_g_mean, spmm_g_yerr = mean_yerr("spmm_gflops_mean", "spmm_gflops_p05", "spmm_gflops_p95")
plot_err(
    N, spmm_g_mean, spmm_g_yerr,
    ylabel="GFLOP/s",
    title="CSR SpMM: GFLOP/s",
    out_png=f"{OUT_PREFIX}_spmm_gflops.png"
)

spmm_b_mean, spmm_b_yerr = mean_yerr("spmm_bw_mean", "spmm_bw_p05", "spmm_bw_p95")
plot_err(
    N, spmm_b_mean, spmm_b_yerr,
    ylabel="Effective bandwidth (GB/s, estimated)",
    title="CSR SpMM: Effective bandwidth",
    out_png=f"{OUT_PREFIX}_spmm_bw.png",
    hline=bw_stream,
    hline_label=f"STREAM = {bw_stream:.3f} GB/s"
)
