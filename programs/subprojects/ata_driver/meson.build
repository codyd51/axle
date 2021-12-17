project('ata_driver', 'c')
executable(
    'ata_driver', 
    'ata_driver.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libport').get_variable('libport_dep'),
        subproject('libgui').get_variable('libgui_dep'),
    ]
)
install_headers(
    ['ata_driver_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'drivers', 'ata')
)
