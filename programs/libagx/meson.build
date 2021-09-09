project('libagx', 'c')
static_library(
    'agx', 
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
    install_dir: meson.get_cross_property('libraries_root')
)
# TODO(PT): Change this folder name to libagx
install_headers(
    [
        'font/font.h',
        'font/font8x8.h',
    ],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'agx', 'font')
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
    ],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'agx', 'lib')
)
