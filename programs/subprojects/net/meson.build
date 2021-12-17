project('net', 'c')
executable(
    'net', 
    [
        'net.c',
        'util.c',
        'ethernet.c',
        'arp.c',
        'ipv4.c',
        'udp.c',
        'dns.c',
        'tcp.c',
        'callback.c',
    ],
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libgui').get_variable('libgui_dep'),
        subproject('libport').get_variable('libport_dep'),
        subproject('libamc').get_variable('libamc_dep'),
        subproject('libutils').get_variable('libutils_dep'),
        # Needed for RTL8139 headers
        subproject('realtek_8139_driver').get_variable('realtek_8139_driver_dep')
    ]
)
install_headers(
    ['net_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'net')
)
