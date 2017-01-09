#ifndef XSERV_H
#define XSERV_H

#include <gfx/lib/gfx.h>
#include <gfx/lib/shapes.h>

//initializes xserv process
void xserv_init();
//translates view's frame into root screen's coordinate space
Rect absolute_frame(Screen* screen, View* view);
//stops (but doesn't teardown) xserv and switches back to text mode
void xserv_pause();
//resumes xserv from text mode (but assumes current xserv wasn't torn down)
void xserv_resume();
//pauses xserv for pause_length seconds, then resumes
void xserv_temp_stop(uint32_t pause_length);
//displays error message for informing user of heap corruption
void xserv_fail();

#endif
