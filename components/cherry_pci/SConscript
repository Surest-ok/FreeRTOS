# -*- python -*-
from SCons.Script import *

Import('env')

pci_env = env.Clone()

pci_env.Append(CPPPATH=[
    '#components/cherry_pci/include',
    '#components/cherry_pci/tests',
    '#include',
    '#bsp/include',
    '#arch/include',
    '#components/cherry_kal/include',
    '#components/cherry_printk/include',
    '#components/cherry_shell/include',
])

sources = [
    'src/cherry_pci.c',
    'src/pci_ecam.c',
    'src/pci_bus.c',
    'src/pci_driver.c',
    'src/pci_its.c',
    'src/pci_msi.c',
    'src/pci_cmd.c',
    'tests/pci_test.c',
]

pci_objs = pci_env.Object(sources)

cherry_pci_lib = pci_env.Library(
    target='libcherry_pci.a',
    source=pci_objs
)

Return('cherry_pci_lib')
