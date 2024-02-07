project('realtek_8139_driver', 'c')
realtek_8139_driver = executable(
    'realtek_8139_driver', 
    'realtek_8139_driver.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libgui').get_variable('libgui_dep'),
        subproject('libport').get_variable('libport_dep'),
        subproject('libamc').get_variable('libamc_dep'),
    ]
)
headers_dir = join_paths(meson.get_cross_property('include_root'), 'drivers', 'realtek_8139')
install_headers(
    ['rtl8139_messages.h'],
    install_dir: headers_dir
)
realtek_8139_driver_dep = declare_dependency(include_directories: headers_dir)
