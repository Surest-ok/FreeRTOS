# -*- python -*-
from SCons.Script import *

Import('env')

ahci_env = env.Clone()

ahci_env.Append(CPPPATH=[
    '#components/cherry_ahci/include',
    '#components/cherry_ahci/tests',
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
    'src/ahci_cmd.c',
    'src/ahci_bdev.c',
    'src/ahci_core.c',
    'src/ahci_shell.c',
    'tests/ahci_test.c',
]

ahci_objs = ahci_env.Object(sources)

cherry_ahci_lib = ahci_env.Library(
    target='libcherry_ahci.a',
    source=ahci_objs
)

Return('cherry_ahci_lib')
