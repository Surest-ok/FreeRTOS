# -*- python -*-
from SCons.Script import *

Import('env')

nvme_env = env.Clone()

nvme_env.Append(CPPPATH=[
    '#components/cherry_nvme/include',
    '#components/cherry_nvme/tests',
    '#components/cherry_pci/include',
    '#components/cherry_vfs/include',
    '#components/cherry_kal/include',
    '#components/cherry_printk/include',
    '#components/cherry_shell/include',
    '#include',
    '#bsp/include',
    '#arch/include',
])

sources = [
    'src/nvme_cmd.c',
    'src/nvme_bdev.c',
    'src/nvme_core.c',
    'src/nvme_shell.c',
    'tests/nvme_test.c',
]

nvme_objs = nvme_env.Object(sources)

cherry_nvme_lib = nvme_env.Library(
    target='libcherry_nvme.a',
    source=nvme_objs
)

Return('cherry_nvme_lib')
