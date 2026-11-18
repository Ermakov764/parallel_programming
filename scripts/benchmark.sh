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
has_baseline=false
for p in $threads; do
  [[ "$p" =~ ^[1-9][0-9]*$ ]] || { echo 'THREADS must contain positive integers' >&2; exit 1; }
  [[ "$p" != 1 ]] || has_baseline=true
done
[[ "$has_baseline" == true ]] || { echo 'THREADS must include 1' >&2; exit 1; }
mkdir -p benchmark-output
root=$(mktemp -d benchmark-output/run-XXXXXXXX)
csv="$root/raw.csv"
printf 'repeat,mode,threads,images,read_ms,process_ms,write_ms,total_ms\n' > "$csv"
echo "Results: $root" >&2
for mode in file-first stage-first pipeline; do
  # An untimed warmup per mode and worker count; each run has fresh output files.
  for p in $threads; do
    for ((r=0; r<=repeats; r++)); do
      destination="$root/${mode}-${p}-${r}"
      row=$("$binary" --batch "$dataset" "$destination" "$mode" "$p" --csv)
      if ((r > 0)); then printf '%s,%s\n' "$r" "$row" >> "$csv"; fi
      # Delete only the known, generated output directory to keep disk use bounded.
      rm -f -- "$destination"/*.[bB][mM][pP]
      rmdir -- "$destination"
    done
  done
done
"${PYTHON:-python}" scripts/summarize_benchmark.py "$csv"
printf 'Dataset: %s\nBinary: %s\nRepeats: %s\nThreads: %s\n' "$dataset" "$binary" "$repeats" "$threads" >> "$root/environment.txt"
if command -v sha256sum >/dev/null 2>&1; then sha256sum "$binary" >> "$root/environment.txt"; fi
