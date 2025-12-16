#!/usr/bin/env bash
set -euo pipefail

FILE="${1:-test.dat}"
RUNTIME="${RUNTIME:-30}"
BS="${BS:-4k}"
OUTDIR="${OUTDIR:-results_baseline}"

mkdir -p "$OUTDIR"

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

# Total concurrency 
CONCS=(1 4 16 64)

JOBS="${JOBS:-4}"

echo "Running baseline on file=$FILE bs=$BS runtime=${RUNTIME}s"
echo "Using JOBS=$JOBS for both sync and async"
echo "Outputs -> $OUTDIR/*.json"

for c in "${CONCS[@]}"; do
  echo "=== SYNC: psync numjobs=$c iodepth=1 ==="
  fio "${COMMON[@]}" \
    --name="sync_psync_c${c}" \
    --ioengine=psync \
    --numjobs="$c" \
    --iodepth=1 \
    --output="$OUTDIR/sync_psync_c${c}.json"

  iodepth=$(( (c + JOBS - 1) / JOBS ))  
  total_inflight=$(( JOBS * iodepth ))

  echo "=== ASYNC: io_uring numjobs=$JOBS iodepth=$iodepth (total_inflight=$total_inflight) ==="
  fio "${COMMON[@]}" \
    --name="async_uring_c${c}_nj${JOBS}_qd${iodepth}" \
    --ioengine=io_uring \
    --numjobs="$JOBS" \
    --iodepth="$iodepth" \
    --iodepth_batch_submit="$iodepth" \
    --iodepth_batch_complete="$iodepth" \
    --output="$OUTDIR/async_uring_c${c}.json"
done

echo "Done."
