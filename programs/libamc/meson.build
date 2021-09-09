project('libamc', 'c')
static_library(
    'amc', 
    'libamc.c',
    install: true,
    install_dir: meson.get_cross_property('libraries_root')
)
install_headers(
    ['libamc.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'libamc')
)
