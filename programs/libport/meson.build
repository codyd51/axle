project('libport', 'c')
static_library(
    'port', 
    'libport.c',
    install: true,
    install_dir: meson.get_cross_property('libraries_root')
)
install_headers(
    ['libport.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'libport')
)