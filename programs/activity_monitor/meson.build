project('activity_monitor', 'c')
add_project_link_arguments('-lgui', '-lagx', '-lamc', language: 'c')
executable(
    'activity_monitor', 
    'activity_monitor.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
