#!/usr/bin/env bash
set -euo pipefail

BASELINE_BIN="${1:-./hash_baseline}"
FINE_BIN="${2:-./hash_fine_grained}"
OUT_CSV="${3:-group2_nkeys_sensitivity_t12.csv}"

THREADS="${THREADS:-12}"
DURATION_MS="${DURATION_MS:-2000}"
REPEAT="${REPEAT:-3}"
NKEYS_LIST_STR="${NKEYS_LIST:-10000 100000 1000000}"

PREFILL_LK="${PREFILL_LK:-1}"   
PREFILL_INS="${PREFILL_INS:-0}" 

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

get_ops () { awk -F',' '{print $8}'; }

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

    for nkeys in $NKEYS_LIST_STR; do
      for rep in $(seq 1 "$REPEAT"); do
        raw1="$(run_bench "$bin" "$wl" "$nkeys" 1 "$DURATION_MS" "$prefill")"
        ops1="$(printf "%s\n" "$raw1" | get_ops)"
        echo "${impl},${wl},${nkeys},1,${rep},${DURATION_MS},${prefill},${ops1},1.000,\"${raw1}\"" >> "$OUT_CSV"

        raw="$(run_bench "$bin" "$wl" "$nkeys" "$THREADS" "$DURATION_MS" "$prefill")"
        ops="$(printf "%s\n" "$raw" | get_ops)"
        spd="$(speedup "$ops" "$ops1")"
        echo "${impl},${wl},${nkeys},${THREADS},${rep},${DURATION_MS},${prefill},${ops},${spd},\"${raw}\"" >> "$OUT_CSV"
      done
    done
  done
done

echo "Done. Results saved to: $OUT_CSV"
echo "Settings: THREADS=$THREADS DURATION_MS=$DURATION_MS REPEAT=$REPEAT NKEYS_LIST=($NKEYS_LIST_STR)"
