#
# Makefile for FreeRTOS on QEMU virt AArch64
# Phase 3: FreeRTOS V11.x + GICv3 + Generic Timer
#

# Toolchain configuration
CROSS := aarch64-linux-gnu-
CC := $(CROSS)gcc
AS := $(CROSS)gcc
LD := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
NM := $(CROSS)nm

# FreeRTOS kernel source directory
FREERTOS_DIR := FreeRTOS-Kernel

# Compiler flags
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
          -Iinclude \
          -I$(FREERTOS_DIR)/include \
          -I$(FREERTOS_DIR)/portable/GCC/ARM_AARCH64_SRE

# Assembly flags
ASFLAGS := -mcpu=cortex-a53 \
           -g \
           -DGUEST \
           -DQEMU

# Linker flags
LDFLAGS := -T linker.ld \
           -nostdlib \
           -lgcc

# Directories
BUILD_DIR := build

# ---------------------------------------------------------------
# Source files
# ---------------------------------------------------------------

# Application assembly
SRCS_S := src/startup.S \
          src/freertos_vectors.S

# FreeRTOS port assembly
SRCS_S += $(FREERTOS_DIR)/portable/GCC/ARM_AARCH64_SRE/portASM.S

# Application C
SRCS_C := src/main.c \
          src/uart.c \
          src/gic.c \
          src/gtimer.c \
          src/app_irq.c \
          src/string.c

# FreeRTOS kernel C
SRCS_C += $(FREERTOS_DIR)/tasks.c \
          $(FREERTOS_DIR)/queue.c \
          $(FREERTOS_DIR)/list.c \
          $(FREERTOS_DIR)/timers.c \
          $(FREERTOS_DIR)/event_groups.c \
          $(FREERTOS_DIR)/stream_buffer.c \
          $(FREERTOS_DIR)/portable/GCC/ARM_AARCH64_SRE/port.c \
          $(FREERTOS_DIR)/portable/MemMang/heap_4.c

# Object files — transform path: replace / with _ in build dir
OBJS := $(patsubst %.S,$(BUILD_DIR)/%.S.o,$(SRCS_S)) \
        $(patsubst %.c,$(BUILD_DIR)/%.c.o,$(SRCS_C))

DEPS := $(OBJS:.o=.d)

# Default target
TARGET := kernel.elf
IMAGE := kernel.bin

# Phony targets
.PHONY: all clean run debug nm dump

# Default target
all: $(BUILD_DIR) $(TARGET) $(IMAGE)

# Build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ---- Generic compilation rules ----

# Assembly files
$(BUILD_DIR)/%.S.o: %.S | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "AS  $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

# C files
$(BUILD_DIR)/%.c.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Link kernel
$(TARGET): $(OBJS)
	@echo "LD  $(TARGET)"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Convert to binary image
$(IMAGE): $(TARGET)
	@echo "OBJCOPY $(IMAGE)"
	@$(OBJCOPY) -O binary $(TARGET) $(IMAGE)

# Run in QEMU
run: $(TARGET)
	@echo "Running QEMU (GICv3)..."
	@timeout 10 qemu-system-aarch64 \
	    -machine virt,gic-version=3 \
	    -cpu cortex-a53 \
	    -smp 1 \
	    -m 512 \
	    -kernel $(TARGET) \
	    -nographic \
	    -monitor none \
	    -serial stdio 2>&1 || true

# Debug mode (with GDB)
debug: CFLAGS += -O0 -g3
debug: $(TARGET)
	@echo "QEMU with GDB debugging..."
	@./scripts/run-qemu-debug.sh $(TARGET)

# Clean build artifacts
clean:
	@echo "Cleaning..."
	@rm -rf $(BUILD_DIR) *.elf *.bin *.map

# Show symbols
nm: $(TARGET)
	@echo "Symbols in $(TARGET):"
	@$(NM) $(TARGET)

# Disassemble
dump: $(TARGET)
	@echo "Disassembly of $(TARGET):"
	@$(OBJDUMP) -D $(TARGET)

# Include dependencies
-include $(DEPS)
