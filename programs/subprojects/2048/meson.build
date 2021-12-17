project('2048', 'c')
executable(
    '2048', 
    '2048.c', 
    install: true, 
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: [
        subproject('libagx').get_variable('libagx_dep'),
        subproject('libgui').get_variable('libgui_dep'),
        subproject('libamc').get_variable('libamc_dep'),
        subproject('libutils').get_variable('libutils_dep'),
    ]
)
