project('kb_driver', 'c')
add_project_link_arguments('-lport', '-lamc', '-lutils', language: 'c')
executable(
    'kb_driver', 
    'kb_driver.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
install_headers(
    ['kb_driver_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'drivers', 'kb')
)
