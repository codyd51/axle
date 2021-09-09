project('snake', 'c')
add_project_link_arguments('-lgui', '-lagx', '-lamc', language: 'c')
executable(
    'snake', 
    'snake.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
