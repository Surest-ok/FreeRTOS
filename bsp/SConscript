# -*- python -*-
"""
SCons script for Board Support Package (BSP) and Target Linking
Builds freertos_smp.elf and freertos_smp.bin
"""
from SCons.Script import *
from arch.rules import setup_linker_rules

Import('env', 'kernel_lib')

bsp_env = env.Clone()

# Setup linker script and bare-metal linking flags
setup_linker_rules(bsp_env, '#bsp/linker.ld')

# BSP include search paths
bsp_env.Append(CPPPATH=[
    '#bsp/include',
    '#include',
    '#kernel/include',
    '#kernel/portable/GCC/ARM_AARCH64_SRE',
    '#arch/include',
])

bsp_sources = [
    'src/main.c',
    'src/ukernel.c',
    'src/port_fatal.c',
    'src/app.c',
    'src/uart.c',
    'src/gic.c',
    'src/gtimer.c',
    'src/app_irq.c',
    'src/string.c',
    'src/startup.S',
    'src/freertos_vectors.S',
]

bsp_objs = bsp_env.Object(bsp_sources)

# Link ELF image with kernel library and libgcc
elf_target = bsp_env.Program(
    target='freertos_smp.elf',
    source=bsp_objs + [kernel_lib]
)

# Generate raw binary image
bin_target = bsp_env.Command(
    target='freertos_smp.bin',
    source=elf_target,
    action='$OBJCOPY -O binary $SOURCE $TARGET'
)

Return('elf_target bin_target')
