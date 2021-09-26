#include "doomgeneric.h"
#include "doomkeys.h"
#include "m_argv.h"

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

#include <libgui/libgui.h>
#include <stdlibadd/array.h>
#include <drivers/kb/kb_driver_messages.h>

uint32_t ms_since_boot();

uint32_t _g_start_time = 0;
array_t* _g_key_queue = NULL;

gui_window_t* _g_window = NULL;
gui_view_t* _g_view = NULL;
gui_layer_t* _g_doom_layer = NULL;

/*
 * Key events
 */

typedef struct key_event_desc {
  bool pressed;
  uint8_t val;
} key_event_desc_t;

static uint8_t translate_to_doom_key(uint32_t key) {
  switch (key) {
    case KEY_IDENT_UP_ARROW:
      key = KEY_UPARROW;
      break;
    case KEY_IDENT_DOWN_ARROW:
      key = KEY_DOWNARROW;
      break;
    case KEY_IDENT_LEFT_ARROW:
      key = KEY_LEFTARROW;
      break;
    case KEY_IDENT_RIGHT_ARROW:
      key = KEY_RIGHTARROW;
      break;
    case KEY_IDENT_ESCAPE:
      key = KEY_ESCAPE;
      break;
    case KEY_IDENT_LEFT_SHIFT:
    case KEY_IDENT_RIGHT_SHIFT:
      key = KEY_RSHIFT;
      break;
    case KEY_IDENT_LEFT_CONTROL:
      key = KEY_FIRE;
      break;
    case '\n':
      key = KEY_ENTER;
      break;
    case ' ':
      key = KEY_USE;
      break;
    default:
      key = tolower(key);
      break;
  }
  return key;
}

static void _handle_key_event(bool pressed, uint32_t ch) {
  key_event_desc_t* desc = calloc(1, sizeof(key_event_desc_t));
  desc->pressed = pressed;
  desc->val = translate_to_doom_key(ch);
  array_insert(_g_key_queue, desc);
}

static void _key_down(gui_view_t* view, uint32_t ch) {
  _handle_key_event(true, ch);
}

static void _key_up(gui_view_t* view, uint32_t ch) {
  _handle_key_event(false, ch);
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
  if (!_g_key_queue->size) {
    return 0;
  }

  key_event_desc_t* desc = array_lookup(_g_key_queue, 0);
  array_remove(_g_key_queue, 0);

  *pressed = desc->pressed;
  *doomKey = desc->val;
  free(desc);

  return 1;
}

/*
 * GUI handling
 */

static Rect _view_sizer(gui_view_t* view, Size window_size) {
  return rect_make(point_zero(), window_size);
}

void DG_Init() {
  amc_register_service("com.axle.doom");

	_g_window = gui_window_create("DOOM Loading...", DOOMGENERIC_RESX, DOOMGENERIC_RESY);
  _g_view = gui_view_create(_g_window, (gui_window_resized_cb_t)_view_sizer);
  _g_view->key_down_cb = (gui_key_down_cb_t)_key_down;
  _g_view->key_up_cb = (gui_key_up_cb_t)_key_up;
  _g_view->controls_content_layer = true;

  Size doom_screen_size = size_make(DOOMGENERIC_RESX, DOOMGENERIC_RESY);
  // Create a layer that points directly to DOOM's internal screen buffer
  _g_doom_layer = gui_layer_create(GUI_FIXED_LAYER, doom_screen_size);
  // And free the pixel buffer that was allocated for us
  free(_g_doom_layer->fixed_layer.inner->raw);
  _g_doom_layer->fixed_layer.inner->raw = (uint8_t*)DG_ScreenBuffer;

  _g_start_time = ms_since_boot();
  _g_key_queue = array_create(128);
}

void DG_DrawFrame() {
  bool did_exit = false;
  // Ask libgui to not block so we don't block the DOOM event loop
  gui_run_event_loop_pass(true, &did_exit);
  if (did_exit) {
    exit(0);
  }

  blit_layer_scaled(
    _g_view->content_layer->fixed_layer.inner,
    _g_doom_layer->fixed_layer.inner,
    _g_view->content_layer_frame.size
  );
}

/*
 * Misc. system hooks
 */

void DG_SleepMs(uint32_t ms) {
  usleep(ms);
}

uint32_t DG_GetTicksMs() {
  return ms_since_boot() - _g_start_time;
}

void DG_SetWindowTitle(const char * title) {
  gui_set_window_title((char*)title);
}
