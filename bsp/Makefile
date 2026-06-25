#
# Makefile for BSP (Board Support Package)
# QEMU virt AArch64 with GICv3 + FreeRTOS SMP
# Links against ../kernel/build/libfreertos.a
#

CROSS := aarch64-linux-gnu-
CC := $(CROSS)gcc
AS := $(CROSS)gcc
LD := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CORES ?= 2
SCHED_MODE ?= full
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
          -DAPP_ENABLE_SMP_DIAG=$(PORT_SMP_DIAG) \
          -DAPP_LOG_LEVEL=$(LOG_LEVEL) \
          -DAPP_ENABLE_BASE_PROBES=$(if $(filter cross cross-%,$(SCHED_MODE)),0,1) \
          -DAPP_ENABLE_CROSS_CORE_STRESS=$(if $(filter cross cross-%,$(SCHED_MODE)),1,0) \
          -DAPP_XSYNC_MASK=$(if $(filter cross-sem,$(SCHED_MODE)),1,$(if $(filter cross-queue,$(SCHED_MODE)),2,$(if $(filter cross-mutex,$(SCHED_MODE)),4,$(if $(filter cross-spin,$(SCHED_MODE)),8,15)))) \
          -I../arch/include \
          -I../include \
          -I../kernel/include \
          -I../kernel/portable/GCC/ARM_AARCH64_SRE \
          -Iinclude

ASFLAGS := -mcpu=cortex-a53 -g -DGUEST -DQEMU -DconfigNUMBER_OF_CORES=$(CORES) -DconfigSMP_TICK_BROADCAST=$(TICK_BROADCAST) -DconfigPORT_SMP_DIAGNOSTICS=$(PORT_SMP_DIAG) -I../arch/include

LDFLAGS := -T linker.ld -nostdlib -lgcc

BUILD_DIR := build-$(CORES)-$(SCHED_MODE)-log$(LOG_LEVEL)-tb$(TICK_BROADCAST)
KERNEL_LIB := ../kernel/build-$(CORES)-log$(LOG_LEVEL)-tb$(TICK_BROADCAST)/libfreertos.a
DEPS := $(wildcard ../include/*.h ../kernel/include/*.h ../kernel/portable/GCC/ARM_AARCH64_SRE/*.h include/*.h)

# --- BSP source files ---
BSP_C := src/main.c \
         src/ukernel.c \
         src/port_fatal.c \
         src/app.c \
         src/uart.c \
         src/gic.c \
         src/gtimer.c \
         src/app_irq.c \
         src/string.c

BSP_S := src/startup.S \
         src/freertos_vectors.S

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(BSP_C)) \
        $(patsubst %.S,$(BUILD_DIR)/%.o,$(BSP_S))

TARGET := kernel-$(CORES)-$(SCHED_MODE)-log$(LOG_LEVEL)-tb$(TICK_BROADCAST).elf

.PHONY: all clean run

all: $(BUILD_DIR) $(KERNEL_LIB) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/src

# C files
$(BUILD_DIR)/%.o: %.c $(DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "  CC    bsp/$<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Assembly files
$(BUILD_DIR)/%.o: %.S $(DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "  AS    bsp/$<"
	@$(CC) $(ASFLAGS) -c $< -o $@

# Link
$(TARGET): $(OBJS) $(KERNEL_LIB) linker.ld
	@echo "  LD    bsp/$(TARGET)"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS) $(KERNEL_LIB)

run: $(TARGET)
	@echo "Running QEMU (GICv3, $(CORES) cores)..."
	@timeout 10 qemu-system-aarch64 \
	    -machine virt,gic-version=3 \
	    -cpu cortex-a53 \
	    -smp $(CORES) \
	    -m 512 \
	    -kernel $(TARGET) \
	    -nographic \
	    -monitor none \
	    -serial stdio 2>&1 || true

clean:
	@echo "  CLEAN bsp"
	@rm -rf build-* *.elf
