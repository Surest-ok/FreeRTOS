# -*- python -*-
"""
SCons script for CherryPrintk (High Performance Concurrent Linux Logging Engine)
Builds libcherry_printk.a
"""
from SCons.Script import *

Import('env')

printk_env = env.Clone()

printk_env.Append(CPPPATH=[
    '#components/cherry_printk/include',
    '#components/cherry_printk/tests',
    '#components/cherry_kal/include',
    '#bsp/include',
    '#include',
    '#kernel/include',
    '#kernel/portable/GCC/ARM_AARCH64_SRE',
    '#arch/include',
])

printk_sources = [
    'src/printk_ringbuffer.c',
    'src/printk.c',
    'tests/printk_test.c',
]

printk_objs = printk_env.Object(printk_sources)

cherry_printk_lib = printk_env.Library(
    target='libcherry_printk.a',
    source=printk_objs
)

Return('cherry_printk_lib')
