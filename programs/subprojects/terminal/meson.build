project('terminal', 'c')
executable(
    'terminal', 
    'terminal.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libgui').get_variable('libgui_dep'),
        subproject('libport').get_variable('libport_dep'),
    ]
)
