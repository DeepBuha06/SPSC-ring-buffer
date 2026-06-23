#!/bin/bash
# Run perf stat on each queue benchmark individually.
# Captures hardware-level counters that prove the effectiveness of cache-line
# alignment, false sharing prevention, and memory ordering choices.
#
# Usage: ./run_perf.sh [path/to/build/bench_throughput]
#
# Requires: linux-tools-$(uname -r) or linux-tools-generic

set -e

BENCH="${1:-../build/bench_throughput}"

if ! command -v perf &> /dev/null; then
    echo "Error: 'perf' not found."
    echo "Install with: sudo apt install linux-tools-generic linux-tools-\$(uname -r)"
    exit 1
fi

if [ ! -f "$BENCH" ]; then
    echo "Error: benchmark binary not found at '$BENCH'"
    echo "Build first: cd build && cmake .. && make"
    exit 1
fi

# Check if hardware counters are available (may not work in WSL2/VM)
if ! perf stat -e cycles true 2>/dev/null; then
    echo "Warning: Hardware performance counters not available."
    echo "This is expected on WSL2/VM environments."
    echo "Running with software counters only."
    echo ""
    EVENTS="task-clock,context-switches,cpu-migrations,page-faults"
else
    EVENTS="task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,cache-references,cache-misses,branches,branch-misses,L1-dcache-loads,L1-dcache-load-misses"
fi

echo "=== perf stat: $BENCH ==="
echo "Events: $EVENTS"
echo ""

perf stat -e "$EVENTS" "$BENCH" 2>&1

echo ""
echo "=== Done ==="
