import pandas as pd
import matplotlib.pyplot as plt

CSV_PATH = "thp.csv"          
OUT_IOPS = "fig_thp_c16_iops.png"
OUT_P99  = "fig_thp_c16_p99.png"

df = pd.read_csv(CSV_PATH)

df["read_iops"] = pd.to_numeric(df["read_iops"])
df["clat_p99_us"] = pd.to_numeric(df["clat_p99_us"])

x_order = ["thp_off", "thp_on"]
df["thp"] = pd.Categorical(df["thp"], categories=x_order, ordered=True)
df = df.sort_values(["mode", "thp"])

modes = ["sync_psync", "async_uring"]

def get_series(mode, col):
    sub = df[df["mode"] == mode].set_index("thp")
    return [sub.loc[x, col] for x in x_order]

x_pos = range(len(x_order))

plt.figure()
for m in modes:
    plt.plot(x_pos, get_series(m, "read_iops"),
             marker="o", label=m)

plt.xticks(x_pos, x_order)
plt.xlabel("THP Setting")
plt.ylabel("Read IOPS")
plt.title("IOPS vs THP (randread, 4KB, O_DIRECT, concurrency=16)")
plt.grid(True, linestyle="--", linewidth=0.5)
plt.ticklabel_format(style="sci", axis="y", scilimits=(0, 0))
plt.legend()
plt.tight_layout()
plt.savefig(OUT_IOPS, dpi=200)

plt.figure()
for m in modes:
    plt.plot(x_pos, get_series(m, "clat_p99_us"),
             marker="o", label=m)

plt.xticks(x_pos, x_order)
plt.xlabel("THP Setting")
plt.ylabel("p99 completion latency (us)")
plt.title("p99 Latency vs THP (randread, 4KB, O_DIRECT, concurrency=16)")
plt.grid(True, linestyle="--", linewidth=0.5)
plt.legend()
plt.tight_layout()
plt.savefig(OUT_P99, dpi=200)

print(f"Saved: {OUT_IOPS}")
print(f"Saved: {OUT_P99}")
