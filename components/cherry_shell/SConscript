# -*- python -*-
"""
SCons script for CherryShell Component
Builds libcherry_shell.a for FreeRTOS / AArch64
"""
from SCons.Script import *

Import('env')

shell_env = env.Clone()

shell_env.Append(CPPPATH=[
    '#components/cherry_shell/include',
    '#components/cherry_shell/tests',
    '#components/cherry_kal/include',
    '#components/cherry_vfs/include',
    '#bsp/include',
    '#include',
    '#kernel/include',
    '#kernel/portable/GCC/ARM_AARCH64_SRE',
    '#arch/include',
])

shell_sources = [
    'src/cherry_shell.c',
    'src/cherry_shell_sym.c',
    'src/cherry_shell_parser.c',
    'src/cherry_shell_cmd.c',
    'tests/shell_test.c',
]

shell_objs = shell_env.Object(shell_sources)

cherry_shell_lib = shell_env.Library(
    target='libcherry_shell.a',
    source=shell_objs
)

Return('cherry_shell_lib')
