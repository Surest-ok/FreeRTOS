#
# CherryMesa - 3D Graphics Subsystem SConscript
#
# Builds libcherry_mesa.a
#

from SCons.Script import *

Import('env')

mesa_env = env.Clone()
mesa_env['CCFLAGS'] = [f for f in mesa_env['CCFLAGS'] if f != '-mgeneral-regs-only']

mesa_env.Append(CPPPATH=[
    '#components/cherry_mesa/include',
    '#components/cherry_mesa/include/EGL',
    '#components/cherry_mesa/include/GLES2',
    '#components/cherry_mesa/include/GL',
    '#components/cherry_drm/include',
    '#components/cherry_kal/include',
    '#components/cherry_vfs/include',
    '#components/cherry_shell/include',
    '#include',
    '#kernel/include',
])

mesa_sources = [
    'src/gbm.c',
    'src/egl.c',
    'src/gl_api.c',
    'src/mesa_gallium.c',
    'src/mesa_demos.c',
    'src/mesa_shell.c',
    'tests/mesa_test.c',
]

mesa_objs = mesa_env.Object(mesa_sources)
mesa_lib = mesa_env.Library(target='cherry_mesa', source=mesa_objs)

Return('mesa_lib')
