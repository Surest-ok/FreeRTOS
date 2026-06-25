#
# Workspace wrapper for LK mainline-derived QEMU virt AArch64 build.
#

CROSS_COMPILE ?= aarch64-linux-gnu-
CORES ?= 32
BUILD_DIR := build-$(CORES)
TARGET := $(BUILD_DIR)/lk.elf

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE    := $(CROSS_COMPILE)size

ARCH_FLAGS := -march=armv8-a -mtune=cortex-a53 -mgeneral-regs-only -mno-outline-atomics

DEFINES := \
  -DARCH_ARM64=1 -DARCH=\"arm64\" -DARM64_CPU_CORTEX_A53=1 \
  -DARM_ISA_ARMV8=1 -DIS_64BIT=1 \
  -DWITH_SMP=1 -DSMP_MAX_CPUS=$(CORES) \
  -DWITH_KERNEL_VM=1 -DARCH_HAS_MMU=1 \
  -DMEMBASE=0x40000000 -DMEMSIZE=0x08000000 \
  -DKERNEL_ASPACE_BASE=0xffff000000000000UL \
  -DKERNEL_ASPACE_SIZE=0x0001000000000000UL \
  -DKERNEL_BASE=0xffff000000000000UL \
  -DKERNEL_LOAD_OFFSET=0x00100000 \
  -DUSER_ASPACE_BASE=0x0000000001000000UL \
  -DUSER_ASPACE_SIZE=0x0000fffffe000000UL \
  -DMMU_KERNEL_SIZE_SHIFT=48 \
  -DARM64_PAGE_SIZE_SHIFT=12 -DARM64_PAGE_SIZE=4096 \
  -DPLATFORM_QEMU_VIRT_ARM=1 -DPLATFORM=\"qemu-virt-arm\" \
  -DTARGET=\"qemu-virt\" -DPROJECT=\"lk_qemu_a53\" \
  -DARM_WITH_GICV3=1 -DWITH_DEV_INTERRUPT_ARM_GICV3=1 \
  -DWITH_LIB_CONSOLE=1 -DENABLE_PANIC_SHELL=0 \
  -DWITH_DEBUG_LINEBUFFER=1 \
  -DKERNEL_ASPACE_BITS=25 \
  -DWITH_LIB_HEAP_CMPCTMALLOC=1 \
  -DWITH_APP_SHELL=0 \
  -DWITH_LIB_ELF=0 -DWITH_ELF64=0 \
  -DWITH_CPP_SUPPORT=0 -DWITH_LIB_CPP=0 \
  -DWITH_LIB_IOVEC=1 -DWITH_LIB_POOL=1 \
  -DLK_DEBUGLEVEL=1

INCLUDES := \
  -I../arch/include \
  -I. -Ibuild-$(CORES) -Iinclude \
  -Iarch/arm64/include -Iarch/arm64 -Iarch/include \
  -Ikernel/include -Ikernel \
  -Iplatform/include -Iplatform/qemu-virt-arm/include \
  -Iplatform/qemu-virt-arm/drivers/interrupt/arm_gicv3/include \
  -Iplatform/qemu-virt-arm/drivers/uart/pl011/include \
  -Iplatform/qemu-virt-arm/drivers/timer/arm_generic/include \
  -Iplatform/qemu-virt-arm/drivers/power/psci/include \
  -Ilib/libc/include -Ilib/heap/include -Ilib/heap/cmpctmalloc/include \
  -Ilib/io/include -Ilib/fixed_point/include -Ilib/cbuf/include \
  -Ilib/console/include -Ilib/debugcommands/include -Ilib/version/include \
  -Ilib/iovec/include -Ilib/pool/include \
  -Itop/include -Iapp/include -Iapp/shell

CFLAGS := -O2 -g -Wall -Wextra -Wno-unused-parameter -std=gnu11 \
          -ffixed-x18 -fno-omit-frame-pointer -fno-strict-aliasing \
          -nostdlib -fno-builtin -fms-extensions \
          -ffunction-sections -fdata-sections -fpie \
          $(ARCH_FLAGS) $(DEFINES) $(INCLUDES)

ASFLAGS := -D__ASSEMBLY__ -DASSEMBLY $(DEFINES) $(INCLUDES) -fno-builtin

