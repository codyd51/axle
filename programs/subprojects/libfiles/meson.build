project('libfiles', 'c')
libfiles = static_library(
    'files', 
    'libfiles.c',
    install: true,
    install_dir: meson.get_cross_property('libraries_root'),
    dependencies: subproject('libutils').get_variable('libutils_dep')
)
headers_dir = join_paths(meson.get_cross_property('include_root'), 'libfiles')
install_headers(['libfiles.h'], install_dir: headers_dir)
libfiles_dep = declare_dependency(include_directories: headers_dir, link_with: libfiles)
