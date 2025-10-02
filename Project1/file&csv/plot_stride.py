import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

CSV_IN = Path("stride.csv")
if not CSV_IN.exists():
    raise FileNotFoundError("stride.csv not found in the current directory.")

df = pd.read_csv(CSV_IN)

df['time_s'] = pd.to_numeric(df['time_s'], errors='coerce')
df['M'] = pd.to_numeric(df['M'], errors='coerce')
df['stride'] = pd.to_numeric(df['stride'], errors='coerce')
df = df.dropna(subset=['time_s','M','stride'])

def elem_size(dtype: str) -> int:
    return 4 if dtype == 'f32' else 8 if dtype == 'f64' else 4

def bytes_per_iter(kernel: str, dtype: str) -> int:
    es = elem_size(dtype)
    if kernel == 'saxpy':    # y = a*x + y
        return 3 * es
    elif kernel == 'elemul': # z = x * y
        return 3 * es
    elif kernel == 'dot':    # s += x*y
        return 2 * es
    return 3 * es

def flops_per_iter(kernel: str) -> int:
    if kernel in ('saxpy','dot'):
        return 2
    elif kernel == 'elemul':
        return 1
    return 1

# ---- aggregate ----
grp_cols = ['kernel','dtype','build','stride']
agg = (df
       .groupby(grp_cols, as_index=False)
       .agg(
           M=('M','first'),
           align=('align','first'),
           misalign_bytes=('misalign_bytes','first'),
           time_median=('time_s','median'),
           time_mean=('time_s','mean'),
           time_std=('time_s','std'),
           n_runs=('time_s','size')
       ))

# ---- metrics ----
def compute_metrics(row):
    B = bytes_per_iter(row['kernel'], row['dtype'])
    F = flops_per_iter(row['kernel'])
    M = row['M']
    t = row['time_median']
    if t <= 0:
        bw_gibs = np.nan
        gflops = np.nan
    else:
        bytes_total = B * M
        bw_gibs = bytes_total / t / (1024**3)
        gflops = (F * M) / t / 1e9
    return pd.Series({'bytes_per_iter': B, 'flops_per_iter': F,
                      'bw_gibs': bw_gibs, 'gflops': gflops})

metrics = agg.apply(compute_metrics, axis=1)
tbl = pd.concat([agg, metrics], axis=1).sort_values(['kernel','dtype','build','stride'])

tbl.to_csv("stride_metrics.csv", index=False)

for (k, d, b), sub in tbl.groupby(['kernel','dtype','build']):
    sub = sub.sort_values('stride')

    sub = sub[sub['stride'].isin([1,2,4,8])]

    plt.figure()
    plt.plot(sub['stride'], sub['bw_gibs'], marker='o')
    plt.xlabel("Stride")
    plt.ylabel("Effective Bandwidth (GiB/s)")
    plt.title(f"Effective Bandwidth vs Stride\nkernel={k}, dtype={d}, build={b}")
    plt.xticks([1,2,4,8])   
    plt.grid(True, linestyle='--', linewidth=0.5)
    plt.tight_layout()
    plt.savefig(f"bw_vs_stride_{k}_{d}_{b}.png", dpi=150)
    plt.close()

print("Done. Generated: stride_metrics.csv + Bandwidth PNG charts (stride=1,2,4,8).")
