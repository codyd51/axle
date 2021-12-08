project('terminal', 'c')
add_project_link_arguments('-lagx', '-lgui', '-lamc', '-lport', '-lutils', language: 'c')
executable(
    'terminal', 
    'terminal.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
