project('awm', 'c')
add_project_link_arguments('-lagx', '-lamc', '-limg', language: 'c')
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
    install_dir: meson.get_cross_property('initrd_dir')
)
install_headers(
    ['awm.h', 'awm_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'awm')
)
