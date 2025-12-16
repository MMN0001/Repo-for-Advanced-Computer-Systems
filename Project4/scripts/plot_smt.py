import pandas as pd
import matplotlib.pyplot as plt

CSV_PATH = "smt.csv"
OUT_IOPS = "fig_smt_c16_iops.png"
OUT_P99  = "fig_smt_c16_p99.png"

df = pd.read_csv(CSV_PATH)

required = {"mode", "smt", "concurrency", "read_iops", "clat_p99_us"}
missing = required - set(df.columns)
if missing:
    raise ValueError(f"Missing columns in {CSV_PATH}: {missing}\nFound: {list(df.columns)}")

df["read_iops"] = pd.to_numeric(df["read_iops"])
df["clat_p99_us"] = pd.to_numeric(df["clat_p99_us"])

x_order = ["smt_off", "smt_on"]
df["smt"] = pd.Categorical(df["smt"], categories=x_order, ordered=True)
df = df.sort_values(["mode", "smt"])

modes = ["sync_psync", "async_uring"]
modes = [m for m in modes if m in set(df["mode"])]

x_labels = x_order
x_pos = list(range(len(x_labels)))

def series_for(mode, col):
    sub = df[df["mode"] == mode].set_index("smt")
    return [sub.loc[x, col] for x in x_labels]

plt.figure()
for m in modes:
    y = series_for(m, "read_iops")
    plt.plot(x_pos, y, marker="o", label=m)

plt.xticks(x_pos, x_labels)
plt.xlabel("SMT Setting")
plt.ylabel("Read IOPS")
plt.title("IOPS vs SMT (randread, 4KB, O_DIRECT, concurrency=16)")
plt.grid(True, linestyle="--", linewidth=0.5)
plt.ticklabel_format(style="sci", axis="y", scilimits=(0, 0))
plt.legend()
plt.tight_layout()
plt.savefig(OUT_IOPS, dpi=200)

plt.figure()
for m in modes:
    y = series_for(m, "clat_p99_us")
    plt.plot(x_pos, y, marker="o", label=m)

plt.xticks(x_pos, x_labels)
plt.xlabel("SMT Setting")
plt.ylabel("p99 completion latency (us)")
plt.title("p99 Latency vs SMT (randread, 4KB, O_DIRECT, concurrency=16)")
plt.grid(True, linestyle="--", linewidth=0.5)
plt.legend()
plt.tight_layout()
plt.savefig(OUT_P99, dpi=200)

print(f"Saved: {OUT_IOPS}")
print(f"Saved: {OUT_P99}")
