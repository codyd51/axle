project('libimg', 'c')
static_library(
    'img', 
    [
        'libimg.c',
        'nanojpeg.c',
    ],
    install: true,
    install_dir: meson.get_cross_property('libraries_root')
)
install_headers(
    ['libimg.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'libimg')
)
