#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/adi.h>
#include <kernel/amc.h>
#include <kernel/idt.h>

#include "math.h"

typedef struct ps2_mouse_state {
	uint8_t idx;
	uint8_t buffer[3];
} ps2_mouse_state_t;

uint8_t inb(uint16_t port) {
	uint8_t _v;
	__asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" (port));
	return _v;
}

int main(int argc, char** argv) {
	// This process will handle PS/2 mouse IRQ's (IRQ 12)
	adi_register_driver("com.axle.mouse_driver", INT_VECTOR_IRQ12);
	amc_register_service("com.axle.mouse_driver");

	ps2_mouse_state_t state = {0, 0};
	while (true) {
		// Await an interrupt from the PS/2 mouse
		adi_interrupt_await(INT_VECTOR_IRQ12);

		// An interrupt is ready to be serviced!
		// TODO(PT): Copy the PS2 header to the sysroot as a build step, 
		// and replace this port number with PS2_DATA
		uint8_t data_packet = inb(0x60);

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
		else {
			uint8_t status_byte = state.buffer[0];
			// Invert the X and Y values if the status packet indicates they're negative
			int8_t rel_x = state.buffer[1] - ((status_byte << 4) & 0x100);
			// Always flip the Y axis as it arrives
			int8_t rel_y = -(state.buffer[2]) - ((status_byte << 5) & 0x100);

			if ((status_byte & 0x80) || (status_byte & 0x40)) {
				printf("Skipping packet with overflow set\n");
				state.idx = 0;
				memset(&state.buffer, 0, sizeof(state.buffer));
				continue;
			}

			//printf("Sending packet to awm: %d %d %d (%d %d)\n", status_byte, state.buffer[1], state.buffer[2], rel_x, rel_y);

			state.idx = 0;
			memset(&state.buffer, 0, sizeof(state.buffer));

			int8_t mouse_databuf[3] = {0};
			mouse_databuf[0] = status_byte;
			mouse_databuf[1] = rel_x;
			mouse_databuf[2] = rel_y;
			amc_message_t* amc_msg = amc_message_construct(&mouse_databuf, sizeof(mouse_databuf));
			amc_message_send("com.axle.awm", amc_msg);
		}
	}
	
	return 0;
}
