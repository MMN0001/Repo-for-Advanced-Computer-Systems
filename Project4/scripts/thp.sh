#!/usr/bin/env bash
set -euo pipefail

FILE="${1:-test.dat}"
RUNTIME="${RUNTIME:-30}"
BS="${BS:-128k}"
OUTDIR="${OUTDIR:-results_thp_c16}"
C=16

CPUS="${CPUS:-0-11}"

mkdir -p "$OUTDIR"

COMMON=(
  "--filename=$FILE"
  "--rw=randread"
  "--bs=$BS"
  "--direct=0"
  "--time_based=1"
  "--runtime=$RUNTIME"
  "--group_reporting=1"
  "--randrepeat=0"
  "--norandommap=1"
  "--output-format=json"
)

JOBS="${JOBS:-4}"
IODEPTH=$(( (C + JOBS - 1) / JOBS ))
TOTAL_INFLIGHT=$(( JOBS * IODEPTH ))

thp_path="/sys/kernel/mm/transparent_hugepage"
enabled_file="$thp_path/enabled"
defrag_file="$thp_path/defrag"

print_thp_status () {
  echo "THP enabled: $(cat "$enabled_file")"
  echo "THP defrag : $(cat "$defrag_file")"
}

set_thp () {
  local mode="$1"   
  echo "$mode" | sudo tee "$enabled_file" >/dev/null
  echo "never" | sudo tee "$defrag_file" >/dev/null
  print_thp_status
}

run_pair () {
  local tag="$1"  

  echo "=== [$tag] SYNC(psync) numjobs=$C iodepth=1 (cpus=$CPUS) ==="
  sudo -E taskset -c "$CPUS" fio "${COMMON[@]}" \
    --name="sync_psync_c${C}_${tag}" \
    --ioengine=psync \
    --numjobs="$C" \
    --iodepth=1 \
    --output="$OUTDIR/sync_psync_c${C}_${tag}.json"

  echo "=== [$tag] ASYNC(io_uring) numjobs=$JOBS iodepth=$IODEPTH total_inflight=$TOTAL_INFLIGHT (cpus=$CPUS) ==="
  sudo -E taskset -c "$CPUS" fio "${COMMON[@]}" \
    --name="async_uring_c${C}_${tag}" \
    --ioengine=io_uring \
    --numjobs="$JOBS" \
    --iodepth="$IODEPTH" \
    --iodepth_batch_submit="$IODEPTH" \
    --iodepth_batch_complete="$IODEPTH" \
    --output="$OUTDIR/async_uring_c${C}_${tag}.json"
}

echo "THP experiment: randread bs=$BS O_DIRECT runtime=${RUNTIME}s concurrency=$C"
echo "CPU affinity fixed: $CPUS"
echo "async: JOBS=$JOBS iodepth=$IODEPTH total_inflight=$TOTAL_INFLIGHT"
echo "Outputs -> $OUTDIR/*.json"
echo

echo "--- Condition A: THP OFF (4KB pages baseline) ---"
set_thp "never"
run_pair "thp_off"
echo

echo "--- Condition B: THP ON (huge pages) ---"
set_thp "always"
run_pair "thp_on"
echo

echo "Done."
