#!/usr/bin/env python3
"""
Generate benchmark comparison charts from raw output.

Usage:
    ./build/bench_throughput | python3 benchmark/plot_results.py throughput
    ./build/bench_latency   | python3 benchmark/plot_results.py latency

Saves charts to benchmark/results/ as PNG files.
Requires: matplotlib (pip install matplotlib)
"""

import sys
import re
import os

def parse_throughput(lines):
    """Parse throughput table output: name, median, min, max"""
    results = []
    for line in lines:
        match = re.match(r'\s+(.+?)\s{2,}([\d.]+)\s+([\d.]+)\s+([\d.]+)', line)
        if match:
            name = match.group(1).strip()
            if name in ('Implementation', '---'):
                continue
            median = float(match.group(2))
            results.append((name, median))
    return results

def parse_latency(lines):
    """Parse latency table output: name, p50, p99, p99.9"""
    results = []
    for line in lines:
        match = re.match(r'\s+(.+?)\s{2,}(\d+)\s+(\d+)\s+(\d+)', line)
        if match:
            name = match.group(1).strip()
            if name in ('Implementation', '---'):
                continue
            p50  = int(match.group(2))
            p99  = int(match.group(3))
            p999 = int(match.group(4))
            results.append((name, p50, p99, p999))
    return results

def plot_throughput(results, out_dir):
    import matplotlib.pyplot as plt

    names   = [r[0] for r in results]
    medians = [r[1] for r in results]
    colors  = ['#2563eb', '#059669', '#7c3aed', '#dc2626']

    fig, ax = plt.subplots(figsize=(10, 5))
    bars = ax.barh(names, medians, color=colors[:len(names)], height=0.5)
    ax.set_xlabel('Throughput (M ops/sec)')
    ax.set_title('SPSC Queue Throughput Comparison')
    ax.invert_yaxis()

    for bar, val in zip(bars, medians):
        ax.text(bar.get_width() + max(medians)*0.01, bar.get_y() + bar.get_height()/2,
                f'{val:.2f}', va='center', fontsize=10)

    plt.tight_layout()
    path = os.path.join(out_dir, 'throughput.png')
    plt.savefig(path, dpi=150)
    print(f'Saved: {path}')

def plot_latency(results, out_dir):
    import matplotlib.pyplot as plt
    import numpy as np

    names = [r[0] for r in results]
    p50s  = [r[1] / 1000 for r in results]  # convert ns to us
    p99s  = [r[2] / 1000 for r in results]
    p999s = [r[3] / 1000 for r in results]

    x = np.arange(len(names))
    width = 0.25

    fig, ax = plt.subplots(figsize=(12, 6))
    bars1 = ax.bar(x - width, p50s,  width, label='p50',   color='#2563eb')
    bars2 = ax.bar(x,         p99s,  width, label='p99',   color='#f59e0b')
    bars3 = ax.bar(x + width, p999s, width, label='p99.9', color='#dc2626')

    ax.set_ylabel('Latency (microseconds)')
    ax.set_title('SPSC Queue Latency Comparison (Lower is Better)')
    ax.set_xticks(x)
    
    # Wrap long names so they center perfectly without needing rotation
    wrapped_names = [n.replace('::', '::\n').replace(' ', '\n') for n in names]
    ax.set_xticklabels(wrapped_names, rotation=0, ha='center')
    ax.legend()
    
    # Add text labels on top of each bar so exact values are visible
    def add_labels(bars):
        for bar in bars:
            height = bar.get_height()
            ax.annotate(f'{height:.1f}',
                        xy=(bar.get_x() + bar.get_width() / 2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha='center', va='bottom', fontsize=9)

    add_labels(bars1)
    add_labels(bars2)
    add_labels(bars3)

    # Increase y-limit slightly to fit the text labels
    ax.set_ylim(0, max(p999s) * 1.15)

    plt.tight_layout()
    path = os.path.join(out_dir, 'latency.png')
    plt.savefig(path, dpi=150)
    print(f'Saved: {path}')

def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ('throughput', 'latency'):
        print(f'Usage: {sys.argv[0]} <throughput|latency>')
        print('Pipe benchmark output into stdin.')
        sys.exit(1)

    mode = sys.argv[1]
    lines = sys.stdin.readlines()

    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'results')
    os.makedirs(out_dir, exist_ok=True)

    if mode == 'throughput':
        results = parse_throughput(lines)
        if not results:
            print('Error: could not parse throughput data from stdin')
            sys.exit(1)
        plot_throughput(results, out_dir)
    else:
        results = parse_latency(lines)
        if not results:
            print('Error: could not parse latency data from stdin')
            sys.exit(1)
        plot_latency(results, out_dir)

if __name__ == '__main__':
    main()
