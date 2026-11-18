"""Publish a report only after all datasets have completed."""
import csv
import os
import platform
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from statistics import median

run = Path(sys.argv[1])
count, batch_size, seed, repeats, *sizes = sys.argv[2:]
cpu = platform.processor() or platform.machine()
cpuinfo = Path('/proc/cpuinfo')
if cpuinfo.exists():
    for line in cpuinfo.read_text().splitlines():
        if line.startswith('model name'):
            cpu = line.partition(':')[2].strip()
            break

lines = ['# Результаты исследования', '',
         f'{datetime.now().astimezone():%Y-%m-%d %H:%M %Z} · {platform.system()} · {cpu} · '
         f'логических CPU в системе: {os.cpu_count()}.', '',
         'Цепочка: размытие → резкость → серый → экспозиция → контуры.',
         f'Синтетические BMP, seed {seed}. Один прогрев и {repeats} измерения для каждой конфигурации.',
         'Время — медиана. База ускорения — file-first с одним потоком на том же наборе. '
         'Эффективность равна ускорению, делённому на число запрошенных потоков.', '']

for label in ['batch', *sizes]:
    folder = run / label
    link = folder.relative_to('results').as_posix()
    batch = label == 'batch'
    scope = 'total_ms' if batch else 'process_ms'
    title = f'Папка: {count} кадров {batch_size}, полный цикл' if batch else f'Один кадр {label}, фильтрация'
    with (folder / 'summary.csv').open(newline='') as stream:
        rows = [row for row in csv.DictReader(stream) if row['scope'] == scope]
    cores = defaultdict(list)
    with (folder / 'raw.csv').open(newline='') as stream:
        for row in csv.DictReader(stream):
            if 'cpu_ms' in row and float(row[scope]) > 0:
                cpu_ms = row['cpu_ms' if batch else 'filter_cpu_ms']
                cores[row['mode'], row['threads']].append(float(cpu_ms) / float(row[scope]))
    cpu_header = ' Занято логических CPU |' if cores else ''
    lines.extend([f'## {title}', '', '| Режим | Потоки | Время, мс | Ускорение | Эффективность |' + cpu_header,
                  '|---|---:|---:|---:|---:|' + ('---:|' if cores else '')])
    for row in rows:
        mode, threads = row['mode'], row['threads']
        line = (f"| {mode} | {threads} | {float(row['median_ms']):.3f} | "
                f"{float(row['speedup']):.3f}× | {float(row['efficiency']):.3f} |")
        if cores:
            line += f' {median(cores[mode, threads]):.2f} |'
        lines.append(line)
    if batch:
        largest = max(int(row['threads']) for row in rows)
        compared = [row for row in rows if int(row['threads']) == largest]
        labels = ', '.join(f'"{row["mode"]}"' for row in compared)
        times = ', '.join(row['median_ms'] for row in compared)
        lines.extend(['', '```mermaid', 'xychart-beta',
                      f'    title "Полное время обработки папки, потоков: {largest}"',
                      f'    x-axis [{labels}]', '    y-axis "мс"', f'    bar [{times}]', '```'])
    best = min(rows, key=lambda row: float(row['median_ms']))
    lines.extend(['', f"Минимальная медиана: {best['mode']}, потоков: {best['threads']}, "
                  f"{best['median_ms']} мс.", '',
                  f'[Все измерения]({link}/raw.csv) · [Сводка]({link}/summary.csv) · '
                  f'[Окружение]({link}/environment.txt)', ''])

lines.extend(['[Архив опытов с тремя фильтрами](RESULTS-3-filters.md)', ''])
report = '\n'.join(lines)
pending = run / 'report.tmp'
pending.write_text(report, encoding='utf-8')
pending.replace('results/RESULTS.md')
print(report)
print('Отчёт сохранён в results/RESULTS.md')
