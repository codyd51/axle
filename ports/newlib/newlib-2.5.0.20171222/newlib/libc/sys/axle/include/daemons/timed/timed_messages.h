#ifndef TIMED_MESSAGES_H
#define TIMED_MESSAGES_H

#include <kernel/amc.h>

#define TIMED_SERVICE_NAME "com.axle.timed"

// Sent from clients to be put to sleep
#define TIMED_REQ_SLEEP_FOR_MS (1 << 0)

// Sent from the time daemon to wake up clients
#define TIMED_RESP_AWAKE (1 << 1)

typedef struct time_msg_common {
    uint8_t event;
} time_msg_common_t;

typedef struct time_msg_sleep {
    time_msg_common_t common;
    uint32_t ms;
} time_msg_sleep_t;

typedef union time_msg {
    time_msg_sleep_t sleep;
} time_msg_t;

#endif
