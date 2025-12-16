#!/usr/bin/env bash
set -euo pipefail

OUT_CSV="${1:-exp2_metrics.csv}"

BIN_BASELINE="./baseline"
BIN_SIMD="./simd"
BIN_THREAD="./thread"
BIN_SIMD_THREAD="./simd_thread"

declare -A SHAPE_M SHAPE_K SHAPE_N
SHAPE_M["square"]=1024;      SHAPE_K["square"]=1024; SHAPE_N["square"]=1024
SHAPE_M["tall_skinny"]=4096; SHAPE_K["tall_skinny"]=256;  SHAPE_N["tall_skinny"]=1024
SHAPE_M["fat"]=256;          SHAPE_K["fat"]=4096; SHAPE_N["fat"]=1024

DENSITY="0.01"
REPEATS="3"
THREADS_LIST=(1 2 4 6 8 12)

CPU_MHZ="4691.105"
CPU_HZ="$(awk -v mhz="${CPU_MHZ}" 'BEGIN{printf "%.6f", mhz*1e6}')"  

echo "kernel,shape,variant,threads,time_sec,gflops,cpnz" > "${OUT_CSV}"

parse_and_write() {
  local shape="$1"
  local variant="$2"
  local threads="$3"
  local bin="$4"

  local m="${SHAPE_M[$shape]}"
  local k="${SHAPE_K[$shape]}"
  local n="${SHAPE_N[$shape]}"

  local cmd=("${bin}" "${m}" "${k}" "${n}" "${DENSITY}" "${REPEATS}")
  if [[ "${variant}" == "thread" || "${variant}" == "simd_thread" ]]; then
    cmd+=("${threads}")
  fi

  local out
  out="$("${cmd[@]}")"

  local gemm_t gemm_gflops spmm_t nnz
  gemm_t="$(awk -F'=' '/GEMM avg runtime/ {gsub(/[[:space:]]*s[[:space:]]*$/, "", $2); gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2}' <<< "${out}" | head -n1)"
  gemm_gflops="$(awk -F'=' '/GFLOP\/s/ {gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2}' <<< "${out}" | head -n1)"
  spmm_t="$(awk -F'=' '/SpMM avg runtime/ {gsub(/[[:space:]]*s[[:space:]]*$/, "", $2); gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2}' <<< "${out}" | head -n1)"
  nnz="$(awk -F'=' '/^nnz/ {gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2}' <<< "${out}" | head -n1)"

  local spmm_gflops cpnz
  spmm_gflops="$(awk -v nnz="${nnz}" -v n="${n}" -v t="${spmm_t}" 'BEGIN{ if(t>0 && nnz>0) printf "%.6f", (2.0*nnz*n)/t/1e9; else printf "" }')"
  cpnz="$(awk -v t="${spmm_t}" -v hz="${CPU_HZ}" -v nnz="${nnz}" 'BEGIN{ if(t>0 && nnz>0) printf "%.6f", (t*hz)/nnz; else printf "" }')"

  echo "gemm,${shape},${variant},${threads},${gemm_t},${gemm_gflops}," >> "${OUT_CSV}"
  echo "spmm,${shape},${variant},${threads},${spmm_t},${spmm_gflops},${cpnz}" >> "${OUT_CSV}"
}

for shape in square tall_skinny fat; do
  parse_and_write "${shape}" "baseline" 1 "${BIN_BASELINE}"
  parse_and_write "${shape}" "simd" 1 "${BIN_SIMD}"

  for th in "${THREADS_LIST[@]}"; do
    parse_and_write "${shape}" "thread" "${th}" "${BIN_THREAD}"
    parse_and_write "${shape}" "simd_thread" "${th}" "${BIN_SIMD_THREAD}"
  done
done

echo "Saved: ${OUT_CSV}"
