project('net', 'c')
add_project_link_arguments('-lagx', '-lgui', '-lamc', '-lport', language: 'c')
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
    install_dir: meson.get_cross_property('initrd_dir')
)
install_headers(
    ['net_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'net')
)
