#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/research.conf
export THREADS REPEATS MEASURE_CPU
if [[ -z ${PYTHON:-} ]]; then
  if command -v python3 >/dev/null; then PYTHON=python3; else PYTHON=python; fi
fi
export PYTHON
case "$(uname -s)" in
  MINGW*|MSYS*) cmd.exe //c build.cmd; export BINARY=./filter.exe ;;
  *) make; export BINARY=./filter ;;
esac

mkdir -p results/runs benchmark-data
run=$(mktemp -d results/runs/run-XXXXXXXX)
data=$(mktemp -d benchmark-data/study-XXXXXXXX)
# Удаляем только временный набор, созданный этим запуском.
cleanup() { "$PYTHON" -c 'import shutil,sys; shutil.rmtree(sys.argv[1])' "$data"; }
trap cleanup EXIT
cp scripts/research.conf "$run/settings.conf"

measure() {
  local label=$1 count=$2 width=$3 height=$4 modes=$5
  "$PYTHON" scripts/generate_dataset.py "$data/$label" --count "$count" --width "$width" --height "$height" --seed "$SEED"
  local summary
  summary=$(MODES="$modes" bash scripts/benchmark.sh "$data/$label")
  mkdir "$run/$label"
  cp "$(dirname "$summary")"/{summary.csv,raw.csv,environment.txt} "$run/$label/"
}
measure batch "$BATCH_COUNT" "$BATCH_WIDTH" "$BATCH_HEIGHT" "file-first stage-first pipeline"
for size in $FRAME_SIZES; do
  [[ "$size" =~ ^([1-9][0-9]*)x([1-9][0-9]*)$ ]] || { echo "Invalid size: $size" >&2; exit 1; }
  measure "$size" 1 "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" file-first
done
"$PYTHON" scripts/research_report.py "$run" "$BATCH_COUNT" "${BATCH_WIDTH}x${BATCH_HEIGHT}" "$SEED" "$REPEATS" $FRAME_SIZES
