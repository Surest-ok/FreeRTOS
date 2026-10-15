# -*- python -*-
"""
SCons script for CherryKmod Component (Kernel Module Subsystem)
Builds libcherry_kmod.a for FreeRTOS / AArch64
"""
import os
from SCons.Script import *

Import('env')

kmod_env = env.Clone()

kmod_env.Append(CPPPATH=[
    '#components/cherry_kmod/include',
    '#components/cherry_kmod/tests',
    'tests',
    Dir('tests'),
    '#components/cherry_kal/include',
    '#components/cherry_vfs/include',
    '#components/cherry_shell/include',
    '#bsp/include',
    '#include',
    '#kernel/include',
    '#kernel/portable/GCC/ARM_AARCH64_SRE',
    '#arch/include',
])

# 1. Action to generate sample_mod_ko.h from sample_mod.ko
def bin2c_gen(target, source, env):
    with open(str(source[0]), 'rb') as f:
        data = f.read()
    with open(str(target[0]), 'w') as f:
        f.write('/* Auto-generated from sample_mod.ko */\n')
        f.write('#include <stddef.h>\n')
        f.write('static const unsigned char g_sample_mod_ko[] = {\n')
        for i, b in enumerate(data):
            f.write(f'0x{b:02x}, ')
            if (i + 1) % 16 == 0:
                f.write('\n')
        f.write('\n};\n')
        f.write(f'static const size_t g_sample_mod_ko_len = {len(data)};\n')
    return None

# 2. Compile sample_mod.c to sample_mod.ko (AArch64 relocatable object, ET_REL)
sample_ko = kmod_env.Command(
    'tests/sample_mod.ko',
    'tests/sample_mod.c',
    '$CC -mcpu=cortex-a53 -mstrict-align -mgeneral-regs-only -fno-pie -c -Icomponents/cherry_kmod/include $SOURCE -o $TARGET'
)

# 3. Generate sample_mod_ko.h header
sample_hdr = kmod_env.Command(
    'tests/sample_mod_ko.h',
    sample_ko,
    Action(bin2c_gen, '==> Generating $TARGET from $SOURCE')
)

kmod_sources = [
    'src/kmod_symtab.c',
    'src/kmod_elf.c',
    'src/kmod_core.c',
    'src/kmod_shell.c',
    'tests/kmod_test.c',
]

# Ensure header is generated before compiling tests/kmod_test.c
kmod_env.Depends('tests/kmod_test.c', sample_hdr)

kmod_objs = kmod_env.Object(kmod_sources)

cherry_kmod_lib = kmod_env.Library(
    target='libcherry_kmod.a',
    source=kmod_objs
)

Return('cherry_kmod_lib')
