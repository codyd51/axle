project('image_viewer', 'c')
add_project_link_arguments('-lamc', '-lgui', '-lagx', '-limg', '-lutils', language: 'c')
executable(
    'image_viewer', 
    'image_viewer.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
install_headers(
    ['image_viewer_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'image_viewer')
)
