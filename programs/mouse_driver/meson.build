project('mouse_driver', 'c')
add_project_link_arguments('-lport', '-lamc', language: 'c')
executable(
    'mouse_driver', 
    'mouse_driver.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
