#!/usr/bin/env bash
set -euo pipefail

BIN=./simd_thread

DENS=0.01
REPEATS=3     
TH=6
SAMPLES=25    

Ns=(64 128 192 256 320 384 512 768 1024 1536 1792 2048 3072 4096)

BW_STREAM=${BW_STREAM:-27.377}

mkdir -p logs

SAMPLES_CSV=ws_samples.csv
SUMMARY_CSV=ws_summary.csv

echo "N,density,threads,repeats,sample_id,gemm_time_s,spmm_time_s,nnz,bw_stream_GBs" > "${SAMPLES_CSV}"

export OMP_NUM_THREADS="${TH}"
export OMP_PROC_BIND=true
export OMP_PLACES=cores

for N in "${Ns[@]}"; do
  echo "== N=${N} =="

  for s in $(seq 1 "${SAMPLES}"); do
    OUT="logs/out_N${N}_s${s}.txt"
    "${BIN}" "${N}" "${N}" "${N}" "${DENS}" "${REPEATS}" "${TH}" > "${OUT}"

    gemm_t=$(awk '/GEMM avg runtime/ {print $5}' "${OUT}")
    spmm_t=$(awk '/SpMM avg runtime/ {print $5}' "${OUT}")
    nnz=$(awk '/^nnz[[:space:]]+/ {print $3}' "${OUT}")

    if [[ -z "${gemm_t}" || -z "${spmm_t}" || -z "${nnz}" ]]; then
      echo "ERROR: parse failed for N=${N}, sample=${s}. Check ${OUT}."
      exit 1
    fi

    echo "${N},${DENS},${TH},${REPEATS},${s},${gemm_t},${spmm_t},${nnz},${BW_STREAM}" >> "${SAMPLES_CSV}"
  done
done

echo "N,density,threads,repeats,SAMPLES,bw_stream_GBs,gemm_gflops_mean,gemm_gflops_p05,gemm_gflops_p95,gemm_bw_mean,gemm_bw_p05,gemm_bw_p95,spmm_gflops_mean,spmm_gflops_p05,spmm_gflops_p95,spmm_bw_mean,spmm_bw_p05,spmm_bw_p95" > "${SUMMARY_CSV}"

summarize_list () {
 
  sort -n | awk '
    {a[NR]=$1}
    END{
      n=NR;
      if(n==0){exit 1}
      # mean
      s=0; for(i=1;i<=n;i++) s+=a[i];
      mean=s/n;

      # nearest-rank percentiles
      p05_idx=int(ceil(0.05*n)); if(p05_idx<1)p05_idx=1;
      p95_idx=int(ceil(0.95*n)); if(p95_idx<1)p95_idx=1;
      p05=a[p05_idx]; p95=a[p95_idx];

      printf("%.6f %.6f %.6f\n", mean, p05, p95);
    }
    function ceil(x){ return (x==int(x))?x:int(x)+1 }
  '
}

for N in "${Ns[@]}"; do
  gemm_gflops_stats=$(
    awk -F, -v N="${N}" '($1==N){t=$6; val=(2.0*N*N*N)/(t*1e9); print val}' "${SAMPLES_CSV}" | summarize_list
  )
  gemm_bw_stats=$(
    awk -F, -v N="${N}" '($1==N){t=$6; val=(16.0*N*N)/(t*1e9); print val}' "${SAMPLES_CSV}" | summarize_list
  )
  spmm_gflops_stats=$(
    awk -F, -v N="${N}" '($1==N){t=$7; nnz=$8; val=(2.0*nnz*N)/(t*1e9); print val}' "${SAMPLES_CSV}" | summarize_list
  )
  spmm_bw_stats=$(
    awk -F, -v N="${N}" '($1==N){t=$7; nnz=$8; val=(8.0*nnz + 12.0*N*N)/(t*1e9); print val}' "${SAMPLES_CSV}" | summarize_list
  )

  read gemm_g_mean gemm_g_p05 gemm_g_p95 <<< "${gemm_gflops_stats}"
  read gemm_b_mean gemm_b_p05 gemm_b_p95 <<< "${gemm_bw_stats}"
  read spmm_g_mean spmm_g_p05 spmm_g_p95 <<< "${spmm_gflops_stats}"
  read spmm_b_mean spmm_b_p05 spmm_b_p95 <<< "${spmm_bw_stats}"

  echo "${N},${DENS},${TH},${REPEATS},${SAMPLES},${BW_STREAM},${gemm_g_mean},${gemm_g_p05},${gemm_g_p95},${gemm_b_mean},${gemm_b_p05},${gemm_b_p95},${spmm_g_mean},${spmm_g_p05},${spmm_g_p95},${spmm_b_mean},${spmm_b_p05},${spmm_b_p95}" >> "${SUMMARY_CSV}"
done

echo "Done."
echo "Raw samples:   ${SAMPLES_CSV}"
echo "Summary (p05/p95): ${SUMMARY_CSV}"
