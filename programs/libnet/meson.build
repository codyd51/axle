project('libnet', 'c')
add_project_link_arguments('-lutils', language: 'c')
static_library(
    'net', 
    ['libnet.c'],
    install: true,
    install_dir: meson.get_cross_property('libraries_root')
)
install_headers(
    ['libnet.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'libnet')
)
