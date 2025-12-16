#!/usr/bin/env bash
set -euo pipefail

DIR="${1:-results_smt_c16}"
OUT="${2:-smt_c16.csv}"

echo "mode,smt,concurrency,read_iops,read_bw_kib,clat_p99_us" > "$OUT"

extract_one () {
  local file="$1"
  local mode="$2"
  local smt="$3"
  local c="$4"

  [[ -f "$file" ]] || { echo "ERROR: file not found: $file" >&2; exit 1; }

  local iops bw p99ns p99us
  iops=$(jq -r '.jobs[0].read.iops' "$file")
  bw=$(jq -r '.jobs[0].read.bw' "$file")
  p99ns=$(jq -r '.jobs[0].read.clat_ns.percentile["99.000000"]' "$file")
  [[ "$p99ns" != "null" && -n "$p99ns" ]] || { echo "ERROR: missing p99 in $file" >&2; exit 1; }

  p99us=$(awk -v ns="$p99ns" 'BEGIN{printf "%.3f", ns/1000.0}')
  echo "$mode,$smt,$c,$iops,$bw,$p99us" >> "$OUT"
}

C=16
for s in smt_on smt_off; do
  extract_one "$DIR/sync_psync_c${C}_${s}.json"  "sync_psync"  "$s" "$C"
  extract_one "$DIR/async_uring_c${C}_${s}.json" "async_uring" "$s" "$C"
done

echo "Wrote $OUT"
