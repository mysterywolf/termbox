Import('rtconfig')
from building import *

cwd = GetCurrentDir()
src	= Glob('termbox.c')

if GetDepend('TERMBOX_USING_DEMOS'):
    src += Glob('demo/demo_keyboard.c')
    src += Glob('demo/demo_output.c')
    src += Glob('demo/demo_paint.c')
    src += Glob('demo/demo_truecolor.c')

path = [cwd]
group = DefineGroup('termbox', src, depend = ['PKG_USING_TERMBOX'], CPPPATH = path)
Return('group')
