#ifndef CA_LAYER_H
#define CA_LAYER_H

#include <std/std_base.h>
#include <stdint.h>
#include "rect.h"
#include <std/array_l.h>
#include <std/list.h>

__BEGIN_DECLS

typedef struct ca_layer_t {
       	Size size; //width/height in pixels
       	uint8_t* raw; //raw RGB values backing this layer
		float alpha; //transparency value bounded to continuous range [0..1]
		List* clip_rects; //list of visible rects within layer that should be rendered
} ca_layer;

typedef struct clip_context {
	ca_layer* source_layer;
	Rect clip_rect;
	Point local_origin;
} clip_context_t;

/**
 * @brief initialize layer with a given size
 * @param size The maximum size the layer can render
 * @return The newly constructed graphical layer
 */
struct ca_layer_t* create_layer(Size size);

/**
 * @brief free all resources associated with a layer
 * @param layer The graphical layer whose resources should be freed
 * @warning No fields of 'layer' should be accessed after this call
 */
void layer_teardown(ca_layer* layer);

/**
 * @brief blit RGB contents of 'src' onto 'dest'
 * automatically switches to compositing if 'dest' needs ot be alpha blended
 * only copies pixels from the rectangle bounded by 'src_frame'
 * only copies pixels into the rectangle bounded by 'dest_frame'
 * @param dest Destination layer to copy pixels to
 * @param src Layer to copy pixels from
 * @param dest_frame Rectangle inset of @p dest which pixels should be copied into
 * @param src_frame Rectangle inset of @p src which pixels should be copied from
 */
void blit_layer(ca_layer* dest, ca_layer* src, Rect dest_frame, Rect src_frame);

//create a copy of layer pointed to by src
//only copies pixels bounded by the rectangle 'frame'
/**
 * @brief Copies pixels bounded by @frame into graphical layer @src
 * @param src Source layer to copy from
 * @param frame Rectangle inset of @p src which pixels should be copied from
 * @return The newly constructed layer containing the copied data
 */
ca_layer* layer_snapshot(ca_layer* src, Rect frame);

/**
 * @brief Add @p rect to layer's clip list
 * This function will also split all existing clip rectangles against
 * the newly added rect to prevent overlap.
 * When a layer is drawn, it will only draw regions in its clip list.
 */
void layer_add_clip_rect(ca_layer* layer, Rect added_clip_rect);
void layer_clear_clip_rects(ca_layer* layer);

__END_DECLS

#endif
