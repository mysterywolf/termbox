Import('rtconfig')
from building import *

cwd = GetCurrentDir()
src	= Glob('termbox.c')

if GetDepend('TERMBOX_USING_DEMOS'):
    src += Glob('demo/keyboard.c')
    src += Glob('demo/output.c')
    src += Glob('demo/paint.c')
    src += Glob('demo/truecolor.c')
    src += Glob('demo/angryly.c')

path = [cwd]
group = DefineGroup('termbox', src, depend = ['PKG_USING_TERMBOX'], CPPPATH = path)
Return('group')
