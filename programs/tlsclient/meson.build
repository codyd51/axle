project('tlsclient', 'c')
add_project_link_arguments('-lnet', '-lport', '-lgui', '-lagx', '-lamc', '-lutils', language: 'c')
executable(
    'tlsclient', 
    [
        'tlsclient.c',
        'tls.c',
        'asn1.c',
        'big_int.c',
    ],
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
