project('pci_driver', 'c')
executable(
    'pci_driver', 
    'pci_driver.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libgui').get_variable('libgui_dep'),
        subproject('libport').get_variable('libport_dep'),
    ]
)
install_headers(
    ['pci_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'pci')
)
