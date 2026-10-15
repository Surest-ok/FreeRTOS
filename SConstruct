# -*- python -*-
"""
Top-level SConstruct for FreeRTOS SMP on QEMU virt AArch64 (ARMv8-A)
Provides full build orchestration, multi-core configuration, QEMU simulation,
and scheduler acceptance testing.
"""
import os
import sys
from SCons.Script import *

# Add current directory to python path for importing rules
sys.path.insert(0, Dir('.').abspath)
from arch.rules import setup_arch_toolchain

# ==============================================================================
# Configuration Options
# ==============================================================================
vars = Variables('build.conf', ARGUMENTS)
vars.AddVariables(
    ('CORES', 'Number of CPU cores (default: 2, e.g. 1, 2, 4, 8, 32)', 2, None, int),
    ('SCHED_MODE', 'Scheduler validation mode: full, cross, cross-sem, cross-queue, cross-mutex, cross-spin', 'full'),
    ('LOG_LEVEL', 'Log verbosity level (0-7, 7 enables diagnostics)', 6, None, int),
    ('TICK_BROADCAST', 'Broadcast tick interrupt across cores (0 or 1)', 1, None, int),
    ('CROSS_COMPILE', 'Cross compiler toolchain prefix', 'aarch64-linux-gnu-'),
    ('BUILD_DIR', 'Build output directory', 'build'),
    ('TIMEOUT', 'Timeout in seconds for QEMU acceptance test', 30, None, int),
)

env = Environment(
    variables=vars,
    ENV={
        'PATH': os.environ.get('PATH', '/usr/bin:/bin:/usr/local/bin'),
        'HOME': os.environ.get('HOME', '/root'),
    }
)

Help(vars.GenerateHelpText(env))

# ==============================================================================
# Generic Architecture Toolchain Setup
# ==============================================================================
setup_arch_toolchain(
    env,
    cross_prefix=env['CROSS_COMPILE'],
    cpu='cortex-a53'
)

# ==============================================================================
# Target Configuration & Preprocessor Defines
# ==============================================================================
cores = int(env['CORES'])
sched_mode = str(env['SCHED_MODE'])
log_level = int(env['LOG_LEVEL'])
tick_broadcast = int(env['TICK_BROADCAST'])
timeout = int(env['TIMEOUT'])
build_dir = str(env['BUILD_DIR'])

port_smp_diag = 1 if log_level == 7 else 0

defines = [
    ('configNUMBER_OF_CORES', cores),
    ('configSMP_TICK_BROADCAST', tick_broadcast),
    ('configPORT_SMP_DIAGNOSTICS', port_smp_diag),
    ('APP_ENABLE_SMP_DIAG', port_smp_diag),
    ('APP_LOG_LEVEL', log_level),
]

if sched_mode.startswith('cross'):
    defines.append(('APP_ENABLE_BASE_PROBES', 0))
    defines.append(('APP_ENABLE_CROSS_CORE_STRESS', 1))
else:
    defines.append(('APP_ENABLE_BASE_PROBES', 1))
    defines.append(('APP_ENABLE_CROSS_CORE_STRESS', 0))

xsync_masks = {
    'cross-sem': 1,
    'cross-queue': 2,
    'cross-mutex': 4,
    'cross-spin': 8,
}
defines.append(('APP_XSYNC_MASK', xsync_masks.get(sched_mode, 15)))

env.Append(CPPDEFINES=defines)

# ==============================================================================
# Sub-component Orchestration
# ==============================================================================
# 1. Architecture abstraction layer
arch_objs = env.SConscript(
    'arch/SConscript',
    variant_dir=f'{build_dir}/arch',
    duplicate=0,
    exports='env'
)

# 2. FreeRTOS Kernel + SMP Port
kernel_lib = env.SConscript(
    'kernel/SConscript',
    variant_dir=f'{build_dir}/kernel',
    duplicate=0,
    exports=['env', 'arch_objs']
)

# 3. CherryKAL Component (Kernel Abstraction Layer / OSAL)
cherry_kal_lib = env.SConscript(
    'components/cherry_kal/SConscript',
    variant_dir=f'{build_dir}/cherry_kal',
    duplicate=0,
    exports='env'
)

# 4. CherryVFS Component (Virtual File System & Device Driver Framework)
cherry_vfs_lib = env.SConscript(
    'components/cherry_vfs/SConscript',
    variant_dir=f'{build_dir}/cherry_vfs',
    duplicate=0,
    exports='env'
)

# 5. CherryShell Component (VxWorks-style Dynamic Shell & Symbol Engine)
cherry_shell_lib = env.SConscript(
    'components/cherry_shell/SConscript',
    variant_dir=f'{build_dir}/cherry_shell',
    duplicate=0,
    exports='env'
)

# 6. CherryPrintk Component (High-Performance Concurrent Logging Engine)
cherry_printk_lib = env.SConscript(
    'components/cherry_printk/SConscript',
    variant_dir=f'{build_dir}/cherry_printk',
    duplicate=0,
    exports='env'
)

# 7. CherryTimer Component (High-Precision Multi-Core Timer Subsystem)
cherry_timer_lib = env.SConscript(
    'components/cherry_timer/SConscript',
    variant_dir=f'{build_dir}/cherry_timer',
    duplicate=0,
    exports='env'
)

