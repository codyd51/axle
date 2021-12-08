project('watchdogd', 'c')
add_project_link_arguments('-lamc', '-lutils', language: 'c')
executable(
    'watchdogd', 
    'watchdogd.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
install_headers(
    ['watchdogd_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'daemons', 'watchdogd')
)
