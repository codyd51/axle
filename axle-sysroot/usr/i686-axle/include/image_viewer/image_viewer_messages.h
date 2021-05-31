#ifndef IMAGE_VIEWER_MESSAGES_H
#define IMAGE_VIEWER_MESSAGES_H

#include <kernel/amc.h>

#define IMAGE_VIEWER_SERVICE_NAME "com.axle.image_viewer"

// Sent from clients to the image viewer
#define IMAGE_VIEWER_LOAD_IMAGE 1
typedef struct image_viewer_load_image_request {
    uint32_t event; // IMAGE_VIEWER_LOAD_IMAGE 
    char path[128];
} image_viewer_load_image_request_t;

#endif
