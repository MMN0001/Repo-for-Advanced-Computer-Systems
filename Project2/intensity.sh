#!/usr/bin/env bash
set -euo pipefail

OUT="intensity.csv"
echo "sizeB,strideB,iters,threads,streams,intensity,avg_ns,BW_GBps" > "$OUT"

for T in 1 2 4 8 12 16 32; do
  ./intensity --size 32M --stride 64 --iters 50000000 --threads $T --pin >> "$OUT"
done

echo "output saved: $OUT"
