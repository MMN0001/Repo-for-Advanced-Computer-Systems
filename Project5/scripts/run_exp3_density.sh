#!/usr/bin/env bash
set -euo pipefail

BIN="${1:-./simd_thread}"
OUT_CSV="${2:-exp3_density.csv}"

M=4096
K=4096
N=4096
THREADS=1
REPEATS=3

DENSITIES=(0.001 0.002 0.005 0.01 0.02 0.05 0.10 0.20 0.30 0.50 0.8)

echo "density,gemm_time_sec,gemm_gflops,spmm_time_sec,spmm_gflops,nnz" > "${OUT_CSV}"

for p in "${DENSITIES[@]}"; do
  out="$("${BIN}" "${M}" "${K}" "${N}" "${p}" "${REPEATS}" "${THREADS}")"

  gemm_t="$(awk -F'=' '/GEMM avg runtime/ {gsub(/[[:space:]]*s[[:space:]]*$/, "", $2); gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2}' <<< "${out}" | head -n1)"
  gemm_gflops="$(awk -F':' '/GFLOP\/s/ {gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2}' <<< "${out}" | head -n1)"
  if [[ -z "${gemm_gflops}" ]]; then
    gemm_gflops="$(awk -F'=' '/GFLOP\/s/ {gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2}' <<< "${out}" | head -n1)"
  fi

  spmm_t="$(awk -F'=' '/SpMM avg runtime/ {gsub(/[[:space:]]*s[[:space:]]*$/, "", $2); gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2}' <<< "${out}" | head -n1)"
  nnz="$(awk -F'=' '/^nnz/ {gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2}' <<< "${out}" | head -n1)"

  spmm_gflops="$(python3 - <<PY
t=float("${spmm_t}")
nnz=int("${nnz}")
n=int("${N}")
flops=2.0*nnz*n
print("{:.6f}".format((flops/t)/1e9 if t>0 else 0.0))
PY
)"

  echo "${p},${gemm_t},${gemm_gflops},${spmm_t},${spmm_gflops},${nnz}" >> "${OUT_CSV}"
done

echo "Saved: ${OUT_CSV}"
