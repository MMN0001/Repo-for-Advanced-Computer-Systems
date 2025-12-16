import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

CSV_PATH = "exp3.csv"
OUT_DIR = "exp3_plots"

def _ensure_dir(d):
    os.makedirs(d, exist_ok=True)

def _label(row, extra=""):
    f = str(row["filter"])
    b = int(row["param_bits"])
    return f"{f}-b{b}{extra}"

def plot_metric(df, y_col, title, y_label, out_name, only_filter=None, only_cols_exist=True):
    plt.figure()
    if only_filter is not None:
        df = df[df["filter"] == only_filter].copy()

    if df.empty:
        print(f"[skip] {out_name}: no data")
        return

    for (filt, bits), g in df.groupby(["filter", "param_bits"], sort=True):
        g = g.sort_values("load")
        x = g["load"].to_numpy()
        y = g[y_col].to_numpy()
        plt.plot(x, y, marker="o", linewidth=1.5, label=f"{filt}-b{int(bits)}")

    plt.xlabel("Load factor")
    plt.ylabel(y_label)
    plt.title(title)
    plt.grid(True)
    plt.legend()
    out_path = os.path.join(OUT_DIR, out_name)
    plt.savefig(out_path, dpi=200, bbox_inches="tight")
    plt.show()
    print(f"[ok] wrote {out_path}")

def plot_two_metrics_same_fig(df, y1, y2, title, y_label, out_name, only_filter=None):
    plt.figure()
    if only_filter is not None:
        df = df[df["filter"] == only_filter].copy()

    if df.empty:
        print(f"[skip] {out_name}: no data")
        return

    for (filt, bits), g in df.groupby(["filter", "param_bits"], sort=True):
        g = g.sort_values("load")
        x = g["load"].to_numpy()
        plt.plot(x, g[y1].to_numpy(), marker="o", linewidth=1.5, label=f"{filt}-b{int(bits)}:{y1}")
        plt.plot(x, g[y2].to_numpy(), marker="x", linewidth=1.5, label=f"{filt}-b{int(bits)}:{y2}")

    plt.xlabel("Load factor")
    plt.ylabel(y_label)
    plt.title(title)
    plt.grid(True)
    plt.legend()
    out_path = os.path.join(OUT_DIR, out_name)
    plt.savefig(out_path, dpi=200, bbox_inches="tight")
    plt.show()
    print(f"[ok] wrote {out_path}")

def main():
    _ensure_dir(OUT_DIR)
    df = pd.read_csv(CSV_PATH)

    for c in ["filter"]:
        df[c] = df[c].astype(str)

    for c in ["param_bits"]:
        df[c] = df[c].astype(int)

    for c in ["load"]:
        df[c] = df[c].astype(float)

    numeric_cols = [
        "insert_mops", "delete_mops", "insert_fail_rate",
        "cuckoo_total_kicks", "cuckoo_stash_inserts", "cuckoo_avg_probe",
        "qf_cluster_p50", "qf_cluster_p95", "qf_cluster_p99", "qf_cluster_max"
    ]
    for c in numeric_cols:
        if c in df.columns:
            df[c] = pd.to_numeric(df[c], errors="coerce")

    df_c = df[df["filter"] == "Cuckoo"].copy()
    df_q = df[df["filter"] == "Quotient"].copy()

    plot_metric(
        df, "insert_mops",
        "Task 3: Insert Throughput vs Load (Cuckoo + Quotient, b=8/12/16)",
        "Insert throughput (Mops/s)",
        "01_insert_mops_all.png"
    )

    plot_metric(
        df, "delete_mops",
        "Task 3: Delete Throughput vs Load (Cuckoo + Quotient, b=8/12/16)",
        "Delete throughput (Mops/s)",
        "02_delete_mops_all.png"
    )

    plot_metric(
        df, "insert_fail_rate",
        "Task 3: Insert Hard-Fail Rate vs Load (Cuckoo + Quotient, b=8/12/16)",
        "Insert failure rate",
        "03_insert_fail_rate_all.png"
    )

    if not df_c.empty:
        plot_metric(
            df_c, "cuckoo_avg_probe",
            "Cuckoo: Average Probe Length vs Load (b=8/12/16)",
            "Avg probe length",
            "04_cuckoo_avg_probe.png"
        )

        plot_metric(
            df_c, "cuckoo_total_kicks",
            "Cuckoo: Total Kicks vs Load (b=8/12/16)",
            "Total kicks",
            "05_cuckoo_total_kicks.png"
        )

        plot_metric(
            df_c, "cuckoo_stash_inserts",
            "Cuckoo: Stash Inserts vs Load (b=8/12/16)",
            "Stash inserts",
            "06_cuckoo_stash_inserts.png"
        )

    if not df_q.empty:
        plot_two_metrics_same_fig(
            df_q, "qf_cluster_p95", "qf_cluster_p99",
            "Quotient: Cluster Length Tail (p95 & p99) vs Load (b=8/12/16)",
            "Cluster length",
            "07_qf_cluster_p95_p99.png"
        )

        plot_metric(
            df_q, "qf_cluster_max",
            "Quotient: Max Cluster Length vs Load (b=8/12/16)",
            "Max cluster length",
            "08_qf_cluster_max.png"
        )

if __name__ == "__main__":
    main()
