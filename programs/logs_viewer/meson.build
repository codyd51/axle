project('logs_viewer', 'c')
add_project_link_arguments('-lgui', '-lagx', '-lamc', language: 'c')
executable(
    'logs_viewer', 
    'logs_viewer.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
