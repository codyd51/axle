#include <stdio.h>
#include <stdlib.h>

#include <kernel/amc.h>

#include "sleep.h"
#include "assert.h"

#define AXLE_CORE_SERVICE_NAME "com.axle.core"
#define AMC_SLEEP_UNTIL_TIMESTAMP 202

unsigned usleep(unsigned int ms) {
    uint32_t b[2];
    b[0] = AMC_SLEEP_UNTIL_TIMESTAMP;
    b[1] = ms;
    amc_message_send(AXLE_CORE_SERVICE_NAME, &b, sizeof(b));
    return 0;
}

unsigned sleep(unsigned int seconds) {
    return usleep(seconds * 1000);
}
