# -*- python -*-
"""
SCons script for CherryEBPF Component (eBPF Virtual Machine & Tracing Subsystem)
Builds libcherry_ebpf.a for FreeRTOS / AArch64
"""
from SCons.Script import *

Import('env')

ebpf_env = env.Clone()

ebpf_env.Append(CPPPATH=[
    '#components/cherry_ebpf/include',
    '#components/cherry_ebpf/tests',
    '#components/cherry_kal/include',
    '#components/cherry_shell/include',
    '#components/cherry_printk/include',
    '#bsp/include',
    '#include',
    '#kernel/include',
    '#kernel/portable/GCC/ARM_AARCH64_SRE',
    '#arch/include',
])

ebpf_sources = [
    'src/ebpf_vm.c',
    'src/ebpf_map.c',
    'src/ebpf_helpers.c',
    'src/ebpf_trace.c',
    'src/ebpf_shell.c',
    'tests/ebpf_test.c',
]

ebpf_objs = ebpf_env.Object(ebpf_sources)

cherry_ebpf_lib = ebpf_env.Library(
    target='libcherry_ebpf.a',
    source=ebpf_objs
)

Return('cherry_ebpf_lib')
