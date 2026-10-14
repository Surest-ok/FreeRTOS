# -*- python -*-
from SCons.Script import *

Import('env')

ext4_env = env.Clone()

ext4_env.Append(CPPPATH=[
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
    'src/cherry_ext4.c',
    'src/ext4_super.c',
    'src/ext4_alloc.c',
    'src/ext4_inode.c',
    'src/ext4_dir.c',
    'src/ext4_jbd2.c',
    'src/ext4_mkfs.c',
    'src/ext4_fsck.c',
    'src/ext4_vfs.c',
    'tests/ext4_test.c',
]

ext4_objs = ext4_env.Object(sources)

cherry_ext4_lib = ext4_env.Library(
    target='libcherry_ext4.a',
    source=ext4_objs
)

Return('cherry_ext4_lib')
