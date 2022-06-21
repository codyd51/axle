project('task_viewer', 'c')
executable(
    'task_viewer', 
    'task_viewer.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: subproject('libgui').get_variable('libgui_dep')
)
