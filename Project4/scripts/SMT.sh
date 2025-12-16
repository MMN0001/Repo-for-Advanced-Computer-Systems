#!/usr/bin/env bash
set -euo pipefail

FILE="${1:-test.dat}"
RUNTIME="${RUNTIME:-30}"
BS="${BS:-4k}"
OUTDIR="${OUTDIR:-results_smt_c16}"

mkdir -p "$OUTDIR"

C=16

COMMON=(
  "--filename=$FILE"
  "--rw=randread"
  "--bs=$BS"
  "--direct=1"
  "--time_based=1"
  "--runtime=$RUNTIME"
  "--group_reporting=1"
  "--randrepeat=0"
  "--norandommap=1"
  "--output-format=json"
)

declare -A PIN
PIN["smt_on"]="0-11"
PIN["smt_off"]="0-5"

JOBS="${JOBS:-4}"
IODEPTH=$(( (C + JOBS - 1) / JOBS ))  
TOTAL_INFLIGHT=$(( JOBS * IODEPTH ))

echo "SMT Experiment (Concurrency=$C)"
echo "file=$FILE bs=$BS runtime=${RUNTIME}s"
echo "async: JOBS=$JOBS iodepth=$IODEPTH total_inflight=$TOTAL_INFLIGHT"
echo "Outputs -> $OUTDIR/*.json"
echo

run_one () {
  local tag="$1"      
  local cpulist="$2"  

  echo "=== [$tag] CPUs=$cpulist : SYNC(psync) numjobs=$C iodepth=1 ==="
  taskset -c "$cpulist" fio "${COMMON[@]}" \
    --name="sync_psync_c${C}_${tag}" \
    --ioengine=psync \
    --numjobs="$C" \
    --iodepth=1 \
    --output="$OUTDIR/sync_psync_c${C}_${tag}.json"

  echo "=== [$tag] CPUs=$cpulist : ASYNC(io_uring) numjobs=$JOBS iodepth=$IODEPTH ==="
  taskset -c "$cpulist" fio "${COMMON[@]}" \
    --name="async_uring_c${C}_${tag}" \
    --ioengine=io_uring \
    --numjobs="$JOBS" \
    --iodepth="$IODEPTH" \
    --iodepth_batch_submit="$IODEPTH" \
    --iodepth_batch_complete="$IODEPTH" \
    --output="$OUTDIR/async_uring_c${C}_${tag}.json"

  echo
}

run_one "smt_on"  "${PIN[smt_on]}"
run_one "smt_off" "${PIN[smt_off]}"

echo "Done."
