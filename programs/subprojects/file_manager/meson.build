project('file_manager', 'c')
executable(
    'file_manager', 
    'ata.c', 
    'fat.c', 
    'file_manager.c', 
    'initrd.c', 
    'ui.c', 
    'util.c', 
    'vfs.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libgui').get_variable('libgui_dep'),
        subproject('libport').get_variable('libport_dep'),
        subproject('libimg').get_variable('libimg_dep'),
    ]
)
install_headers(
    ['file_manager_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'file_manager')
)
