project('logs_viewer', 'c')
executable(
    'logs_viewer', 
    'logs_viewer.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libgui').get_variable('libgui_dep'),
    ]
)
