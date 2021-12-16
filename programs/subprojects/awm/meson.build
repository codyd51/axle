project('awm', 'c')
executable(
    'awm', 
    [
        'awm.c',
        'effects.c',
        'window.c',
        'utils.c',
        'animations.c',
        'composite.c'
    ],
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libagx').get_variable('libagx_dep'),
        subproject('libamc').get_variable('libamc_dep'),
        subproject('libimg').get_variable('libimg_dep'),
        subproject('libutils').get_variable('libutils_dep'),
    ]
)
install_headers(
    ['awm.h', 'awm_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'awm')
)