LDFLAGS := -O2 -g $(ARCH_FLAGS) -nostdlib -fno-builtin \
           -Wl,--gc-sections,--build-id=none -nostartfiles -nodefaultlibs -lgcc \
           -Wl,-Map=$(BUILD_DIR)/lk.map,-cref,-u,_start \
           -T lk_qemu.ld -z max-page-size=4096 \
           -pie -Wl,-z,notext

SRCS_C := \
  kernel/thread.c kernel/semaphore.c kernel/mutex.c kernel/event.c \
  kernel/timer.c kernel/mp.c kernel/init.c kernel/debug.c kernel/port.c \
  kernel/vm/bootalloc.c kernel/vm/pmm.c kernel/vm/vm.c kernel/vm/vmm.c \
  kernel/vm/page.c \
  arch/arm64/arch.c arch/arm64/exceptions_c.c arch/arm64/fpu.c \
  arch/arm64/mmu.c arch/arm64/mp.c arch/arm64/thread.c arch/arch.c \
  platform/qemu-virt-arm/platform.c platform/qemu-virt-arm/debug.c \
  platform/qemu-virt-arm/target/init.c \
  platform/qemu-virt-arm/drivers/uart/pl011/uart.c \
  platform/qemu-virt-arm/drivers/interrupt/arm_gicv3/arm_gicv3.c \
  platform/qemu-virt-arm/drivers/timer/arm_generic/arm_generic_timer.c \
  platform/qemu-virt-arm/drivers/power/psci/psci.c \
  lib/heap/heap_wrapper.c lib/heap/cmpctmalloc/cmpctmalloc.c \
  lib/libc/string/memchr.c lib/libc/string/memcmp.c \
  lib/libc/string/memcpy.c lib/libc/string/memmove.c \
  lib/libc/string/memset.c lib/libc/string/strcat.c \
  lib/libc/string/strchr.c lib/libc/string/strcmp.c \
  lib/libc/string/strcpy.c lib/libc/string/strlcat.c \
  lib/libc/string/strlcpy.c lib/libc/string/strlen.c \
  lib/libc/string/strncmp.c lib/libc/string/strncpy.c \
  lib/libc/string/strnlen.c lib/libc/string/strpbrk.c \
  lib/libc/string/strrchr.c lib/libc/string/strstr.c \
  lib/libc/string/strdup.c lib/libc/atoi.c lib/libc/bsearch.c \
  lib/libc/ctype.c lib/libc/eabi.c lib/libc/printf.c \
  lib/libc/stdio.c lib/libc/qsort.c lib/libc/rand.c lib/libc/atexit.c \
  lib/io/console.c lib/io/io.c \
  lib/cbuf/cbuf.c lib/console/console.c \
  lib/debugcommands/debugcommands.c lib/version/version.c \
  lib/fixed_point/fixed_point.c lib/iovec/iovec.c lib/pool/pool.c \
  top/main.c top/init.c top/debug.c top/mp.c \
  app/app.c app/sched_matrix.c

SRCS_S := \
  arch/arm64/start.S arch/arm64/exceptions.S arch/arm64/spinlock.S \
  arch/arm64/cache-ops.S arch/arm64/asm.S arch/arm64/atomics.S \
  platform/qemu-virt-arm/drivers/power/psci/psci_asm.S

ARCH_API_C := ../arch/aarch64/arch.c
ARCH_API_OBJ := $(BUILD_DIR)/workspace_arch/arch.o

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS_C)) \
        $(ARCH_API_OBJ) \
        $(patsubst %.S,$(BUILD_DIR)/%.o,$(SRCS_S))

.PHONY: all clean run

all: $(BUILD_DIR)/buildid.h $(TARGET)
	@$(SIZE) $(TARGET)

$(BUILD_DIR)/buildid.h:
	@mkdir -p $(dir $@)
	@printf '%s\n' '#define BUILDID "workspace-lk-mainline"' > $@

$(TARGET): $(OBJS) lk_qemu.ld $(BUILD_DIR)/buildid.h
	@mkdir -p $(dir $@)
	@echo "  LD    lk/$(TARGET)"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: %.c $(BUILD_DIR)/buildid.h
	@mkdir -p $(dir $@)
	@echo "  CC    lk/$<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(ARCH_API_OBJ): $(ARCH_API_C)
	@mkdir -p $(dir $@)
	@echo "  CC    arch/aarch64/arch.c"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "  AS    lk/$<"
	@$(CC) $(ASFLAGS) -c -o $@ $<

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
	@echo "  CLEAN lk"
	@rm -rf build-*
