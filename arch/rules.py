# -*- python -*-
"""
Generic Architecture Compilation Rules for ARMv8-A (AArch64) SMP RTOS
Provides reusable toolchain configuration, compilation flags, and linking rules.
"""
import os
from SCons.Script import *


def setup_arch_toolchain(env, cross_prefix='aarch64-linux-gnu-', cpu='cortex-a53'):
    """
    Configure GNU toolchain and architecture-specific flags for AArch64 bare-metal.
    """
    # 1. Toolchain programs
    env['CC'] = cross_prefix + 'gcc'
    env['AS'] = cross_prefix + 'gcc'
    env['AR'] = cross_prefix + 'ar'
    env['LINK'] = cross_prefix + 'gcc'
    env['OBJCOPY'] = cross_prefix + 'objcopy'
    env['RANLIB'] = cross_prefix + 'ranlib'

    # 2. Architecture flags (AArch64 bare-metal)
    # -mcpu=cortex-a53: target ARM Cortex-A53 core
    # -mgeneral-regs-only: avoid floating point registers in general kernel code
    # -mno-outline-atomics: force inline LL/SC (ldxr/stxr) for atomic builtins;
    #                       prevents linking dependency on libgcc __aarch64_* helpers.
    # -ffreestanding, -nostdlib, -nostartfiles: bare-metal standalone environment
    arch_flags = [
        f'-mcpu={cpu}',
        '-mstrict-align',
        '-mgeneral-regs-only',
        '-mno-outline-atomics',
        '-ffreestanding',
        '-nostdlib',
        '-nostartfiles',
        '-g',
        '-Wall',
        '-Wextra',
    ]

    env.Append(CCFLAGS=arch_flags)
    env.Append(ASFLAGS=arch_flags + ['-x', 'assembler-with-cpp'])
    env['ASPPFLAGS'] = arch_flags + ['-x', 'assembler-with-cpp']

    # Preprocessor assembly compilation commands
    env['ASCOM'] = '$AS $ASFLAGS $_CPPINCFLAGS -c -o $TARGET $SOURCES'
    env['ASPPCOM'] = '$AS $ASPPFLAGS $_CPPINCFLAGS -c -o $TARGET $SOURCES'

    # Common platform macros
    env.Append(CPPDEFINES=['GUEST', 'QEMU'])

    # Global include search paths
    env.Append(CPPPATH=[
        '#arch/include',
        '#include',
    ])


def setup_linker_rules(env, linker_script):
    """
    Configure linker flags and linker script for bare-metal image.
    """
    env.Append(LINKFLAGS=[
        '-mcpu=cortex-a53',
        '-nostdlib',
        '-T', File(linker_script).abspath,
    ])
    # Bare-metal arithmetic helper library
    env.Append(LIBS=['gcc'])
