#
# CherryDRM - Direct Rendering Manager Subsystem SConscript
#
# Builds libcherry_drm.a
#

from SCons.Script import *

Import('env')

drm_env = env.Clone()

drm_env.Append(CPPPATH=[
    '#components/cherry_drm/include',
    '#components/cherry_drm/include/drm',
    '#components/cherry_kal/include',
    '#components/cherry_vfs/include',
    '#components/cherry_pci/include',
    '#components/cherry_shell/include',
    '#include',
    '#kernel/include',
    '#arch/include',
])

drm_sources = [
    'src/drm_core.c',
    'src/drm_gem.c',
    'src/drm_kms.c',
    'src/drm_fb.c',
    'src/drm_virtio.c',
    'src/drm_mali.c',
    'src/drm_shell.c',
    'tests/drm_test.c',
]

drm_objs = drm_env.Object(drm_sources)
drm_lib = drm_env.Library(target='cherry_drm', source=drm_objs)

Return('drm_lib')
