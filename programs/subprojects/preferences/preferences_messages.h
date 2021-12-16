#ifndef PREFERENCES_MESSAGES_H
#define PREFERENCES_MESSAGES_H

#include <kernel/amc.h>
#include <agx/lib/color.h>

#define PREFERENCES_SERVICE_NAME "com.axle.preferences"

typedef struct prefs_updated_msg {
    uint32_t event;
    Color from;
    Color to;
} prefs_updated_msg_t;

#endif