import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("exp2_fig1_raw.csv")

for c in ["kernel", "shape", "variant"]:
    df[c] = df[c].astype(str).str.strip()
df["threads"] = pd.to_numeric(df["threads"], errors="coerce").astype(int)
df["time_sec"] = pd.to_numeric(df["time_sec"], errors="coerce")

base_df = df[(df["variant"] == "baseline") & (df["threads"] == 1)][["kernel", "shape", "time_sec"]].drop_duplicates()
base = {(r.kernel, r.shape): r.time_sec for r in base_df.itertuples(index=False)}

def speedup(row):
    t0 = base.get((row["kernel"], row["shape"]))
    if t0 is None or row["time_sec"] <= 0:
        return float("nan")
    return t0 / row["time_sec"]

df["speedup"] = df.apply(speedup, axis=1)

shapes_order = ["square", "tall_skinny", "fat"]
labels = {"simd": "SIMD-only", "thread": "Thread-only", "simd_thread": "SIMD+Thread"}

threads_ticks = sorted(df[df["variant"].isin(["thread", "simd_thread"])]["threads"].unique())

def plot_kernel(kernel):
    fig, axes = plt.subplots(1, 3, figsize=(15, 4), sharey=True)

    for ax, shape in zip(axes, shapes_order):
        sub = df[(df["kernel"] == kernel) & (df["shape"] == shape)].copy()

        simd_series = sub[(sub["variant"] == "simd") & (sub["threads"] == 1)]["speedup"].dropna()
        simd_val = float(simd_series.iloc[0]) if len(simd_series) else None

        for v in ["thread", "simd_thread"]:
            s = sub[sub["variant"] == v].sort_values("threads")
            ax.plot(s["threads"], s["speedup"], marker="o", label=labels[v])

        if simd_val is not None:
            ax.plot(threads_ticks, [simd_val] * len(threads_ticks), linestyle="--", label=labels["simd"])

        ax.set_xscale("log", base=2)
        ax.set_xticks(threads_ticks)
        ax.set_xticklabels([str(t) for t in threads_ticks])
        ax.set_xlabel("Threads")
        ax.set_title(shape.replace("_", "-"))
        ax.grid(True, which="both", linestyle="--", linewidth=0.5)

    axes[0].set_ylabel("Speedup (vs baseline, 1 thread)")

    handles, leg_labels = [], []
    for ax in axes:
        h, l = ax.get_legend_handles_labels()
        for hh, ll in zip(h, l):
            if ll not in leg_labels:
                handles.append(hh)
                leg_labels.append(ll)

    fig.legend(
        handles,
        leg_labels,
        loc="upper center",
        ncol=3,
        frameon=False,
        bbox_to_anchor=(0.5, 1.02)
    )

    fig.suptitle(f"Figure 1: Speedup vs Threads ({kernel.upper()})", y=1.12)
    fig.tight_layout(rect=[0, 0, 1, 0.95])

    out = f"fig1_speedup_{kernel}.png"
    fig.savefig(out, dpi=200, bbox_inches="tight")
    plt.close(fig)
    print("Saved:", out)

plot_kernel("gemm")
plot_kernel("spmm")
