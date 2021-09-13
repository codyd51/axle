project('ata_driver', 'c')
add_project_link_arguments('-lport', '-lamc', '-lgui', '-lagx', language: 'c')
executable(
    'ata_driver', 
    'ata_driver.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
install_headers(
    ['ata_driver_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'drivers', 'ata')
)

