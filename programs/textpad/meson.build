project('textpad', 'c')
add_project_link_arguments('-lgui', '-lagx', '-lamc', language: 'c')
executable(
    'textpad', 
    'textpad.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
