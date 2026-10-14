# -*- python -*-
"""
SCons script for CherryKAL (Kernel Abstraction Layer / OSAL)
Builds libcherry_kal.a
"""
from SCons.Script import *

Import('env')

kal_env = env.Clone()

kal_env.Append(CPPPATH=[
    '#components/cherry_kal/include',
    '#include',
    '#bsp/include',
    '#kernel/include',
    '#kernel/portable/GCC/ARM_AARCH64_SRE',
    '#arch/include',
])

kal_sources = [
    'src/kal_freertos.c',
]

kal_objs = kal_env.Object(kal_sources)

cherry_kal_lib = kal_env.Library(
    target='libcherry_kal.a',
    source=kal_objs
)

Return('cherry_kal_lib')
