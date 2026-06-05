#!/bin/bash

# Run QEMU for FreeRTOS SMP kernel
# Usage: ./run-qemu.sh [kernel.elf]

KERNEL=${1:-build/kernel.elf}

# Check if kernel exists
if [ ! -f "$KERNEL" ]; then
    echo "Error: Kernel file $KERNEL not found!"
    echo "Run 'make' first to build the kernel."
    exit 1
fi

# Run QEMU
echo "Starting QEMU..."
echo "Kernel: $KERNEL"
echo "Press Ctrl+X to exit QEMU"

# QEMU command for AArch64 virt platform
qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a53 \
    -smp 1 \
    -m 512 \
    -kernel "$KERNEL" \
    -nographic \
    -monitor none \
    -serial file:/tmp/qemu_serial.log \
    -serial stdio

# Check if QEMU produced any output
if [ -f /tmp/qemu_serial.log ]; then
    echo "=== QEMU Serial Output ==="
    cat /tmp/qemu_serial.log
    rm -f /tmp/qemu_serial.log
else
    echo "No serial output found"
fi