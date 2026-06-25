#
# Makefile for FreeRTOS kernel library (libfreertos.a)
# AArch64 SMP port for QEMU virt with GICv3
#

CROSS := aarch64-linux-gnu-
CC := $(CROSS)gcc
AS := $(CROSS)gcc
AR := $(CROSS)ar

# Compiler flags
CORES ?= 2
LOG_LEVEL ?= 6
TICK_BROADCAST ?= 1
PORT_SMP_DIAG := $(if $(filter 7,$(LOG_LEVEL)),1,0)

CFLAGS := -mcpu=cortex-a53 \
          -mgeneral-regs-only \
          -ffreestanding \
          -nostdlib \
          -nostartfiles \
          -g \
          -Wall \
          -Wextra \
          -DGUEST \
          -DQEMU \
          -DconfigNUMBER_OF_CORES=$(CORES) \
          -DconfigSMP_TICK_BROADCAST=$(TICK_BROADCAST) \
          -DconfigPORT_SMP_DIAGNOSTICS=$(PORT_SMP_DIAG) \
          -I../arch/include \
          -I../include \
          -Iinclude \
          -Iportable/GCC/ARM_AARCH64_SRE

ASFLAGS := -mcpu=cortex-a53 -g -DGUEST -DQEMU -DconfigNUMBER_OF_CORES=$(CORES) -DconfigSMP_TICK_BROADCAST=$(TICK_BROADCAST) -DconfigPORT_SMP_DIAGNOSTICS=$(PORT_SMP_DIAG) -I../arch/include

BUILD_DIR := build-$(CORES)-log$(LOG_LEVEL)-tb$(TICK_BROADCAST)
DEPS := $(wildcard ../include/*.h include/*.h portable/GCC/ARM_AARCH64_SRE/*.h)

# --- Kernel source files ---
KERNEL_C := tasks.c \
            queue.c \
            list.c \
            timers.c \
            event_groups.c \
            stream_buffer.c \
            MemMang/heap_4.c

# --- Our SMP port ---
PORT_C := port/port.c \
          port/port_smp.c

ARCH_C := ../arch/aarch64/arch.c

PORT_S := port/portASM.S

# --- All objects ---
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C) $(PORT_C)) \
        $(BUILD_DIR)/arch/arch.o \
        $(patsubst %.S,$(BUILD_DIR)/%.o,$(PORT_S))

TARGET := $(BUILD_DIR)/libfreertos.a

.PHONY: all clean

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/port $(BUILD_DIR)/MemMang $(BUILD_DIR)/arch

# C files
$(BUILD_DIR)/%.o: %.c $(DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "  CC    kernel/$<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/arch.o: $(ARCH_C) $(DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "  CC    arch/aarch64/arch.c"
	@$(CC) $(CFLAGS) -c $< -o $@

# Assembly files
$(BUILD_DIR)/%.o: %.S $(DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "  AS    kernel/$<"
	@$(CC) $(ASFLAGS) -c $< -o $@

# Archive
$(TARGET): $(OBJS)
	@echo "  AR    $@"
	@$(AR) rcs $@ $(OBJS)

clean:
	@echo "  CLEAN kernel"
	@rm -rf build-*
