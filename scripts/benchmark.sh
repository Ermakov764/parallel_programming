#!/usr/bin/env bash
set -euo pipefail
# Run from any directory. Paths may contain spaces and Unicode.
cd "$(dirname "$0")/.."
binary=${BINARY:-./filter}
[[ -x "$binary" ]] || binary=./filter.exe
[[ -x "$binary" ]] || { echo 'Build the application first' >&2; exit 1; }
dataset=${1:-benchmark-data/fullhd}
repeats=${REPEATS:-5}
threads=${THREADS:-"1 2 4 8"}
[[ "$repeats" =~ ^[1-9][0-9]*$ ]] || { echo 'REPEATS must be positive' >&2; exit 1; }
mkdir -p benchmark-output
root=$(mktemp -d benchmark-output/run-XXXXXXXX)
csv="$root/raw.csv"
printf 'repeat,mode,threads,images,read_ms,process_ms,write_ms,total_ms\n' > "$csv"
echo "Results: $root" >&2
for mode in image stage pipeline; do
  # An untimed warmup per mode and worker count; each run has fresh output files.
  for p in $threads; do
    for ((r=0; r<=repeats; r++)); do
      destination="$root/${mode}-${p}-${r}"
      row=$("$binary" --batch "$dataset" "$destination" "$mode" "$p" --csv)
      if ((r > 0)); then printf '%s,%s\n' "$r" "$row" >> "$csv"; fi
      # Delete only the known, generated output directory to keep disk use bounded.
      rm -f -- "$destination"/*.bmp
      rmdir -- "$destination"
    done
  done
done
"${PYTHON:-python}" scripts/summarize_benchmark.py "$csv"
