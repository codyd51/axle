project('file_manager', 'c')
add_project_link_arguments('-lport', '-lamc', '-lgui', '-lagx', '-limg', language: 'c')
executable(
    'file_manager', 
    'ata.c', 
    'fat.c', 
    'file_manager.c', 
    'initrd.c', 
    'ui.c', 
    'util.c', 
    'vfs.c', 
    'tests.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
install_headers(
    ['file_manager_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'file_manager')
)
