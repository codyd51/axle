project('libamc', 'c')
libamc = static_library(
    'amc', 
    'libamc.c',
    install: true,
    install_dir: meson.get_cross_property('libraries_root')
)
headers_dir = join_paths(meson.get_cross_property('include_root'), 'libamc')
install_headers(
    ['libamc.h'],
    install_dir: headers_dir
)
libamc_dep = declare_dependency(include_directories: headers_dir, link_with: libamc)