# 8. CherryPCI Component (PCIe ECAM Subsystem & GICv3 ITS / MSI)
cherry_pci_lib = env.SConscript(
    'components/cherry_pci/SConscript',
    variant_dir=f'{build_dir}/cherry_pci',
    duplicate=0,
    exports='env'
)

# 9. CherrySDIO Component (SD / SDIO / MMC / eMMC Stack & Block Device)
cherry_sdio_lib = env.SConscript(
    'components/cherry_sdio/SConscript',
    variant_dir=f'{build_dir}/cherry_sdio',
    duplicate=0,
    exports='env'
)

# 10. CherryEXT4 Component (Ext4 Filesystem, JBD2 Journaling, mkfs, fsck)
cherry_ext4_lib = env.SConscript(
    'components/cherry_ext4/SConscript',
    variant_dir=f'{build_dir}/cherry_ext4',
    duplicate=0,
    exports='env'
)

# 11. CherryFSTools Component (MBR Partitioning, mount/umount, Core File Utilities)
cherry_fstools_lib = env.SConscript(
    'components/cherry_fstools/SConscript',
    variant_dir=f'{build_dir}/cherry_fstools',
    duplicate=0,
    exports='env'
)

# 12. CherryNet Component (Multi-Core Multi-Queue TCP/IP Stack & VirtIO Driver)
cherry_net_lib = env.SConscript(
    'components/cherry_net/SConscript',
    variant_dir=f'{build_dir}/cherry_net',
    duplicate=0,
    exports='env'
)

# 13. CherryNVMe Component (PCIe NVMe 1.4+ Storage Controller Subsystem)
cherry_nvme_lib = env.SConscript(
    'components/cherry_nvme/SConscript',
    variant_dir=f'{build_dir}/cherry_nvme',
    duplicate=0,
    exports='env'
)

# 14. CherryAHCI Component (SATA AHCI 1.3+ Controller & Disk Subsystem)
cherry_ahci_lib = env.SConscript(
    'components/cherry_ahci/SConscript',
    variant_dir=f'{build_dir}/cherry_ahci',
    duplicate=0,
    exports='env'
)

# 15. CherryUSBD Component (USB 2.0/3.0 Device Stack, CDC-ACM, MSC, HID & VUDC)
cherry_usbd_lib = env.SConscript(
    'components/cherry_usbd/SConscript',
    variant_dir=f'{build_dir}/cherry_usbd',
    duplicate=0,
    exports='env'
)

# 16. CherryProcess Component (Lightweight Process Model, POSIX Syscall & Busybox)
cherry_process_lib = env.SConscript(
    'components/cherry_process/SConscript',
    variant_dir=f'{build_dir}/cherry_process',
    duplicate=0,
    exports='env'
)

# 17. BSP and Target Binary
elf_target, bin_target = env.SConscript(
    'bsp/SConscript',
    variant_dir=f'{build_dir}/bsp',
    duplicate=0,
    exports=['env', 'kernel_lib', 'cherry_kal_lib', 'cherry_vfs_lib', 'cherry_shell_lib', 'cherry_printk_lib', 'cherry_timer_lib', 'cherry_pci_lib', 'cherry_sdio_lib', 'cherry_ext4_lib', 'cherry_fstools_lib', 'cherry_net_lib', 'cherry_nvme_lib', 'cherry_ahci_lib', 'cherry_usbd_lib', 'cherry_process_lib']
)



# Default build targets
Default([elf_target, bin_target])
env.Alias('all', [elf_target, bin_target])

# Simulation & Acceptance Test Targets
# ==============================================================================
# QEMU Execution
qemu_cmd = (
    f"truncate -s 64M build/test_sd.img 2>/dev/null || true && "
    f"truncate -s 64M build/test_nvme.img 2>/dev/null || true && "
    f"truncate -s 64M build/test_sata.img 2>/dev/null || true && "
    f"qemu-system-aarch64 "
    f"-machine virt,gic-version=3,its=on "
    f"-cpu cortex-a53 "
    f"-smp {cores} "
    f"-m 512 "
    f"-device sdhci-pci,id=sdhci0 "
    f"-drive if=none,id=sd0,file=build/test_sd.img,format=raw "
    f"-device sd-card,drive=sd0 "
    f"-netdev user,id=net0 "
    f"-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56 "
    f"-drive if=none,id=nvme0,file=build/test_nvme.img,format=raw "
    f"-device nvme,drive=nvme0,serial=nvme_disk_0 "
    f"-device ich9-ahci,id=ahci0 "
    f"-drive if=none,id=sata0,file=build/test_sata.img,format=raw "
    f"-device ide-hd,drive=sata0,bus=ahci0.0 "
    f"-kernel {elf_target[0].abspath} "
    f"-nographic -monitor none -serial stdio"
)
run_node = env.Command('__run_qemu', elf_target, Action(qemu_cmd, f"==> Running QEMU virt AArch64 ({cores} cores)..."))
env.AlwaysBuild(run_node)
env.Alias('run', run_node)
env.Alias('qemu', run_node)

# Acceptance Test
test_cmd = (
    f"python3 tests/acceptance/accept_qemu_sched.py "
    f"--cores {cores} "
    f"--timeout {timeout} "
    f"--mode {sched_mode} "
    f"--elf {elf_target[0].abspath}"
)
test_node = env.Command('__run_test', elf_target, Action(test_cmd, f"==> Running Scheduler Acceptance Test (cores={cores}, mode={sched_mode})..."))
env.AlwaysBuild(test_node)
env.Alias('test', test_node)
env.Alias('sched-test', test_node)
