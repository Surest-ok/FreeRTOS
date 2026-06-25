#
# Workspace wrapper for Luna-style ThreadX SMP AArch64 build.
#

THREADX_SRC ?= /mnt/d/workspace/luna-workspace/other-os/threadx-master
CROSS ?= aarch64-none-elf-
CC := $(CROSS)gcc
LD := $(CROSS)gcc
SIZE := $(CROSS)size

CORES ?= 4
BUILD_DIR := build-$(CORES)
TARGET := $(BUILD_DIR)/threadx.elf

CORE_MASK := $(shell python3 -c 'c=int("$(CORES)"); print(hex((1 << c) - 1 if c < 32 else 0xffffffff))')

ARCH_FLAGS := -mcpu=cortex-a53 -mgeneral-regs-only
COMMON_DEFS := \
  -DGUEST -DQEMU \
  -DEL1 \
  -DTX_DISABLE_ERROR_CHECKING \
  -DTX_THREAD_SMP_MAX_CORES=$(CORES) \
  -DTX_THREAD_SMP_CORE_MASK=$(CORE_MASK) \
  -DTX_THREAD_USER_EXTENSION='void *tx_arch_thread;' \
  -DTX_THREAD_CREATE_INTERNAL_EXTENSION\(t\)=threadx_arch_thread_create\(t\)\;

LOWLEVEL_C_STACK ?= 1
ifeq ($(LOWLEVEL_C_STACK),1)
COMMON_DEFS += -DTHREADX_LOWLEVEL_C_STACK_BUILD=1
endif

INCLUDES := \
  -I../arch/include \
  -Iport \
  -I$(THREADX_SRC)/arch/aarch64/include \
  -I$(THREADX_SRC)/arch/aarch64 \
  -I$(THREADX_SRC)/include \
  -I$(THREADX_SRC)/examples/drivers/timer \
  -I$(THREADX_SRC)/examples/drivers/gic \
  -I$(THREADX_SRC)/examples/drivers \
  -I$(THREADX_SRC)

CFLAGS := $(ARCH_FLAGS) -ffreestanding -nostdlib -nostartfiles -g -O2 \
          -ffunction-sections -fdata-sections \
          -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function \
          -include threadx_arch.h \
          $(COMMON_DEFS) $(INCLUDES)
ASFLAGS := $(ARCH_FLAGS) -g $(COMMON_DEFS) $(INCLUDES)
LDFLAGS := -T $(THREADX_SRC)/threadx_smp.ld -nostdlib -nostartfiles -Wl,--gc-sections

THREADX_C := \
  app/sched_matrix.c \
  port/threadx_arch.c \
  port/threadx_lowlevel.c \
  $(THREADX_SRC)/examples/drivers/uart/pl011.c \
  $(THREADX_SRC)/examples/drivers/gic/gicv3_qemu.c \
  $(THREADX_SRC)/examples/drivers/timer/arm_generic_timer.c \
  $(THREADX_SRC)/arch/aarch64/irq_handler.c \
  $(THREADX_SRC)/arch/aarch64/psci.c \
  $(wildcard $(THREADX_SRC)/arch/aarch64/kernel/*.c) \
  $(filter-out $(THREADX_SRC)/kernel/smp/ke_% $(THREADX_SRC)/kernel/smp/kr_%,$(wildcard $(THREADX_SRC)/kernel/smp/*.c)) \
  $(THREADX_SRC)/kernel/k_string.c \
  $(THREADX_SRC)/kernel/printk.c \
  ../arch/aarch64/arch.c

THREADX_KERNEL_ASM_ALL := $(wildcard $(THREADX_SRC)/arch/aarch64/kernel/*.S)
ifeq ($(LOWLEVEL_C_STACK),1)
THREADX_KERNEL_ASM_ALL := $(filter-out $(THREADX_SRC)/arch/aarch64/kernel/k_thread_stack_build.S,$(THREADX_KERNEL_ASM_ALL))
endif

THREADX_S := \
  $(THREADX_SRC)/arch/aarch64/boot.S \
  $(THREADX_SRC)/arch/aarch64/vectors.S \
  $(THREADX_SRC)/arch/aarch64/psci_call.S \
  $(filter-out \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_smp_core_get.S \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_smp_time_get.S \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_smp_current_state_get.S \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_smp_current_thread_get.S \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_smp_low_level_initialize.S \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_smp_initialize_wait.S \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_interrupt_disable.S \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_interrupt_restore.S \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_interrupt_control.S \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_smp_protect.S \
    $(THREADX_SRC)/arch/aarch64/kernel/k_thread_smp_unprotect.S, \
    $(THREADX_KERNEL_ASM_ALL))

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(THREADX_C)) \
        $(patsubst %.S,$(BUILD_DIR)/%.o,$(THREADX_S))

.PHONY: all clean run

all: $(TARGET)
	@$(SIZE) $(TARGET)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    threadx/$<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "  AS    threadx/$<"
	@$(CC) $(ASFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	@echo "  LD    threadx/$(TARGET)"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS) -lgcc

run: $(TARGET)
	@qemu-system-aarch64 \
	    -machine virt,gic-version=3 \
	    -cpu cortex-a53 \
	    -smp $(CORES) \
	    -m 512 \
	    -kernel $(TARGET) \
	    -nographic \
	    -monitor none \
	    -serial stdio

clean:
	@echo "  CLEAN threadx"
	@rm -rf build-*
