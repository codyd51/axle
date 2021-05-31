#ifndef WATCHDOGD_MESSAGES_H
#define WATCHDOGD_MESSAGES_H

#include <kernel/amc.h>

#define WATCHDOGD_SERVICE_NAME "com.axle.watchdogd"

// Sent from watchdogd to any amc service
#define WATCHDOGD_LIVELINESS_PING   (1 << 2)

// Sent from any amc service to watchdogd
#define WATCHDOGD_LIVELINESS_PING_RESPONSE  (1 << 3)

#endif
