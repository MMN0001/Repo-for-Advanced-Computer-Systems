#!/usr/bin/env bash
set -euo pipefail

FILE="${1:-test.dat}"
RUNTIME="${RUNTIME:-30}"
BS="${BS:-4k}"
OUTDIR="${OUTDIR:-results_affinity_c16}"

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


#  no pin: OS decides
#  1 physical core + SMT sibling: 0,6
#  2 physical cores + SMT siblings: 0,1,6,7
declare -A PIN
PIN["nopin"]=""
PIN["core1"]="0,6"
PIN["core2"]="0,1,6,7"

JOBS="${JOBS:-4}"
IODEPTH=$(( (C + JOBS - 1) / JOBS ))   
TOTAL_INFLIGHT=$(( JOBS * IODEPTH ))

echo "CPU Affinity Experiment (Concurrency=$C)"
echo "file=$FILE bs=$BS runtime=${RUNTIME}s"
echo "async: JOBS=$JOBS iodepth=$IODEPTH total_inflight=$TOTAL_INFLIGHT"
echo "Outputs -> $OUTDIR/*.json"
echo

run_fio () {
  local tag="$1"       
  local cpulist="$2"   

  local PREFIX=()
  if [[ -n "$cpulist" ]]; then
    PREFIX=(taskset -c "$cpulist")
  fi

  echo "=== [$tag] SYNC: psync numjobs=$C iodepth=1 (cpus=${cpulist:-any}) ==="
  "${PREFIX[@]}" fio "${COMMON[@]}" \
    --name="sync_psync_c${C}_${tag}" \
    --ioengine=psync \
    --numjobs="$C" \
    --iodepth=1 \
    --output="$OUTDIR/sync_psync_c${C}_${tag}.json"

  echo "=== [$tag] ASYNC: io_uring numjobs=$JOBS iodepth=$IODEPTH (cpus=${cpulist:-any}) ==="
  "${PREFIX[@]}" fio "${COMMON[@]}" \
    --name="async_uring_c${C}_${tag}" \
    --ioengine=io_uring \
    --numjobs="$JOBS" \
    --iodepth="$IODEPTH" \
    --iodepth_batch_submit="$IODEPTH" \
    --iodepth_batch_complete="$IODEPTH" \
    --output="$OUTDIR/async_uring_c${C}_${tag}.json"

  echo
}

run_fio "nopin" "${PIN[nopin]}"
run_fio "core1" "${PIN[core1]}"
run_fio "core2" "${PIN[core2]}"

echo "Done."
