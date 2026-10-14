# -*- python -*-
from SCons.Script import *

Import('env')

usbd_env = env.Clone()

usbd_env.Append(CPPPATH=[
    '#components/cherry_usbd/include',
    '#components/cherry_usbd/tests',
    '#components/cherry_vfs/include',
    '#components/cherry_kal/include',
    '#components/cherry_printk/include',
    '#components/cherry_shell/include',
    '#include',
    '#bsp/include',
    '#arch/include',
])

sources = [
    'src/usbd_core.c',
    'src/usbd_cdc.c',
    'src/usbd_msc.c',
    'src/usbd_hid.c',
    'src/usbd_vudc.c',
    'src/usbd_shell.c',
    'tests/usbd_test.c',
]

usbd_objs = usbd_env.Object(sources)

cherry_usbd_lib = usbd_env.Library(
    target='libcherry_usbd.a',
    source=usbd_objs
)

Return('cherry_usbd_lib')
