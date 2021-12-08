project('pci_driver', 'c')
add_project_link_arguments('-lagx', '-lgui', '-lamc', '-lport', '-lutils', language: 'c')
executable(
    'pci_driver', 
    'pci_driver.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
install_headers(
    ['pci_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'pci')
)
