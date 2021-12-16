project('crash_reporter', 'c')
executable(
    'crash_reporter', 
    'crash_reporter.c',
    install: true,
    install_dir: meson.get_cross_property('initrd_dir'),
    dependencies: subproject('libgui').get_variable('libgui_dep')
)
install_headers(
    ['crash_reporter_messages.h'],
    install_dir: join_paths(meson.get_cross_property('include_root'), 'crash_reporter')
)

