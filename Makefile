#
# Makefile for FreeRTOS SMP kernel prototype on QEMU virt AArch64
# Phase 1: Bare-metal boot with PL011 UART
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
SRCS := src/startup.S \
        src/main.c \
        src/uart.c

# Object files
OBJS := $(SRCS:src/%=$(BUILD_DIR)/src/%.o)
DEPS := $(OBJS:.o=.d)

# Default target
TARGET := kernel.elf
IMAGE := kernel.bin

# Phony targets
.PHONY: all clean run debug build_objs

# Default target
all: $(BUILD_DIR) $(TARGET) $(IMAGE)

# Build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build all object files
$(BUILD_DIR)/src/startup.S.o: src/startup.S | $(BUILD_DIR)/src
	@echo "AS  $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/src/main.c.o: src/main.c | $(BUILD_DIR)/src
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/uart.c.o: src/uart.c | $(BUILD_DIR)/src
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src:
	mkdir -p $(BUILD_DIR)/src

# Link kernel
$(TARGET): $(OBJS)
	@echo "LD  $(TARGET)"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS)

# Convert to binary image
$(IMAGE): $(TARGET)
	@echo "OBJCOPY $(IMAGE)"
	@$(OBJCOPY) -O binary $(TARGET) $(IMAGE)

# Build all objects (dependency calculation)
build_objs: $(OBJS)

# Run in QEMU
run: $(TARGET)
	@echo "Running QEMU..."
	@./scripts/run-qemu.sh $(TARGET)

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

# Rule to generate dependencies
$(BUILD_DIR)/src/%.d: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -MM $< -MT $(BUILD_DIR)/src/$*.o > $@