project('mouse_driver', 'c')
executable(
    'mouse_driver', 
    'mouse_driver.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libport').get_variable('libport_dep'),
        subproject('libamc').get_variable('libamc_dep')
    ]
)
install_headers(
    ['mouse_driver_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'drivers', 'mouse')
)
