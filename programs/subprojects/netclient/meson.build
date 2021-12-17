project('netclient', 'c')
executable(
    'netclient', 
    [
        'netclient.c',
        'html.c',
        'elem_stack.c',
        'layout.c',
        'render.c',
        'shims.c',
        'css.c',
        'utils.c',
    ],
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libgui').get_variable('libgui_dep'),
        subproject('libnet').get_variable('libnet_dep'),
        subproject('libutils').get_variable('libutils_dep'),
        meson.get_compiler('c').find_library('m')
    ]
)
