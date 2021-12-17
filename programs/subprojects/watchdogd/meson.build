project('watchdogd', 'c')
executable(
    'watchdogd', 
    'watchdogd.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libamc').get_variable('libamc_dep'),
        subproject('libutils').get_variable('libutils_dep'),
    ]
)
install_headers(
    ['watchdogd_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'daemons', 'watchdogd')
)
