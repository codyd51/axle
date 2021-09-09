project('2048', 'c')
add_project_link_arguments('-lagx', '-lgui', '-lamc', language: 'c')
executable(
    '2048', 
    '2048.c', 
    install: true, 
    install_dir: meson.get_cross_property('initrd_dir')
)
