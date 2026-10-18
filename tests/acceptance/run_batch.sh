#!/usr/bin/env bash
set -e
cd /home/surest/freertos-workspace
for i in {1..5}; do
    echo "=== ACCEPTANCE RUN $i / 5 ==="
    python3 tests/acceptance/accept_qemu_sched.py --mode full --timeout 35
done
echo "=== ALL 5 ACCEPTANCE TESTS PASSED ==="
