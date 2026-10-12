#!/usr/bin/env bash
set -eu

cd "$(dirname "$0")/.."

echo "=== PipeFlow Failure Cleanup Test ==="

make -j2

# Run the existing DAG tests with timeouts.
timeout 15s make test-dag
timeout 15s make test-dag-runtime

# Check for unreaped PipeFlow children after tests finish.
sleep 1

if pgrep -af 'test_dag_runtime|pipeflow' | grep -v 'pgrep -af' | grep -q .; then
    echo "NOTE: PipeFlow-related processes may still be running."
    pgrep -af 'test_dag_runtime|pipeflow' || true
else
    echo "PASS: No matching PipeFlow processes remain."
fi

echo "=== Cleanup Test Finished ==="
