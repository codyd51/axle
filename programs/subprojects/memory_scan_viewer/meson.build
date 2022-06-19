project('memory_scan_viewer', 'c')
executable(
    'memory_scan_viewer', 
    'memory_scan_viewer.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: subproject('libgui').get_variable('libgui_dep')
)
