import pandas as pd
import matplotlib.pyplot as plt


df = pd.read_csv("rw_mix.csv")  

order = ["100R", "70/30", "50/50", "100W"]
df["mix"] = pd.Categorical(df["mix"], categories=order, ordered=True)
df = df.sort_values("mix")


plt.figure(figsize=(7,4))
plt.bar(df["mix"], df["avg_ns"])
plt.xlabel("Read/Write Mix")
plt.ylabel("Average Latency per Access (ns)")
plt.title("Memory Access Latency vs Read/Write Mix")
for i, v in enumerate(df["avg_ns"]):
    plt.text(i, v*1.01, f"{v:.3f}", ha="center", va="bottom")
plt.tight_layout()
plt.savefig("rw_mix_latency.png", dpi=300)
plt.show()


plt.figure(figsize=(7,4))
plt.bar(df["mix"], df["BW_GB_Ps" if "BW_GB_Ps" in df.columns else "BW_GBps"] if "BW_GBps" not in df.columns else df["BW_GBps"])

bw_col = "BW_GBps" if "BW_GBps" in df.columns else ("BW_GB_Ps" if "BW_GB_Ps" in df.columns else None)
if bw_col is None:
    raise SystemExit("Cannot find bandwidth column. Rename it to BW_GBps.")
plt.bar(df["mix"], df[bw_col])
plt.xlabel("Read/Write Mix")
plt.ylabel("Bandwidth (GB/s)")
plt.title("Memory Bandwidth vs Read/Write Mix")
for i, v in enumerate(df[bw_col]):
    plt.text(i, v*1.01, f"{v:.2f}", ha="center", va="bottom")
plt.tight_layout()
plt.savefig("rw_mix_bandwidth.png", dpi=300)
plt.show()
