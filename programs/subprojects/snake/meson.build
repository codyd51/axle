project('snake', 'c')
executable(
    'snake', 
    'snake.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: subproject('libgui').get_variable('libgui_dep')
)
