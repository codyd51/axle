project('libutils', 'c')
static_library(
    'utils', 
    'array.c',
    'assert.c',
    'sleep.c',
    install: true,
    install_dir: meson.get_cross_property('libraries_root')
)
install_headers(
    ['libutils.h', 'array.h', 'assert.h', 'sleep.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'libutils')
)
