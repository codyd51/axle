project('paintbrush', 'c')
add_project_link_arguments('-lgui', '-lagx', '-lamc', '-lutils', language: 'c')
executable(
    'paintbrush', 
    'paintbrush.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
