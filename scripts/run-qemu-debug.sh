#!/bin/bash

# Debug QEMU for FreeRTOS SMP kernel
# Usage: ./run-qemu-debug.sh [kernel.elf]

KERNEL=${1:-build/kernel.elf}

# Check if kernel exists
if [ ! -f "$KERNEL" ]; then
    echo "Error: Kernel file $KERNEL not found!"
    echo "Run 'make' first to build the kernel."
    exit 1
fi

# Start QEMU with GDB
echo "Starting QEMU with GDB..."
echo "Kernel: $KERNEL"
echo "Connect GDB with: aarch64-linux-gnu-gdb -ex 'target remote localhost:1234'"
echo "Press Ctrl+C to exit QEMU"

# Run QEMU with GDB server
qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a53 \
    -smp 1 \
    -m 512 \
    -kernel "$KERNEL" \
    -s \
    -S \
    -nographic \
    -monitor none \
    -serial stdio