import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("exp1.csv")


numeric_cols = [
    "target_fpr", "param_bits", "n_inserted", "bytes",
    "bpe", "neg_queries", "fp_count", "achieved_fpr", "build_ms"
]
for c in numeric_cols:
    df[c] = pd.to_numeric(df[c])

filters = df["filter"].unique()


plt.figure(figsize=(7, 5))

for f in filters:
    sub = df[df["filter"] == f]
    plt.plot(
        sub["achieved_fpr"],
        sub["bpe"],
        marker="o",
        linewidth=2,
        label=f
    )

plt.xscale("log")
plt.xlabel("Achieved False Positive Rate")
plt.ylabel("Bits Per Element (BPE)")
plt.title("Space vs Accuracy Tradeoff")
plt.grid(True, which="both", linestyle="--", alpha=0.5)
plt.legend()
plt.tight_layout()
plt.savefig("exp1_bpe_vs_fpr.png", dpi=200)
plt.show()


plt.figure(figsize=(6, 6))

for f in filters:
    sub = df[df["filter"] == f]
    plt.plot(
        sub["target_fpr"],
        sub["achieved_fpr"],
        marker="o",
        linewidth=2,
        label=f
    )


targets = sorted(df["target_fpr"].unique())
plt.plot(targets, targets, "k--", label="ideal (y = x)")

plt.xscale("log")
plt.yscale("log")
plt.xlabel("Target FPR")
plt.ylabel("Achieved FPR")
plt.title("Target vs Achieved FPR")
plt.grid(True, which="both", linestyle="--", alpha=0.5)
plt.legend()
plt.tight_layout()
plt.savefig("exp1_target_vs_achieved.png", dpi=200)
plt.show()


plt.figure(figsize=(8, 4))

bar_width = 0.25
targets = sorted(df["target_fpr"].unique())
x = range(len(filters))

for i, t in enumerate(targets):
    sub = df[df["target_fpr"] == t]
    plt.bar(
        [j + i * bar_width for j in x],
        sub["build_ms"],
        width=bar_width,
        label=f"target_fpr={t}"
    )

plt.xticks(
    [j + bar_width for j in x],
    filters
)
plt.ylabel("Build Time (ms)")
plt.title("Build Cost Comparison")
plt.legend()
plt.tight_layout()
plt.savefig("exp1_build_time.png", dpi=200)
plt.show()
