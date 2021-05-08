#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>

#include <kernel/amc.h>
#include <stdlibadd/array.h>

#include <libamc/libamc.h>

#include "timed.h"
#include "timed_messages.h"

#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))

typedef struct asleep_proc {
	char* service_name;
	uint32_t sleep_start;
	uint32_t sleep_duration;
} asleep_proc_t;

static void wake_sleeping_procs(array_t* sleeping_procs) {
	int now = ms_since_boot();

	// To avoid modifying the array while iterating,
	// awake only one proc per runloop
	for (int i = 0; i < sleeping_procs->size; i++) {
		asleep_proc_t* p = array_lookup(sleeping_procs, i);
		if (now >= p->sleep_start + p->sleep_duration) {
			//printf("timed waking up %s at %d\n", p->service_name, now);
			time_msg_sleep_t msg;
			msg.common.event = TIMED_RESP_AWAKE;
			msg.ms = now - p->sleep_start; 
			amc_message_construct_and_send(p->service_name, &msg, sizeof(time_msg_t));

			array_remove(sleeping_procs, i);
			free(p->service_name);
			free(p);
			return;
		}
	}
}

static uint32_t _ms_to_soonest_wakeup(array_t* sleeping_procs) {
	uint32_t now = ms_since_boot();
	uint32_t soonest_wakeup = INT32_MAX;

	for (int i = 0; i < sleeping_procs->size; i++) {
		asleep_proc_t* p = array_lookup(sleeping_procs, i);
		if (now >= p->sleep_start + p->sleep_duration) {
			soonest_wakeup = 0;
			break;
		}
		soonest_wakeup = min(soonest_wakeup, p->sleep_start + p->sleep_duration - now);
	}
	return soonest_wakeup;
}

static void process_messages(array_t* asleep_procs) {
	amc_message_t* msg;
	do {
		amc_message_await_any(&msg);
		if (libamc_handle_message(msg)) {
			printf("timed caught watchdogd liveliness check\n");
			continue;
		}

		const char* source_service = amc_message_source(msg);
		//printf("timed got message from %s\n", source_service);
		time_msg_t* time_msg = (time_msg_t*)&msg->body;
		if (time_msg->sleep.common.event == TIMED_REQ_SLEEP_FOR_MS) {
			uint32_t duration = time_msg->sleep.ms;

			asleep_proc_t* proc = malloc(sizeof(asleep_proc_t));
			proc->service_name = strndup(source_service, AMC_MAX_SERVICE_NAME_LEN);
			proc->sleep_start = ms_since_boot();
			proc->sleep_duration = duration;
			array_insert(asleep_procs, proc);
			//printf("timed putting %s to sleep for %dms at %d\n", source_service, duration, proc->sleep_start);
		}
		else {
			printf("Unknown message from %s\n", source_service);
		}
	} while (amc_has_message());
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.timed");

	array_t* asleep_procs = array_create(128);
	bool should_block_until_next_message = false;
	while (true) {
		if (amc_has_message() || should_block_until_next_message) {
			process_messages(asleep_procs);
		}
		// If no processes are waiting to be woken up, there's no reason to 
		// execute this loop often, and we can instead wait for the next request
		// process_messages() will implicitly block until the next message arrives
		if (!asleep_procs->size) {
			process_messages(asleep_procs);
		}
		else {
			wake_sleeping_procs(asleep_procs);

			// If we have nothing to wake up, block until our next message
			should_block_until_next_message = asleep_procs->size == 0;
			if (!should_block_until_next_message) {
				uint32_t ms_to_next_wakeup = _ms_to_soonest_wakeup(asleep_procs);
				// Ask the kernel to not schedule us until the provided timestamp is hit,
				// or another amc message arrives
				amc_msg_u32_2__send(AXLE_CORE_SERVICE_NAME, AMC_TIMED_AWAIT_TIMESTAMP_OR_MESSAGE, ms_to_next_wakeup);
			}
		}
	}
	array_destroy(asleep_procs);

	return 0;
}
