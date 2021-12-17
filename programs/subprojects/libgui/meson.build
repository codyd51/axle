project('libgui', 'c')
libgui = static_library(
    'gui', 
    [
        'gui_button.c',
        'gui_layer.c',
        'gui_scroll_view.c',
        'gui_scrollbar.c',
        'gui_slider.c',
        'gui_text_input.c',
        'gui_text_view.c',
        'gui_timer.c',
        'gui_view.c',
        'libgui.c',
        'utils.c',
    ],
    install: true,
    install_dir: meson.get_cross_property('libraries_root'),
    dependencies: [
        subproject('libagx').get_variable('libagx_dep'), 
        subproject('libamc').get_variable('libamc_dep'), 
        subproject('libutils').get_variable('libutils_dep')
    ]
)
headers_dir = join_paths(meson.get_cross_property('include_root'), 'libgui')
install_headers(
    [
        'gui_elem.h',
        'gui_button.h',
        'gui_layer.h',
        'gui_scroll_view.h',
        'gui_scrollbar.h',
        'gui_slider.h',
        'gui_text_input.h',
        'gui_text_view.h',
        'gui_timer.h',
        'gui_view.h',
        'libgui.h',
        'utils.h',
    ],
    install_dir: headers_dir
)
libgui_dep = declare_dependency(include_directories: headers_dir, link_with: libgui)
