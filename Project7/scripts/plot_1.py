import sys
import pandas as pd
import matplotlib.pyplot as plt

def load_and_aggregate(csv_path: str) -> pd.DataFrame:
    df = pd.read_csv(csv_path)

    df["threads"] = df["threads"].astype(int)
    df["nkeys"] = df["nkeys"].astype(int)
    df["ops_per_sec"] = df["ops_per_sec"].astype(float)
    df["speedup_vs_1t"] = df["speedup_vs_1t"].astype(float)

    g = df.groupby(["impl", "workload", "nkeys", "threads"], as_index=False).agg(
        ops_mean=("ops_per_sec", "mean"),
        ops_std=("ops_per_sec", "std"),
        spd_mean=("speedup_vs_1t", "mean"),
        spd_std=("speedup_vs_1t", "std"),
        reps=("speedup_vs_1t", "count"),
        duration_ms=("duration_ms", "first"),
        prefill=("prefill", "first"),
    )
    return g

def plot_workload(g: pd.DataFrame, workload: str, nkeys: int, out_prefix: str):
    sub = g[(g["workload"] == workload) & (g["nkeys"] == nkeys)].copy()
    if sub.empty:
        print(f"[WARN] No data for workload={workload}, nkeys={nkeys}")
        return


    plt.figure()
    for impl in ["baseline", "fine"]:
        s = sub[sub["impl"] == impl].sort_values("threads")
        if s.empty:
            continue
        plt.errorbar(
            s["threads"], s["ops_mean"], yerr=s["ops_std"],
            marker="o", capsize=3, label=impl
        )
    plt.xlabel("Threads")
    plt.ylabel("Throughput (ops/s)")
    plt.title(f"Throughput | workload={workload}, nkeys={nkeys}")
    plt.grid(True, which="both", linestyle="--", linewidth=0.5)
    plt.legend()
    plt.tight_layout()
    plt.savefig(f"{out_prefix}_{workload}_throughput.png", dpi=200)
    plt.close()

    plt.figure()
    for impl in ["baseline", "fine"]:
        s = sub[sub["impl"] == impl].sort_values("threads")
        if s.empty:
            continue
        plt.errorbar(
            s["threads"], s["spd_mean"], yerr=s["spd_std"],
            marker="o", capsize=3, label=impl
        )
    plt.xlabel("Threads")
    plt.ylabel("Speedup")
    plt.title(f"Speedup | workload={workload}, nkeys={nkeys}")
    plt.grid(True, which="both", linestyle="--", linewidth=0.5)
    plt.legend()
    plt.tight_layout()
    plt.savefig(f"{out_prefix}_{workload}_speedup.png", dpi=200)
    plt.close()

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 plot_group1.py <group1.csv> [nkeys]")
        sys.exit(1)

    csv_path = sys.argv[1]
    g = load_and_aggregate(csv_path)

    if len(sys.argv) >= 3:
        nkeys = int(sys.argv[2])
    else:
        nkeys = int(g["nkeys"].iloc[0])

    out_prefix = f"group1_nkeys{nkeys}"

    for wl in ["lookup", "insert", "mixed"]:
        plot_workload(g, wl, nkeys, out_prefix)

    print("Saved figures:")
    print(f"  {out_prefix}_lookup_throughput.png / {out_prefix}_lookup_speedup.png")
    print(f"  {out_prefix}_insert_throughput.png / {out_prefix}_insert_speedup.png")
    print(f"  {out_prefix}_mixed_throughput.png / {out_prefix}_mixed_speedup.png")

if __name__ == "__main__":
    main()
