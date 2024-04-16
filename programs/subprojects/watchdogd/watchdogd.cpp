extern "C" {
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>

#include <kernel/amc.h>
#include <libutils/array.h>

#include <libamc/libamc.h>

void assert(bool cond, const char* msg);
}

#include "watchdogd.h"
#include "watchdogd_messages.h"

/*
#if defined (__cplusplus)
extern "C" {
#endif

void amc_register_service(const char* service_name);

#if defined (__cplusplus)
}
#endif
*/

int main(int argc, char** argv) {
	amc_register_service("com.axle.watchdogd");

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
				}
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
		sleep(1);
	}

	return 0;
}
