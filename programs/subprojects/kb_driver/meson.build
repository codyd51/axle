project('kb_driver', 'c')
executable(
    'kb_driver', 
    'kb_driver.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libport').get_variable('libport_dep'),
        subproject('libamc').get_variable('libamc_dep'),
        subproject('libutils').get_variable('libutils_dep')
    ]
)
install_headers(
    ['kb_driver_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'drivers', 'kb')
)
