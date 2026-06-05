#
# Makefile for FreeRTOS SMP kernel prototype on QEMU virt AArch64
# Phase 2: Exception vectors, GICv3, Generic Timer
#

# Toolchain configuration
CROSS := aarch64-linux-gnu-
CC := $(CROSS)gcc
AS := $(CROSS)as
LD := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
NM := $(CROSS)nm

# Compiler flags
CFLAGS := -mcpu=cortex-a53 \
          -mgeneral-regs-only \
          -ffreestanding \
          -nostdlib \
          -nostartfiles \
          -g \
          -Wall \
          -Wextra \
          -Iinclude

# Assembly flags
ASFLAGS := -mcpu=cortex-a53 \
           -g

# Linker flags
LDFLAGS := -T linker.ld \
           -nostdlib

# Directories
BUILD_DIR := build

# Source files
SRCS_S := src/startup.S \
          src/vectors.S

SRCS_C := src/main.c \
          src/uart.c \
          src/gic.c \
          src/gtimer.c \
          src/interrupt.c

# Object files
OBJS := $(SRCS_S:src/%=$(BUILD_DIR)/src/%.o) \
        $(SRCS_C:src/%=$(BUILD_DIR)/src/%.o)

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

# Build directory for source objects
$(BUILD_DIR)/src:
	mkdir -p $(BUILD_DIR)/src

# ---- Assembly files ----
$(BUILD_DIR)/src/startup.S.o: src/startup.S | $(BUILD_DIR)/src
	@echo "AS  $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/src/vectors.S.o: src/vectors.S | $(BUILD_DIR)/src
	@echo "AS  $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

# ---- C files ----
$(BUILD_DIR)/src/main.c.o: src/main.c | $(BUILD_DIR)/src
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/uart.c.o: src/uart.c | $(BUILD_DIR)/src
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/gic.c.o: src/gic.c | $(BUILD_DIR)/src
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/gtimer.c.o: src/gtimer.c | $(BUILD_DIR)/src
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/interrupt.c.o: src/interrupt.c | $(BUILD_DIR)/src
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Link kernel
$(TARGET): $(OBJS)
	@echo "LD  $(TARGET)"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS)

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
