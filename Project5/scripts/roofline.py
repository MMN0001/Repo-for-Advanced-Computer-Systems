import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

CSV = "ws_summary.csv"

P_PEAK = 900.0  
df = pd.read_csv(CSV).sort_values("N")
BW_STREAM = float(df["bw_stream_GBs"].iloc[0])  

ai_gemm = df["gemm_gflops_mean"].to_numpy() / df["gemm_bw_mean"].to_numpy()
ai_spmm = df["spmm_gflops_mean"].to_numpy() / df["spmm_bw_mean"].to_numpy()

p_gemm = df["gemm_gflops_mean"].to_numpy()
p_spmm = df["spmm_gflops_mean"].to_numpy()

ai_min = min(ai_gemm.min(), ai_spmm.min()) / 2.0
ai_max = max(ai_gemm.max(), ai_spmm.max()) * 2.0
AI_line = np.logspace(np.log10(ai_min), np.log10(ai_max), 400)

mem_roof = BW_STREAM * AI_line
comp_roof = np.full_like(AI_line, P_PEAK)

AI_star = P_PEAK / BW_STREAM  

plt.figure(figsize=(7.5, 6.2))

plt.loglog(AI_line, mem_roof, linestyle="--", linewidth=2,
           label=f"Memory roof (BW={BW_STREAM:.3f} GB/s)")
plt.loglog(AI_line, comp_roof, linestyle="-", linewidth=2,
           label=f"Compute roof (Peak={P_PEAK:.0f} GFLOP/s)")

plt.axvline(AI_star, linestyle=":", linewidth=1.8)
plt.text(AI_star, P_PEAK/3, f"  AI*={AI_star:.1f}", rotation=90,
         va="bottom", ha="left")

plt.loglog(ai_gemm, p_gemm, "o", label="Dense GEMM (mean)")
plt.loglog(ai_spmm, p_spmm, "s", label="CSR SpMM (mean)")

for i in [0, 3, 6, 9, 12, 13]:
    n = int(df["N"].iloc[i])
    plt.text(ai_gemm[i], p_gemm[i], f" N={n}", fontsize=8, ha="left", va="bottom")

plt.xlabel("Arithmetic Intensity (FLOP / Byte)")
plt.ylabel("Performance (GFLOP/s)")
plt.title("Roofline Model (measured BW + estimated peak FLOPs)")
plt.grid(True, which="both", linestyle="--", linewidth=0.5)
plt.legend()
plt.tight_layout()
plt.savefig("roofline.png", dpi=220)
plt.show()

print(f"BW_STREAM = {BW_STREAM:.3f} GB/s")
print(f"P_PEAK    = {P_PEAK:.1f} GFLOP/s")
print(f"AI*       = {AI_star:.3f} FLOP/Byte")
