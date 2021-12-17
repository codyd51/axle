project('image_viewer', 'c')
executable(
    'image_viewer', 
    'image_viewer.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libgui').get_variable('libgui_dep'),
        subproject('libimg').get_variable('libimg_dep'),
    ]
)
install_headers(
    ['image_viewer_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'image_viewer')
)
