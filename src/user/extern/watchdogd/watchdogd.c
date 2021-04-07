#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>

#include <kernel/amc.h>
#include <stdlibadd/sleep.h>
#include <stdlibadd/array.h>

#include <libamc/libamc.h>

#include "watchdogd.h"
#include "watchdogd_messages.h"

typedef struct proc {
	char* name;
	uint32_t last_ping_time;
	uint32_t last_response_time;
	uint32_t ping_count;
	uint32_t response_count;
} proc_t;

static array_t* procs = 0;

#define PING_INTERVAL_MS 5000
#define PING_INTERVAL_SECONDS (PING_INTERVAL_MS / 1000)
#define MAX_PING_RESPONSE_DELAY 3000

static proc_t* _proc_with_name(const char* name) {
	for (int i = 0; i < procs->size; i++) {
		proc_t* proc = array_lookup(procs, i);
		if (!strncmp(proc->name, name, AMC_MAX_SERVICE_NAME_LEN)) {
			return proc;
		}
	}
	return NULL;
}

static void _track_service_if_new(amc_service_description_t desc) {
	// Don't track ourselves
	if (!strncmp(desc.service_name, WATCHDOGD_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
		return;
	}
	proc_t* known_proc = _proc_with_name(desc.service_name);
	if (known_proc) {
		// This service is already known
		return;
	}
	// New service - track it
	proc_t* p = calloc(1, sizeof(proc_t));
	p->name = strndup(desc.service_name, AMC_MAX_SERVICE_NAME_LEN);
	p->last_response_time = 0;
	array_insert(procs, p);
	printf("%d watchdogd tracking new service %s\n", ms_since_boot(), p->name);
}

static void _ping_service_if_necessary(proc_t* proc) {
	// If it's been a while since we sent the last ping or received a ping 
	// response, send another
	if (ms_since_boot() >= proc->last_ping_time + PING_INTERVAL_MS &&
		ms_since_boot() >= proc->last_response_time + PING_INTERVAL_MS) {
		//printf("%d watchdogd pinging %s\n", ms_since_boot(), proc->name);
		proc->ping_count += 1;
		proc->last_ping_time = ms_since_boot();
		amc_msg_u32_1__send(proc->name, WATCHDOGD_LIVELINESS_PING);
	}
}

static void _alert_if_proc_is_hanging(proc_t* proc) {
	if (ms_since_boot() - proc->last_response_time >= PING_INTERVAL_MS) {
		printf("%d watchdogd warning: hanging proc: %s (last response %d, last ping %d)\n", ms_since_boot(), proc->name, proc->last_response_time, proc->last_ping_time);
	}
}

static void _perform_periodic_tasks(void) {
	for (int i = 0; i < procs->size; i++) {
		proc_t* proc = array_lookup(procs, i);
		_alert_if_proc_is_hanging(proc);
		_ping_service_if_necessary(proc);
	}
}

static void _handle_ping_response(proc_t* proc) {
	//printf("%d watchdogd received ping-ack from %s\n", ms_since_boot(), proc->name);
	proc->response_count += 1;
	proc->last_response_time = ms_since_boot();
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.watchdogd");
	procs = array_create(128);

	while (true) {
		amc_msg_u32_1__send(AXLE_CORE_SERVICE_NAME, AMC_COPY_SERVICES);
		do {
			amc_message_t* msg;
			amc_message_await_any(&msg);

			if (!strncmp(msg->source, AXLE_CORE_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
				uint32_t event = amc_msg_u32_get_word(msg, 0);
				assert(event == AMC_COPY_SERVICES_RESPONSE, "Expected amc service list");

				amc_service_list_t* services = (amc_service_list_t*)msg->body;
				for (int i = 0; i < services->service_count; i++) {
					amc_service_description_t desc = services->service_descs[i];
					_track_service_if_new(desc);
				}
			}
			else {
				proc_t* p = _proc_with_name(msg->source);
				if (!p) {
					printf("%d watchdogd received message from unknown service %s\n", ms_since_boot(), msg->source);
					continue;
				}
				_handle_ping_response(p);
			}

			/*
				printf("Got services from core:\n");
				for (int i = 0; i < services->service_count; i++) {
					printf("\tAMC service %s, unread msg count %d\n", desc.service_name, desc.unread_message_count);
					if (!strncmp(desc.service_name, WATCHDOGD_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
						continue;
					}
					amc_msg_u32_1__send(desc.service_name, WATCHDOGD_LIVELINESS_PING);
				}
			*/

		} while (amc_has_message());

		_perform_periodic_tasks();
		printf("%d watchdogd sleeping...\n", ms_since_boot());
		sleep(1);
	}

	return 0;
}
