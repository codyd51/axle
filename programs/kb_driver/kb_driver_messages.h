#ifndef KB_DRIVER_MESSAGES_H
#define KB_DRIVER_MESSAGES_H

#define KEY_IDENT_UP_ARROW		0x999
#define KEY_IDENT_DOWN_ARROW	0x998
#define KEY_IDENT_LEFT_ARROW	0x997
#define KEY_IDENT_RIGHT_ARROW	0x996

#define KB_DRIVER_SERVICE_NAME	"com.axle.kb_driver"

typedef enum key_event_type {
	KEY_PRESSED = 0,
	KEY_RELEASED = 1
} key_event_type_t;

typedef struct key_event {
	uint32_t key;
	key_event_type_t type;
} key_event_t;

#endif
