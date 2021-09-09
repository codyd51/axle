project('preferences', 'c')
add_project_link_arguments('-lagx', '-lgui', '-lamc', language: 'c')
executable(
    'preferences', 
    'preferences.c', 
    install: true,
    install_dir: meson.get_cross_property('initrd_dir')
)
install_headers(
    ['preferences_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'preferences')
)
