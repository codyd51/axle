#ifndef CRASH_REPORTER_MESSAGES_H
#define CRASH_REPORTER_MESSAGES_H

#include <kernel/amc.h>

#define CRASH_REPORTER_SERVICE_NAME "com.axle.crash_reporter"

// Sent from clients to the crash reporter
#define CRASH_REPORTER_INFORM_ASSERT 100
typedef struct crash_reporter_inform_assert {
    uint32_t event; // CRASH_REPORTER_INFORM_ASSERT
    char assert_message[128];
} crash_reporter_inform_assert_t;

#endif
