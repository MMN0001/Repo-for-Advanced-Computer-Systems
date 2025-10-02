#!/usr/bin/env bash
set -euo pipefail

OUT=locality.csv
: > "$OUT"   

run_one () {
  local kernel=$1 dtype=$2 M=$3
  ./kernel_scalar --kernel "$kernel" --dtype "$dtype" --stride 1 --M "$M" --repeats 10 --align aligned --csv "$OUT"
  ./kernel_simd   --kernel "$kernel" --dtype "$dtype" --stride 1 --M "$M" --repeats 10 --align aligned --csv "$OUT"
}
# SAXPY / DOT : footprint ≈ 2 * M * sizeof(dtype)
# f32 points: [0.5K, 1K, 30K, 64K, 90K, 1M, 2M, 5M]
F32_SAXPY_DOT=(512 1024 30720 65536 92160 1048576 2097152 5242880)
# f64 points: [0.5K, 0.8K, 5K, 20K, 50K, 80K, 2M, 5M]
F64_SAXPY_DOT=(512 819 5120 20480 51200 81920 2097152 5242880)

# ELEMENTWISE : footprint ≈ 3 * M * sizeof(dtype)
# f32 points: [1K, 1.5K, 32K, 44K, 64K, 1M, 1.4M, 5M]
F32_ELEMUL=(1024 1536 32768 45056 65536 1048576 1468006 5242880)
# f64 points: [0.5K, 0.7K, 16K, 22K, 32K, 0.7M, 1M, 5M]
F64_ELEMUL=(512 716 16384 22528 32768 734003 1048576 5242880)

echo "[INFO] Running SAXPY (f32/f64)…"
for M in "${F32_SAXPY_DOT[@]}"; do run_one saxpy f32 "$M"; done
for M in "${F64_SAXPY_DOT[@]}"; do run_one saxpy f64 "$M"; done

echo "[INFO] Running DOT (f32/f64)…"
for M in "${F32_SAXPY_DOT[@]}"; do run_one dot f32 "$M"; done
for M in "${F64_SAXPY_DOT[@]}"; do run_one dot f64 "$M"; done

echo "[INFO] Running ELEMUL (f32/f64)…"
for M in "${F32_ELEMUL[@]}"; do run_one elemul f32 "$M"; done
for M in "${F64_ELEMUL[@]}"; do run_one elemul f64 "$M"; done

echo "[DONE] Results -> $OUT"
