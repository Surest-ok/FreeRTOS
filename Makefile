#
# Top-level Makefile for FreeRTOS SMP on QEMU virt AArch64
# Phase 4: kernel library + BSP
#
# Targets:
#   make all     — build kernel library then BSP
#   make kernel  — build libfreertos.a only
#   make bsp     — build BSP (depends on kernel)
#   make run     — build and run in QEMU (CORES cores, default 2)
#   make sched-test — run local QEMU scheduler acceptance
#   make clean   — clean all build artifacts
#

CORES ?= 2
TIMEOUT ?= 30
SCHED_MODE ?= full
LOG_LEVEL ?= 6
TICK_BROADCAST ?= 1

.PHONY: all kernel bsp run sched-test sched-full sched-cross sched-smp sched-matrix sched-cross-sem sched-cross-queue sched-cross-mutex sched-cross-spin lk-kernel lk-run lk-sched-test lk-sched-full lk-sched-cross lk-sched-smp threadx-kernel threadx-run threadx-sched-test threadx-sched-full threadx-sched-cross threadx-sched-smp arch-sched-matrix clean

all: kernel bsp

kernel:
	@$(MAKE) -C kernel CORES=$(CORES) LOG_LEVEL=$(LOG_LEVEL) TICK_BROADCAST=$(TICK_BROADCAST)

bsp: kernel
	@$(MAKE) -C bsp CORES=$(CORES) SCHED_MODE=$(SCHED_MODE) LOG_LEVEL=$(LOG_LEVEL) TICK_BROADCAST=$(TICK_BROADCAST)

run: bsp
	@$(MAKE) -C bsp run CORES=$(CORES) SCHED_MODE=$(SCHED_MODE) LOG_LEVEL=$(LOG_LEVEL) TICK_BROADCAST=$(TICK_BROADCAST)

sched-test: bsp
	@python3 tests/acceptance/accept_qemu_sched.py \
	    --cores $(CORES) \
	    --timeout $(TIMEOUT) \
	    --mode $(SCHED_MODE) \
	    --elf bsp/kernel-$(CORES)-$(SCHED_MODE)-log$(LOG_LEVEL)-tb$(TICK_BROADCAST).elf

sched-full: SCHED_MODE=full
sched-full: sched-test

sched-cross: SCHED_MODE=cross
sched-cross: sched-test

sched-smp:
	@$(MAKE) CORES=$(CORES) TIMEOUT=$(TIMEOUT) LOG_LEVEL=$(LOG_LEVEL) TICK_BROADCAST=$(TICK_BROADCAST) SCHED_MODE=full sched-test
	@$(MAKE) CORES=$(CORES) TIMEOUT=$(TIMEOUT) LOG_LEVEL=$(LOG_LEVEL) TICK_BROADCAST=$(TICK_BROADCAST) SCHED_MODE=cross sched-test

sched-matrix: sched-smp

sched-cross-sem: SCHED_MODE=cross-sem
sched-cross-sem: sched-test

sched-cross-queue: SCHED_MODE=cross-queue
sched-cross-queue: sched-test

sched-cross-mutex: SCHED_MODE=cross-mutex
sched-cross-mutex: sched-test

sched-cross-spin: SCHED_MODE=cross-spin
sched-cross-spin: sched-test

lk-kernel:
	@$(MAKE) -C lk CORES=$(CORES)

lk-run: lk-kernel
	@$(MAKE) -C lk run CORES=$(CORES)

lk-sched-test: lk-kernel
	@python3 tests/acceptance/accept_qemu_sched.py \
	    --cores $(CORES) \
	    --timeout $(TIMEOUT) \
	    --mode $(SCHED_MODE) \
	    --elf lk/build-$(CORES)/lk.elf

lk-sched-full: SCHED_MODE=full
lk-sched-full: lk-sched-test

lk-sched-cross: SCHED_MODE=cross
lk-sched-cross: lk-sched-test

lk-sched-smp:
	@$(MAKE) CORES=$(CORES) TIMEOUT=$(TIMEOUT) SCHED_MODE=full lk-sched-test
	@$(MAKE) CORES=$(CORES) TIMEOUT=$(TIMEOUT) SCHED_MODE=cross lk-sched-test

threadx-kernel:
	@$(MAKE) -C threadx CORES=$(CORES)

threadx-run: threadx-kernel
	@$(MAKE) -C threadx run CORES=$(CORES)

threadx-sched-test: threadx-kernel
	@python3 tests/acceptance/accept_qemu_sched.py \
	    --cores $(CORES) \
	    --timeout $(TIMEOUT) \
	    --mode $(SCHED_MODE) \
	    --elf threadx/build-$(CORES)/threadx.elf

threadx-sched-full: SCHED_MODE=full
threadx-sched-full: threadx-sched-test

threadx-sched-cross: SCHED_MODE=cross
threadx-sched-cross: threadx-sched-test

threadx-sched-smp:
	@$(MAKE) CORES=$(CORES) TIMEOUT=$(TIMEOUT) SCHED_MODE=full threadx-sched-test
	@$(MAKE) CORES=$(CORES) TIMEOUT=$(TIMEOUT) SCHED_MODE=cross threadx-sched-test

arch-sched-matrix:
	@$(MAKE) CORES=$(CORES) TIMEOUT=$(TIMEOUT) LOG_LEVEL=$(LOG_LEVEL) TICK_BROADCAST=$(TICK_BROADCAST) sched-smp
	@$(MAKE) CORES=$(CORES) TIMEOUT=$(TIMEOUT) lk-sched-smp
	@$(MAKE) CORES=4 TIMEOUT=$(TIMEOUT) threadx-sched-smp

clean:
	@$(MAKE) -C kernel clean
	@$(MAKE) -C bsp clean
	@$(MAKE) -C lk clean
	@$(MAKE) -C threadx clean
	@echo "  CLEAN done"
