project('memory_walker', 'c')
executable(
    'memory_walker', 
    'memory_walker.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: subproject('libgui').get_variable('libgui_dep')
)
