project('netclient', 'c')
add_project_link_arguments('-lgui', '-lnet', '-lagx', '-lamc', '-lutils', language: 'c')
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
    install_dir: meson.get_cross_property('initrd_dir')
)
