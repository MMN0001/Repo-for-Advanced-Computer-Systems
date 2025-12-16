import sys
import pandas as pd
import matplotlib.pyplot as plt

csv_path = sys.argv[1] if len(sys.argv) > 1 else "exp3_density.csv"
df = pd.read_csv(csv_path)

for c in ["density","gemm_time_sec","spmm_time_sec","gemm_gflops","spmm_gflops","nnz"]:
    df[c] = pd.to_numeric(df[c], errors="coerce")

df = df.dropna(subset=["density","gemm_time_sec","spmm_time_sec"]).sort_values("density")

ratio = df["spmm_time_sec"] / df["gemm_time_sec"]
p_star = None

for i in range(len(df)-1):
    r0, r1 = ratio.iloc[i], ratio.iloc[i+1]
    if (r0 - 1.0) * (r1 - 1.0) <= 0:
        p0, p1 = df["density"].iloc[i], df["density"].iloc[i+1]
        if r1 != r0:
            p_star = p0 + (1.0 - r0) * (p1 - p0) / (r1 - r0)
        else:
            p_star = p0
        break

plt.figure(figsize=(7.5, 4.8))
plt.plot(df["density"], df["gemm_time_sec"], marker="o", label="Dense GEMM (time)")
plt.plot(df["density"], df["spmm_time_sec"], marker="o", label="CSR-SpMM (time)")

plt.xscale("log")
plt.xlabel("Density p (log scale)")
plt.ylabel("Runtime (s)")
plt.title("Task 3: Density break-even (Dense GEMM vs CSR-SpMM)")
plt.grid(True, which="both", linestyle="--", linewidth=0.5)

if p_star is not None:
    plt.axvline(p_star, linestyle="--")
    y = max(df["gemm_time_sec"].max(), df["spmm_time_sec"].max())
    plt.text(p_star, y*0.9, f"break-even p* ≈ {p_star:.4f}", rotation=90, va="top")

plt.legend()
out = "exp3_density_runtime.png"
plt.tight_layout()
plt.savefig(out, dpi=200, bbox_inches="tight")
print("Saved:", out)
