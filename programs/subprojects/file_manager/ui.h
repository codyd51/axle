#ifndef UI_H
#define UI_H

#include <agx/lib/shapes.h>
#include <agx/lib/rect.h>
#include <libgui/libgui.h>

#include "vfs.h"

typedef struct file_view file_view_t;

void file_manager_load_images(void);
Rect ui_content_view_sizer(gui_view_t* view, Size window_size);

void ui_generate_tree(gui_view_t* container_view, file_view_t* parent_view, uint32_t idx_within_parent, fs_node_t* node);

#endif