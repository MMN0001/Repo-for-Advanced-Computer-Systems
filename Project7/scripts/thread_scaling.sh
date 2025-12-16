#!/usr/bin/env bash
set -euo pipefail

BASELINE_BIN="${1:-./hash_baseline}"
FINE_BIN="${2:-./hash_fine_grained}"
OUT_CSV="${3:-group1_thread_scaling.csv}"

NKEYS="${NKEYS:-100000}"          
DURATION_MS="${DURATION_MS:-2000}"
PREFILL_LK="${PREFILL_LK:-1}"     
PREFILL_INS="${PREFILL_INS:-0}"  
REPEAT="${REPEAT:-3}"
THREADS_STR="${THREADS:-1 2 4 8 12}"

WORKLOADS=("lookup" "insert" "mixed")

echo "impl,workload,nkeys,threads,repeat,duration_ms,prefill,ops_per_sec,speedup_vs_1t,raw_output" > "$OUT_CSV"

run_bench () {
  local bin="$1"
  local wl="$2"
  local nkeys="$3"
  local th="$4"
  local dur="$5"
  local prefill="$6"

  "$bin" --workload "$wl" --nkeys "$nkeys" --threads "$th" --duration_ms "$dur" --prefill "$prefill"
}

get_ops () {
  awk -F',' '{print $8}'
}

speedup () {
  local ops="$1"
  local ops1="$2"
  awk -v a="$ops" -v b="$ops1" 'BEGIN{ if (b>0) printf "%.3f", a/b; else print "nan"; }'
}

for impl in baseline fine; do
  bin="$BASELINE_BIN"
  [[ "$impl" == "fine" ]] && bin="$FINE_BIN"

  for wl in "${WORKLOADS[@]}"; do
    prefill="$PREFILL_LK"
    [[ "$wl" == "insert" ]] && prefill="$PREFILL_INS"

    for rep in $(seq 1 "$REPEAT"); do
      raw1="$(run_bench "$bin" "$wl" "$NKEYS" 1 "$DURATION_MS" "$prefill")"
      ops1="$(printf "%s\n" "$raw1" | get_ops)"
      echo "${impl},${wl},${NKEYS},1,${rep},${DURATION_MS},${prefill},${ops1},1.000,\"${raw1}\"" >> "$OUT_CSV"

      for th in $THREADS_STR; do
        [[ "$th" -eq 1 ]] && continue
        raw="$(run_bench "$bin" "$wl" "$NKEYS" "$th" "$DURATION_MS" "$prefill")"
        ops="$(printf "%s\n" "$raw" | get_ops)"
        spd="$(speedup "$ops" "$ops1")"
        echo "${impl},${wl},${NKEYS},${th},${rep},${DURATION_MS},${prefill},${ops},${spd},\"${raw}\"" >> "$OUT_CSV"
      done
    done
  done
done

echo "Done. Results saved to: $OUT_CSV"
echo "Default settings: NKEYS=$NKEYS DURATION_MS=$DURATION_MS REPEAT=$REPEAT THREADS=($THREADS_STR)"
echo "Tip: Plot ops_per_sec vs threads and speedup_vs_1t vs threads, comparing impl=baseline vs fine, per workload."
