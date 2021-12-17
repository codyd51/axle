project('activity_monitor', 'c', default_options : ['b_lto=true'])
executable(
    'activity_monitor', 
    'activity_monitor.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: subproject('libgui').get_variable('libgui_dep')
)
