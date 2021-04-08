#include <stdio.h>
#include <stdlib.h>

#include "include/daemons/timed/timed_messages.h"
#include <kernel/amc.h>

#include "sleep.h"
#include "assert.h"

unsigned sleep(unsigned int seconds) {
    // Ask timed to put us to sleep
    time_msg_sleep_t msg;
    msg.common.event = TIMED_REQ_SLEEP_FOR_MS;
    msg.ms = seconds * 1000;
    // TODO(PT): Roll libamc into stdlib so we can use _request_response_sync here
    amc_message_construct_and_send(TIMED_SERVICE_NAME, &msg, sizeof(time_msg_t));

    // Wait for the response
    while (true) {
        amc_message_t* resp;
        amc_message_await(TIMED_SERVICE_NAME, &resp);
        time_msg_sleep_t* awake = (time_msg_sleep_t*)&resp->body;
        if (awake->common.event == TIMED_RESP_AWAKE) {
            break;
        }
        printf("Discarding message from timed because it was not an awake signal: %d\n", awake->common.event);
    }
    return 0;
}
