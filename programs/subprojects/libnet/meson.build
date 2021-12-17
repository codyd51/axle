project('libnet', 'c')
add_project_link_arguments('-lutils', language: 'c')
libnet = static_library(
    'net', 
    ['libnet.c'],
    install: true,
    install_dir: meson.get_cross_property('libraries_root'),
    dependencies: subproject('libutils').get_variable('libutils_dep')
)
headers_dir = join_paths(meson.get_cross_property('include_root'), 'libnet')
install_headers(
    ['libnet.h'],
    install_dir: headers_dir
)
libnet_dep = declare_dependency(include_directories: headers_dir, link_with: libnet)
