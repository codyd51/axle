#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/amc.h>

#include "mouse_driver.h"

typedef struct ps2_mouse_state {
	uint8_t idx;
	uint8_t buffer[3];
} ps2_mouse_state_t;

int main(int argc, char** argv) {
	amc_register_service("com.axle.mouse_driver");

	ps2_mouse_state_t state = {0, 0};

	while (true) {
		// The message from the low-level mouse driver will contain a data packet
		amc_message_t msg = {0};
		amc_message_await("com.axle.core", &msg);
		uint8_t data_packet = msg.data[0];
		state.buffer[state.idx] = data_packet;
		if (state.idx == 0) {
			state.idx += 1;

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
