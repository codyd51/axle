project('libutils', 'c')
libutils = static_library(
    'utils', 
    'array.c',
    'assert.c',
    'sleep.c',
    install: true,
    install_dir: meson.get_cross_property('libraries_root')
)
headers_dir = join_paths(meson.get_cross_property('include_root'), 'libutils')
install_headers(
    ['libutils.h', 'array.h', 'assert.h', 'sleep.h', 'cmp.h'],
    install_dir: headers_dir
)
libutils_dep = declare_dependency(include_directories: headers_dir, link_with: libutils)
