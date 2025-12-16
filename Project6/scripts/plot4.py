import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("exp4.csv")

df["threads"] = pd.to_numeric(df["threads"], errors="coerce")
df["throughput_mops"] = pd.to_numeric(df["throughput_mops"], errors="coerce")

df = df.dropna(subset=["threads", "throughput_mops"])
df["threads"] = df["threads"].astype(int)

g = (
    df.groupby(["workload", "filter", "threads"], as_index=False)["throughput_mops"]
      .mean()
      .sort_values("threads")
)

plt.figure()
for (workload, filt), sub in g.groupby(["workload", "filter"]):
    plt.plot(sub["threads"], sub["throughput_mops"], marker="o", linewidth=1.5,
             label=f"{workload}-{filt}")

plt.xlabel("Threads")
plt.ylabel("Throughput (Mops/s)")
plt.title("Task 3.4: Thread Scaling (Cuckoo vs Quotient, Read-mostly vs Balanced)")
plt.grid(True)
plt.legend()
plt.savefig("exp4_thread_scaling_all.png", dpi=200, bbox_inches="tight")
plt.show()

print("[ok] wrote exp4_thread_scaling_all.png")
