# -*- python -*-
"""
SCons script for CherryVFS Component
Builds libcherry_vfs.a for FreeRTOS on AArch64
"""
from SCons.Script import *

Import('env')

vfs_env = env.Clone()

# Include search paths for CherryVFS
vfs_env.Append(CPPPATH=[
    '#components/cherry_vfs/include',
    '#components/cherry_vfs/tests',
    '#include',
    '#bsp/include',
    '#kernel/include',
    '#kernel/portable/GCC/ARM_AARCH64_SRE',
    '#arch/include',
])

vfs_sources = [
    'src/vfs_core.c',
    'src/vfs_posix.c',
    'src/vfs_dev.c',
    'src/vfs_driver.c',
    'src/vfs_platform.c',
    'src/fs_devfs.c',
    'src/fs_ramfs.c',
    'src/dev_null_zero.c',
    'src/dev_ramdisk.c',
    'src/dev_socket.c',
    'src/dev_pl011.c',
    'src/cherry_vfs.c',
    'tests/vfs_test.c',
]

vfs_objs = vfs_env.Object(vfs_sources)

# Build static library
cherry_vfs_lib = vfs_env.Library(
    target='libcherry_vfs.a',
    source=vfs_objs
)

Return('cherry_vfs_lib')
