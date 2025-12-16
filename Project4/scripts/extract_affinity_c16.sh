#!/usr/bin/env bash
set -euo pipefail

DIR="${1:-results_affinity_c16}"
OUT="${2:-affinity_c16.csv}"

echo "mode,affinity,concurrency,read_iops,read_bw_kib,clat_p99_us" > "$OUT"

extract_one () {
  local file="$1"
  local mode="$2"
  local affinity="$3"
  local c="$4"

  if [[ ! -f "$file" ]]; then
    echo "ERROR: file not found: $file" >&2
    exit 1
  fi

  local iops bw p99ns p99us
  iops=$(jq -r '.jobs[0].read.iops' "$file")
  bw=$(jq -r '.jobs[0].read.bw' "$file")  
  p99ns=$(jq -r '.jobs[0].read.clat_ns.percentile["99.000000"]' "$file")

  if [[ "$p99ns" == "null" || -z "$p99ns" ]]; then
    echo "ERROR: missing p99 latency in $file" >&2
    exit 1
  fi

  p99us=$(awk -v ns="$p99ns" 'BEGIN{printf "%.3f", ns/1000.0}')

  echo "$mode,$affinity,$c,$iops,$bw,$p99us" >> "$OUT"
}

C=16

for aff in nopin core1 core2; do
  extract_one "$DIR/sync_psync_c${C}_${aff}.json"  "sync_psync"  "$aff" "$C"
  extract_one "$DIR/async_uring_c${C}_${aff}.json" "async_uring" "$aff" "$C"
done

echo "Wrote $OUT"
