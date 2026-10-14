# -*- python -*-
from SCons.Script import *

Import('env')

fstools_env = env.Clone()

fstools_env.Append(CPPPATH=[
    '#components/cherry_fstools/include',
    '#components/cherry_fstools/tests',
    '#components/cherry_ext4/include',
    '#components/cherry_ext4/tests',
    '#components/cherry_vfs/include',
    '#components/cherry_vfs/tests',
    '#components/cherry_kal/include',
    '#components/cherry_printk/include',
    '#components/cherry_shell/include',
    '#include',
    '#bsp/include',
    '#arch/include',
])

sources = [
    'src/cherry_fstools.c',
    'src/part_mbr.c',
    'src/part_bdev.c',
    'src/cmd_partition.c',
    'src/cmd_mount.c',
    'src/cmd_fileops.c',
    'tests/fstools_test.c',
]

fstools_objs = fstools_env.Object(sources)

cherry_fstools_lib = fstools_env.Library(
    target='libcherry_fstools.a',
    source=fstools_objs
)

Return('cherry_fstools_lib')
