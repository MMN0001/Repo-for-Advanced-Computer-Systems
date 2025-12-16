import os
import pandas as pd
import matplotlib.pyplot as plt

CSV_PATH = "exp2.csv"
OUT_DIR = "figs_exp2"

def ensure_dir(p):
    os.makedirs(p, exist_ok=True)

def load_csv(path):
    df = pd.read_csv(path)
    needed = [
        "filter","target_fpr","param_bits","neg_share","nqueries",
        "secs","qps","mops","lat_p50_ns","lat_p95_ns","lat_p99_ns","hit_rate"
    ]
    missing = [c for c in needed if c not in df.columns]
    if missing:
        raise ValueError(f"Missing columns in CSV: {missing}")
    return df

def plot_throughput(df_fpr, target_fpr, out_dir):

    plt.figure()
    for filt, g in df_fpr.groupby("filter"):
        g = g.sort_values("neg_share")
        plt.plot(g["neg_share"], g["mops"], marker="o", label=filt)
    plt.xlabel("Negative-lookup share (%)")
    plt.ylabel("Throughput (Mops/s)")
    plt.title(f"Lookup Throughput vs Negative Share (target_fpr={target_fpr:g})")
    plt.grid(True)
    plt.legend()
    out = os.path.join(out_dir, f"exp2_throughput_targetFPR_{target_fpr:g}.png")
    plt.tight_layout()
    plt.savefig(out, dpi=200)
    plt.close()

def plot_latency(df_fpr, target_fpr, out_dir, which="p99"):

    col_ns = {"p50": "lat_p50_ns", "p95": "lat_p95_ns", "p99": "lat_p99_ns"}[which]

    plt.figure()
    for filt, g in df_fpr.groupby("filter"):
        g = g.sort_values("neg_share")

        y_us = g[col_ns] / 1000.0
        plt.plot(g["neg_share"], y_us, marker="o", label=filt)

    plt.xlabel("Negative-lookup share (%)")
    plt.ylabel(f"Latency {which} (us)")
    plt.title(f"Lookup Latency ({which}) vs Negative Share (target_fpr={target_fpr:g})")
    plt.grid(True)
    plt.legend()
    out = os.path.join(out_dir, f"exp2_latency_{which}_targetFPR_{target_fpr:g}.png")
    plt.tight_layout()
    plt.savefig(out, dpi=200)
    plt.close()

def plot_latency_all_in_one(df_fpr, target_fpr, out_dir, filter_name):
  
    g = df_fpr[df_fpr["filter"] == filter_name].sort_values("neg_share")
    if g.empty:
        return

    plt.figure()
    plt.plot(g["neg_share"], g["lat_p50_ns"]/1000.0, marker="o", label="p50")
    plt.plot(g["neg_share"], g["lat_p95_ns"]/1000.0, marker="o", label="p95")
    plt.plot(g["neg_share"], g["lat_p99_ns"]/1000.0, marker="o", label="p99")
    plt.xlabel("Negative-lookup share (%)")
    plt.ylabel("Latency (us)")
    plt.title(f"{filter_name} Latency Tails vs Negative Share (target_fpr={target_fpr:g})")
    plt.grid(True)
    plt.legend()
    out = os.path.join(out_dir, f"exp2_{filter_name}_lat_tails_targetFPR_{target_fpr:g}.png")
    plt.tight_layout()
    plt.savefig(out, dpi=200)
    plt.close()

def main():
    ensure_dir(OUT_DIR)
    df = load_csv(CSV_PATH)

   

    for target_fpr, df_fpr in df.groupby("target_fpr"):

        plot_throughput(df_fpr, target_fpr, OUT_DIR)


        plot_latency(df_fpr, target_fpr, OUT_DIR, "p50")
        plot_latency(df_fpr, target_fpr, OUT_DIR, "p95")
        plot_latency(df_fpr, target_fpr, OUT_DIR, "p99")

    
        for filt in sorted(df_fpr["filter"].unique()):
            plot_latency_all_in_one(df_fpr, target_fpr, OUT_DIR, filt)

    print(f"Done. Figures saved to: {OUT_DIR}/")

if __name__ == "__main__":
    main()
