"""Summarize repeated timings using medians, speedup and efficiency."""
import csv
import platform
import statistics
import sys
from datetime import datetime, timezone
from pathlib import Path

source = Path(sys.argv[1])
with source.open(newline='') as stream:
    rows = list(csv.DictReader(stream))
groups = {}
for row in rows:
    # Earlier benchmark runs used image and stage as mode names.
    mode = {'image': 'file-first', 'stage': 'stage-first'}.get(row['mode'], row['mode'])
    groups.setdefault((mode, int(row['threads'])), []).append(row)
baseline = groups.get(('file-first', 1))
if not baseline:
    raise SystemExit('THREADS must include 1 for the sequential baseline')
output = source.with_name('summary.csv')
with output.open('w', newline='') as stream:
    writer = csv.writer(stream)
    writer.writerow(['mode', 'threads', 'scope', 'median_ms', 'min_ms', 'max_ms',
                     'speedup', 'efficiency', 'images_per_second'])
    for (mode, threads), samples in sorted(groups.items()):
        for scope in ('process_ms', 'total_ms'):
            values = [float(row[scope]) for row in samples]
            median = statistics.median(values)
            base = statistics.median(float(row[scope]) for row in baseline)
            speedup = base / median
            writer.writerow([mode, threads, scope, round(median, 3), min(values), max(values),
                             round(speedup, 4), round(speedup / threads, 4),
                             round(int(samples[0]['images']) * 1000 / median, 3)])
source.with_name('environment.txt').write_text(
    f'UTC: {datetime.now(timezone.utc).isoformat()}\nPlatform: {platform.platform()}\n'
    f'CPU: {platform.processor()}\nPython: {platform.python_version()}\n'
    'Build: optimized release; see Makefile / build.cmd.\n'
    'Baseline: file-first mode, 1 thread. Warm filesystem cache; startup excluded.\n', encoding='utf-8')
print(output.as_posix())
