import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import textwrap
import os

CSV_PATH = "saxpy_f32_L2.csv"              
OUT_CSV  = "table_saxpy_summary.csv"       
OUT_PNG  = "table_saxpy_summary.png"       

df = pd.read_csv(CSV_PATH)
df = df[["kernel","build","dtype","M","stride","align","misalign_bytes","time_s"]].copy()

def vector_len(dtype: str) -> int:
    if dtype == "f32":
        return 8
    elif dtype == "f64":
        return 4
    return 8

vl = vector_len(df["dtype"].iloc[0])
tail = (df["M"] % vl != 0)

align_map = {"aligned": "alignment", "misaligned": "misalignment"}
df["align_label"] = df["align"].map(align_map) + np.where(tail, " + tail", " + no tail")

FLOPs_per_elem = 2.0  
grp = (
    df.groupby(["kernel","build","align_label","M"], as_index=False)
      .agg(mean_time=("time_s","mean"))
)
grp["GFLOPs"] = (FLOPs_per_elem * grp["M"] / grp["mean_time"]) / 1e9
grp["GFLOPs"] = grp["GFLOPs"].round(2)

table = grp[["kernel","build","align_label","M","GFLOPs"]].rename(
    columns={"align_label":"align"}
)

align_order = ["alignment + no tail", "alignment + tail",
               "misalignment + no tail", "misalignment + tail"]
table["align"] = pd.Categorical(table["align"], categories=align_order, ordered=True)
table = table.sort_values(by=["kernel","build","align","M"]).reset_index(drop=True)

table.to_csv(OUT_CSV, index=False)

tbl_for_draw = table.copy()
tbl_for_draw["M"] = tbl_for_draw["M"].astype(str)
tbl_for_draw["GFLOPs"] = tbl_for_draw["GFLOPs"].map(lambda x: f"{x:.2f}")

def wrap(s, width=18):
    return "\n".join(textwrap.wrap(str(s), width=width))
tbl_for_draw["align"] = tbl_for_draw["align"].map(lambda s: wrap(s, width=20))

n_rows = len(tbl_for_draw)
row_height = 0.35           
fig_height = max(2.0, n_rows * row_height)
fig_width = 9.5          

fig, ax = plt.subplots(figsize=(fig_width, fig_height))
ax.axis('off')

col_labels = list(tbl_for_draw.columns)
cell_text  = tbl_for_draw.values

colWidths = [0.15, 0.12, 0.30, 0.18, 0.15]

the_table = ax.table(
    cellText=cell_text,
    colLabels=col_labels,
    colLoc='center',
    cellLoc='center',
    colWidths=colWidths,
    loc='center'
)

the_table.auto_set_font_size(False)
the_table.set_fontsize(10)
the_table.scale(1.0, 1.3)  

for (row, col), cell in the_table.get_celld().items():
    if row == 0:  
        cell.set_text_props(weight='bold')

        cell.set_facecolor('#f2f2f2')

plt.tight_layout()


os.makedirs(os.path.dirname(OUT_PNG) or ".", exist_ok=True)
plt.savefig(OUT_PNG, dpi=300, bbox_inches='tight')
print(f"Saved table image to: {OUT_PNG}")

