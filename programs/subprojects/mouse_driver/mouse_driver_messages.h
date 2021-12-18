#ifndef MOUSE_DRIVER_MESSAGES_H
#define MOUSE_DRIVER_MESSAGES_H

#define MOUSE_DRIVER_SERVICE_NAME	"com.axle.mouse_driver"

#define MOUSE_PACKET 1
typedef struct mouse_packet_msg {
    uint32_t event;
    int8_t status;
    int8_t rel_x;
    int8_t rel_y;
    int8_t rel_z;
} mouse_packet_msg_t;

#endif
