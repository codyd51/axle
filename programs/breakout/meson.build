project('breakout', 'c')
add_project_link_arguments('-lagx', '-lgui', '-lamc', '-lutils', language: 'c')
executable(
    'breakout', 
    'breakout.c', 
    install: true, 
    install_dir: meson.get_cross_property('initrd_dir')
)
