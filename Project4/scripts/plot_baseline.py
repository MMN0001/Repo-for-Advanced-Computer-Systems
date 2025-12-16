import pandas as pd
import matplotlib.pyplot as plt

CSV_PATH = "baseline.csv"          
OUT_IOPS = "fig_iops_vs_conc.png"
OUT_P99  = "fig_p99_vs_conc.png"

df = pd.read_csv(CSV_PATH)

required = {"mode", "concurrency", "read_iops", "clat_p99_us"}
missing = required - set(df.columns)
if missing:
    raise ValueError(f"Missing columns in {CSV_PATH}: {missing}\n"
                     f"Found columns: {list(df.columns)}")

df["concurrency"] = pd.to_numeric(df["concurrency"])
df["read_iops"] = pd.to_numeric(df["read_iops"])
df["clat_p99_us"] = pd.to_numeric(df["clat_p99_us"])

df = df.sort_values(["mode", "concurrency"])

modes = df["mode"].unique()

plt.figure()
for m in modes:
    sub = df[df["mode"] == m]
    plt.plot(sub["concurrency"], sub["read_iops"], marker="o", label=m)
plt.xscale("log", base=2)  
plt.xticks([1, 4, 16, 64], [1, 4, 16, 64])
plt.xlabel("Concurrency")
plt.ylabel("Read IOPS")
plt.title("IOPS vs Concurrency (Random Read, 4KB, O_DIRECT)")
plt.grid(True, which="both", linestyle="--", linewidth=0.5)
plt.legend()
plt.tight_layout()
plt.savefig(OUT_IOPS, dpi=200)

plt.figure()
for m in modes:
    sub = df[df["mode"] == m]
    plt.plot(sub["concurrency"], sub["clat_p99_us"], marker="o", label=m)
plt.xscale("log", base=2)
plt.xticks([1, 4, 16, 64], [1, 4, 16, 64])
plt.xlabel("Concurrency")
plt.ylabel("p99 completion latency (us)")
plt.title("p99 Latency vs Concurrency (Random Read, 4KB, O_DIRECT)")
plt.grid(True, which="both", linestyle="--", linewidth=0.5)
plt.legend()
plt.tight_layout()
plt.savefig(OUT_P99, dpi=200)

print(f"Saved: {OUT_IOPS}")
print(f"Saved: {OUT_P99}")
