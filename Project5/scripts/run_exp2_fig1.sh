#!/usr/bin/env bash
set -euo pipefail

BASELINE=./baseline
SIMD=./simd
THREAD=./thread
SIMD_THREAD=./simd_thread

density=0.01
repeats=3

threads_list=(1 2 4 6 8 12)

# shapes: name,m,k,n
shapes=(
  "square,1024,1024,1024"
  "tall_skinny,4096,1024,256"
  "fat,256,1024,4096"
)

out="exp2_fig1_raw.csv"
echo "kernel,shape,variant,threads,time_sec" > "$out"

extract_gemm() { awk -F'=' '/GEMM avg runtime/ {gsub(/ s/,"",$2); gsub(/^[ \t]+/,"",$2); print $2; exit}'; }
extract_spmm() { awk -F'=' '/SpMM avg runtime/ {gsub(/ s/,"",$2); gsub(/^[ \t]+/,"",$2); print $2; exit}'; }

run_one () {
  local exe=$1 m=$2 k=$3 n=$4 d=$5 r=$6 t=$7
  if [[ "$t" == "" ]]; then
    "$exe" "$m" "$k" "$n" "$d" "$r"
  else
    "$exe" "$m" "$k" "$n" "$d" "$r" "$t"
  fi
}

for s in "${shapes[@]}"; do
  IFS=',' read -r shape m k n <<< "$s"
  echo "=== shape=$shape (m=$m k=$k n=$n) ==="

  # baseline/simd only need threads=1 once per shape
  out_base=$(run_one "$BASELINE" "$m" "$k" "$n" "$density" "$repeats" "")
  t_gemm=$(echo "$out_base" | extract_gemm)
  t_spmm=$(echo "$out_base" | extract_spmm)
  echo "gemm,$shape,baseline,1,$t_gemm" >> "$out"
  echo "spmm,$shape,baseline,1,$t_spmm" >> "$out"

  out_simd=$(run_one "$SIMD" "$m" "$k" "$n" "$density" "$repeats" "")
  t_gemm=$(echo "$out_simd" | extract_gemm)
  t_spmm=$(echo "$out_simd" | extract_spmm)
  echo "gemm,$shape,simd,1,$t_gemm" >> "$out"
  echo "spmm,$shape,simd,1,$t_spmm" >> "$out"

  for T in "${threads_list[@]}"; do
    echo "  threads=$T"

    out_thr=$(run_one "$THREAD" "$m" "$k" "$n" "$density" "$repeats" "$T")
    t_gemm=$(echo "$out_thr" | extract_gemm)
    t_spmm=$(echo "$out_thr" | extract_spmm)
    echo "gemm,$shape,thread,$T,$t_gemm" >> "$out"
    echo "spmm,$shape,thread,$T,$t_spmm" >> "$out"

    out_st=$(run_one "$SIMD_THREAD" "$m" "$k" "$n" "$density" "$repeats" "$T")
    t_gemm=$(echo "$out_st" | extract_gemm)
    t_spmm=$(echo "$out_st" | extract_spmm)
    echo "gemm,$shape,simd_thread,$T,$t_gemm" >> "$out"
    echo "spmm,$shape,simd_thread,$T,$t_spmm" >> "$out"
  done
done

echo "Saved: $out"
