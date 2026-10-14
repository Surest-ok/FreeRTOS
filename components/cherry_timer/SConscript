# -*- python -*-
"""
SCons script for CherryTimer (High Precision Multi-Core Timer Subsystem)
Builds libcherry_timer.a
"""
from SCons.Script import *

Import('env')

timer_env = env.Clone()

timer_env.Append(CPPPATH=[
    '#components/cherry_timer/include',
    '#components/cherry_timer/tests',
    '#components/cherry_kal/include',
    '#components/cherry_printk/include',
    '#bsp/include',
    '#include',
    '#kernel/include',
    '#kernel/portable/GCC/ARM_AARCH64_SRE',
    '#arch/include',
])

timer_sources = [
    'src/timer.c',
    'src/hrtimer.c',
    'src/cherry_timer.c',
    'tests/timer_test.c',
]

timer_objs = timer_env.Object(timer_sources)

cherry_timer_lib = timer_env.Library(
    target='libcherry_timer.a',
    source=timer_objs
)

Return('cherry_timer_lib')
