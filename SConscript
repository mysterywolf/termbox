from building import *

cwd = GetCurrentDir()
src	= Glob('*.c')

if GetDepend('TERMBOX_USING_DEMOS'):
    src += Glob('demo/*.c')
    if GetDepend('TERMBOX_USING_CPP_DEMOS'):
        src += Glob('demo/*.cpp')

path = [cwd]
group = DefineGroup('termbox', src, depend = ['PKG_USING_TERMBOX'], CPPPATH = path)
Return('group')
