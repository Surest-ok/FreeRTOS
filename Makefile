#
# Top-level Makefile for FreeRTOS SMP on QEMU virt AArch64
# SCons Build System Frontend Wrapper
#
# Targets:
#   make all         — build FreeRTOS SMP kernel and BSP image using SCons
#   make run         — build and run simulation in QEMU
#   make sched-test  — run QEMU scheduler acceptance test
#   make sched-full  — run full acceptance suite
#   make sched-cross — run cross-core synchronization stress suite
#   make sched-smp   — run both full and cross acceptance suites
#   make clean       — clean all build artifacts
#

CORES ?= 2
TIMEOUT ?= 30
SCHED_MODE ?= full
LOG_LEVEL ?= 6
TICK_BROADCAST ?= 1
SCONS ?= scons

SCONS_FLAGS := CORES=$(CORES) \
               SCHED_MODE=$(SCHED_MODE) \
               LOG_LEVEL=$(LOG_LEVEL) \
               TICK_BROADCAST=$(TICK_BROADCAST) \
               TIMEOUT=$(TIMEOUT)

.PHONY: all run sched-test sched-full sched-cross sched-smp sched-matrix \
        sched-cross-sem sched-cross-queue sched-cross-mutex sched-cross-spin clean

all:
	@$(SCONS) $(SCONS_FLAGS)

run:
	@$(SCONS) $(SCONS_FLAGS) run

sched-test:
	@$(SCONS) $(SCONS_FLAGS) test

sched-full:
	@$(SCONS) $(SCONS_FLAGS) SCHED_MODE=full test

sched-cross:
	@$(SCONS) $(SCONS_FLAGS) SCHED_MODE=cross test

sched-smp:
	@$(SCONS) $(SCONS_FLAGS) SCHED_MODE=full test
	@$(SCONS) $(SCONS_FLAGS) SCHED_MODE=cross test

sched-matrix: sched-smp

sched-cross-sem:
	@$(SCONS) $(SCONS_FLAGS) SCHED_MODE=cross-sem test

sched-cross-queue:
	@$(SCONS) $(SCONS_FLAGS) SCHED_MODE=cross-queue test

sched-cross-mutex:
	@$(SCONS) $(SCONS_FLAGS) SCHED_MODE=cross-mutex test

sched-cross-spin:
	@$(SCONS) $(SCONS_FLAGS) SCHED_MODE=cross-spin test

clean:
	@$(SCONS) -c || true
	@rm -rf build build-* *.elf *.bin .sconsign.dblite
	@echo "  CLEAN done"
