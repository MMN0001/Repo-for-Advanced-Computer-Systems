import pandas as pd
import matplotlib.pyplot as plt
import os
from io import StringIO

csv_text = """kernel,build,align,M,GFLOPs
saxpy,scalar,alignment + no tail,131072,6.99
saxpy,scalar,alignment + no tail,4194304,6.84
saxpy,scalar,alignment + tail,131075,6.94
saxpy,scalar,alignment + tail,4194307,6.84
saxpy,scalar,misalignment + no tail,131072,6.64
saxpy,scalar,misalignment + no tail,4194304,6.49
saxpy,scalar,misalignment + tail,131075,6.57
saxpy,scalar,misalignment + tail,4194307,6.53
saxpy,simd,alignment + no tail,131072,25.23
saxpy,simd,alignment + no tail,4194304,13.73
saxpy,simd,alignment + tail,131075,25.04
saxpy,simd,alignment + tail,4194307,13.69
saxpy,simd,misalignment + no tail,131072,24.25
saxpy,simd,misalignment + no tail,4194304,12.33
saxpy,simd,misalignment + tail,131075,24.11
saxpy,simd,misalignment + tail,4194307,12.42
"""
df = pd.read_csv(StringIO(csv_text))

def m_bucket(m: int) -> str:
    if m in (131072, 131075):
        return "L2-size (131072/131075)"
    elif m in (4194304, 4194307):
        return "L3-size (4194304/4194307)"
    else:
        return "Other"

df["M_bucket"] = df["M"].apply(m_bucket)

align_order = [
    "alignment + no tail",
    "alignment + tail",
    "misalignment + no tail",
    "misalignment + tail",
]
df["align"] = pd.Categorical(df["align"], categories=align_order, ordered=True)

out_dir = "/home/zhoun3/Repo-for-Advanced-Computer-Systems/figs"
os.makedirs(out_dir, exist_ok=True)

def save_table_as_png(data: pd.DataFrame, caption: str, out_file: str):
    data = data[["align", "M", "GFLOPs"]].sort_values(["align", "M"]).copy()
    data["M"] = data["M"].map("{:,}".format)
    data["GFLOPs"] = data["GFLOPs"].map("{:.2f}".format)

    fig, ax = plt.subplots(figsize=(6, 0.5*len(data) + 1))
    ax.axis('off')

    table = ax.table(
        cellText=data.values,
        colLabels=data.columns,
        cellLoc='center',
        colLoc='center',
        loc='center'
    )

    table.auto_set_font_size(False)
    table.set_fontsize(10)
    table.scale(1, 1.4)

    for key, cell in table.get_celld().items():
        if key[0] == 0:
            cell.set_text_props(weight='bold')
            cell.set_facecolor('#f2f2f2')

    plt.title(caption, fontsize=12, weight='bold')
    plt.tight_layout()

    plt.savefig(out_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Saved: {out_file}")

for build in ["scalar", "simd"]:
    for bucket in ["L2-size (131072/131075)", "L3-size (4194304/4194307)"]:
        sub = df[(df["build"] == build) & (df["M_bucket"] == bucket)]
        if not sub.empty:
            caption = f"{build.upper()} — {bucket}"
            fname = f"{build}_{bucket.replace(' ', '_').replace('(', '').replace(')', '').replace('/', '-')}.png"
            out_path = os.path.join(out_dir, fname)
            save_table_as_png(sub, caption, out_path)
