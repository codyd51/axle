#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/adi.h>
#include <kernel/amc.h>
#include <kernel/idt.h>

#include <libport/libport.h>
#include <awm/awm_messages.h>

#include "math.h"
#include "mouse_driver_messages.h"

typedef struct ps2_mouse_state {
	uint8_t idx;
	uint8_t buffer[4];
} ps2_mouse_state_t;

static _handle_amc_messages(void) {
	if (!amc_has_message()) {
		return;
	}
	do {
		amc_message_t* msg;
		amc_message_await_any(&msg);
		if (!libamc_handle_message(msg)) {
			printf("com.axle.mouse_driver received unknown amc message from %s\n", msg->source);
		}
	} while (amc_has_message());
}

int main(int argc, char** argv) {
	amc_register_service(MOUSE_DRIVER_SERVICE_NAME);
	// This process will handle PS/2 mouse IRQ's (IRQ 12)
	adi_register_driver(MOUSE_DRIVER_SERVICE_NAME, INT_VECTOR_APIC_12);

	ps2_mouse_state_t state = {0, 0};
	while (true) {
		// Await an interrupt from the PS/2 mouse
		bool awoke_for_interrupt = adi_event_await(INT_VECTOR_APIC_12);
		if (!awoke_for_interrupt) {
			_handle_amc_messages();
			continue;
		}

		// An interrupt is ready to be serviced!
		// TODO(PT): Copy the PS2 header to the sysroot as a build step, 
		// and replace this port number with PS2_DATA
		uint8_t data_packet = inb(0x60);
		adi_send_eoi(INT_VECTOR_APIC_12);

		state.buffer[state.idx] = data_packet;
		if (state.idx == 0) {
			state.idx += 1;

			if (!((data_packet >> 3) & 0x1)) {
				// This bit should always be one - we are misaligned!
				// Ignore this packet to maintain alignment
				// TODO(PT): Could this be caused by another mouse interrupt before we're done processing one?
				printf("*** Unaligned mouse packet\n");
				state.idx = 0;
				memset(&state.buffer, 0, sizeof(state.buffer));
				continue;
			}

			// This byte contains info about button events
			/*
			bool middle_button_pressed = data_packet & (1 << 2);
			bool right_button_pressed = data_packet & (1 << 1);
			bool left_button_pressed = data_packet & (1 << 0);
			*/
		}
		else if (state.idx == 1) {
			state.idx += 1;
		}
		else if (state.idx == 2) {
			state.idx += 1;
		}
		else {
			uint8_t status_byte = state.buffer[0];
			// Invert the X and Y values if the status packet indicates they're negative
			int8_t rel_x = state.buffer[1] - ((status_byte << 4) & 0x100);
			// Always flip the Y axis as it arrives
			int8_t rel_y = -(state.buffer[2]) - ((status_byte << 5) & 0x100);
			int8_t rel_z = (state.buffer[3]);

			if ((status_byte & 0x80) || (status_byte & 0x40)) {
				printf("Skipping packet with overflow set\n");
				state.idx = 0;
				memset(&state.buffer, 0, sizeof(state.buffer));
				continue;
			}

			state.idx = 0;
			memset(&state.buffer, 0, sizeof(state.buffer));

			mouse_packet_msg_t msg = {
				.event = MOUSE_PACKET,
				.status = status_byte,
				.rel_x = (int16_t)rel_x,
				.rel_y = (int16_t)rel_y,
				.rel_z = rel_z
			};
			amc_message_send(AWM_SERVICE_NAME, &msg, sizeof(msg));
		}
	}
	
	return 0;
}
