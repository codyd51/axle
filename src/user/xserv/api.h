#ifndef XSERV_API_H
#define XSERV_API_H

#include <gfx/lib/window.h>

void xserv_win_create(Window* out, Rect frame);
void xserv_win_present(Window* win);
void xserv_win_destroy(Window* win);

#endif
