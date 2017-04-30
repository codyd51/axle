#ifndef BMP_H
#define BMP_H

#include <std/std_base.h>
#include <stdint.h>
#include "color.h"
#include "rect.h"
#include "ca_layer.h"

__BEGIN_DECLS

typedef struct bitmap {
	//common
	Rect frame;
	char needs_redraw;
	ca_layer* layer;
	struct view* superview;
} Bmp;

/**
 * @brief Construct a new Bmp container
 * @param frame Maximum dimensions of Bmp to create
 * @param layer The backing image data for the Bmp container
 * @return The newly constructed Bmp
 */
Bmp* create_bmp(Rect frame, ca_layer* layer);

/**
 * @brief Construct a Bmp using the image specified by @p filename
 * This function automatically detectss whether the provided file is a Bitmap or JPEG. 
 * It then decodes appropriately.
 * @param frame The size of Bmp to create
 * @param filename The file to look for bitmap or JPEG data in
 * @return The newly constructed image, or NULL if the file wasn't valid
 */
Bmp* load_bmp(Rect frame, char* filename);

/**
 * @brief Render a Bmp to a graphical layer
 * @param dest Graphical layer to render to
 * @param bmp The image data to render
 */
void draw_bmp(ca_layer* dest, Bmp* bmp);

/**
 * @brief Free all resources associated with a Bmp container
 * @param bmp The bitmap whose resources should be freed
 */
void bmp_teardown(Bmp* bmp);

__END_DECLS

#endif
