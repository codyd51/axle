project('preferences', 'c')
executable(
    'preferences', 
    'preferences.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: subproject('libgui').get_variable('libgui_dep')
)
install_headers(
    ['preferences_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'preferences')
)
