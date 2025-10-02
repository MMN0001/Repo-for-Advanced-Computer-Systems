#!/usr/bin/env bash
set -euo pipefail

BIN=./stride_sweep
SIZE=256M
ITERS=64
STRIDES=(32 64 128 256 512 1024 2048)
PATTERNS=(rand_read seq_read seq_write rand_write)

OUT="stride.csv"

echo "pattern,stride,size_B,iters,ops,avg_ns,bw_MBps" > "$OUT"

for p in "${PATTERNS[@]}"; do
  for st in "${STRIDES[@]}"; do
    "$BIN" --pattern "$p" --size "$SIZE" --stride "$st" --iters "$ITERS" >> "$OUT"
  done
done

echo "Saved -> $OUT"
