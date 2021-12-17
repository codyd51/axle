project('libimg', 'c')
libimg = static_library(
    'img', 
    [
        'libimg.c',
        'nanojpeg.c',
    ],
    install: true,
    install_dir: meson.get_cross_property('libraries_root')
)
headers_dir = join_paths(meson.get_cross_property('include_root'), 'libimg')
install_headers(
    ['libimg.h'],
    install_dir: headers_dir
)
libimg_dep = declare_dependency(include_directories: headers_dir, link_with: libimg)
