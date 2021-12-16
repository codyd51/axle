project('cat', 'c', default_options : ['b_lto=true'])
executable(
    'cat', 
    'cat.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
