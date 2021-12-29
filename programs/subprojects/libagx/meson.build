project('libagx', 'c')
libagx = static_library(
    'libagx', 
    [
        'font/font.c',
        'lib/point.c',
        'lib/color.c',
        'lib/putpixel.c',
        'lib/size.c',
        'lib/ca_layer.c',
        'lib/shapes.c',
        'lib/rect.c',
        'lib/text_box.c',
        'lib/elem_stack.c',
        'lib/hash_map.c',
    ],
    install: true,
    install_dir: meson.get_cross_property('libraries_root'),
    # https://stackoverflow.com/questions/60927711/how-to-set-meson-link-dependency-for-buildin-lib-as-lm-lrt
    dependencies: meson.get_compiler('c').find_library('m')
)
headers_dir = join_paths(meson.get_cross_property('include_root'), 'libagx')
install_headers('libagx.h', install_dir: headers_dir)
install_headers(
    [
        'font/font.h',
        'font/font8x8.h',
    ],
    install_dir: headers_dir
)
install_headers(
    [
        'lib/point.h',
        'lib/color.h',
        'lib/putpixel.h',
        'lib/size.h',
        'lib/ca_layer.h',
        'lib/shapes.h',
        'lib/rect.h',
        'lib/text_box.h',
        'lib/elem_stack.h',
        'lib/hash_map.h',
        'lib/screen.h',
        'lib/gfx.h',
    ],
    install_dir: headers_dir
)
libagx_dep = declare_dependency(include_directories: headers_dir, link_with: libagx)
