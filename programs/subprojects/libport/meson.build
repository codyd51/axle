project('libport', 'c')
libport = static_library(
    'port', 
    'libport.c',
    install: true,
    install_dir: meson.get_cross_property('libraries_root')
)
headers_dir = join_paths(meson.get_cross_property('include_root'), 'libport')
install_headers(
    ['libport.h'],
    install_dir: headers_dir
)
libport_dep = declare_dependency(include_directories: headers_dir, link_with: libport)
