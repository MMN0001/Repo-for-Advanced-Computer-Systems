#!/usr/bin/env bash
set -euo pipefail

DIR="${1:-results_baseline}"
OUT="${2:-baseline.csv}"

echo "mode,concurrency,read_iops,read_bw_kib,clat_p99_us" > "$OUT"

extract_one () {
  local f="$1"
  local mode="$2"
  local c="$3"

  if [[ ! -f "$f" ]]; then
    echo "ERROR: file not found: $f" >&2
    exit 1
  fi

  local iops bw p99ns p99us
  iops=$(jq -r '.jobs[0].read.iops' "$f")
  bw=$(jq -r '.jobs[0].read.bw' "$f")
  p99ns=$(jq -r '.jobs[0].read.clat_ns.percentile["99.000000"]' "$f")

  if [[ "$p99ns" == "null" || -z "$p99ns" ]]; then
    echo "ERROR: missing p99 percentile in $f" >&2
    exit 1
  fi

  p99us=$(awk -v ns="$p99ns" 'BEGIN{printf "%.3f", ns/1000.0}')
  echo "$mode,$c,$iops,$bw,$p99us" >> "$OUT"
}

for c in 1 4 16 64; do
  extract_one "$DIR/sync_psync_c${c}.json"  "sync_psync" "$c"
  extract_one "$DIR/async_uring_c${c}.json" "async_uring" "$c"
done

echo "Wrote $OUT"
