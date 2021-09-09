project('realtek_8139_driver', 'c')
add_project_link_arguments('-lagx', '-lgui', '-lamc', '-lport', language: 'c')
executable(
    'realtek_8139_driver', 
    'realtek_8139_driver.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
install_headers(
    ['rtl8139_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'drivers', 'realtek_8139')
)
