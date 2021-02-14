#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <kernel/amc.h>

// Layers and drawing
#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>
#include <agx/lib/text_box.h>

// Window management
#include <awm/awm.h>

// Communication with other processes
#include <libamc/libamc.h>
// Port IO
#include <libport/libport.h>

// Communication with NIC driver
#include <drivers/realtek_8139/rtl8139_messages.h>

#include "net.h"
#include "net_messages.h"

// Passing along frames
#include "ethernet.h"

// Displaying network stack state
#include "arp.h"
#include "dns.h"
#include "util.h"

// Many graphics lib functions call gfx_screen() 
Screen _screen = {0};
Screen* gfx_screen() {
	return &_screen;
}

static ca_layer* window_layer_get(uint32_t width, uint32_t height) {
	// Ask awm to make a window for us
	amc_msg_u32_3__send("com.axle.awm", AWM_REQUEST_WINDOW_FRAMEBUFFER, width, height);
	// And get back info about the window it made
	amc_command_ptr_message_t receive_framebuf = {0};
	amc_message_await("com.axle.awm", &receive_framebuf);
	// TODO(PT): Need a struct type selector
	if (amc_command_ptr_msg__get_command(&receive_framebuf) != AWM_CREATED_WINDOW_FRAMEBUFFER) {
		printf("Invalid state. Expected framebuffer command\n");
	}

	printf("Received framebuffer from awm: %d 0x%08x\n", amc_command_ptr_msg__get_command(&receive_framebuf), amc_command_ptr_msg__get_ptr(&receive_framebuf));
	uint32_t framebuffer_addr = receive_framebuf.body.cmd_ptr.ptr_val;
	uint8_t* buf = (uint8_t*)framebuffer_addr;

	// TODO(PT): Use an awm command to get screen info
	_screen.resolution = size_make(1920, 1080);
	_screen.physbase = (uint32_t*)0;
	_screen.bits_per_pixel = 32;
	_screen.bytes_per_pixel = 4;

	ca_layer* dummy_layer = malloc(sizeof(ca_layer));
	memset(dummy_layer, 0, sizeof(dummy_layer));
	dummy_layer->size = _screen.resolution;
	dummy_layer->raw = (uint8_t*)framebuffer_addr;
	dummy_layer->alpha = 1.0;
    _screen.vmem = dummy_layer;

	return dummy_layer;
}

int main(int argc, char** argv) {
	amc_register_service(NET_SERVICE_NAME);

	Size window_size = size_make(700, 600);
	Rect window_frame = rect_make(point_zero(), window_size);
	ca_layer* window_layer = window_layer_get(window_size.width, window_size.height);

	int text_box_padding = 6;
	Rect text_box_frame = rect_make(
		point_make(
			text_box_padding,
			text_box_padding
		),
		size_make(
			window_size.width - (text_box_padding * 2), 
			window_size.height - (text_box_padding * 2)
		)
	);
	draw_rect(window_layer, window_frame, color_orange(), THICKNESS_FILLED);

	text_box_t* text_box = text_box_create(text_box_frame.size, color_gray());
	text_box_puts(text_box, "Net stack\n", color_black());

    // Blit the text box to the window layer
    blit_layer(window_layer, text_box->layer, text_box_frame, rect_make(point_zero(), text_box_frame.size));
    // And ask awm to draw our window
    amc_command_msg__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);

	while (true) {
		net_message_t msg;
		do {
			// Wait until we've unblocked with at least one message available
			amc_message_await_any((amc_message_t*)&msg);
            const char* source_service = amc_message_source((amc_message_t*)&msg);

			if (!strcmp(source_service, RTL8139_SERVICE_NAME)) {
				uint8_t event = msg.event;
				if (event == NET_RX_ETHERNET_FRAME) {
					uint8_t* packet_data = (uint8_t*)&msg.data;
					ethernet_frame_t* eth_frame = malloc(msg.len);
					memcpy(eth_frame, packet_data, msg.len);
					ethernet_receive(eth_frame, msg.len);
				}
			}
			else {
				// Ignore messages from other services
				printf("Net stack received unknown message from %s\n", source_service);
				continue;
			}
		} while (amc_has_message());
		// We're out of messages to process
		// Wait for a new message to arrive on the next loop iteration

		// Draw the new ARP table
		text_box_clear(text_box, color_black());
		text_box_set_cursor_y(text_box,  0);
		text_box_puts(text_box, "ARP Table\n", color_white());
		arp_entry_t* arp_ents = arp_table();
		for (int i = 0; i < ARP_TABLE_SIZE; i++) {
			arp_entry_t* ent = &arp_ents[i];
			if (ent->allocated) {
				char buf[64];
				Color c = color_white();
				text_box_puts(text_box, "IP ", c);
				format_ipv4_address__buf(buf, sizeof(buf), ent->ip_addr);
				text_box_puts(text_box, buf, color_gray());

				text_box_puts(text_box, " = MAC ", c);
				format_mac_address(buf, sizeof(buf), ent->mac_addr);
				text_box_puts(text_box, buf, color_gray());
				text_box_puts(text_box, "\n", c);
			}
		}

		text_box_puts(text_box, "\nDNS Services Table\n", color_white());
		dns_service_type_t* dns_service_type_ents = dns_service_type_table();
		for (int i = 0; i < DNS_SERVICE_TYPE_TABLE_SIZE; i++) {
			dns_service_type_t* type_ent = &dns_service_type_ents[i];
			if (type_ent->allocated) {
				text_box_puts(text_box, "Type ", color_white());
				char buf[256];
				snprintf(buf, sizeof(buf), "%s\n", type_ent->type_name.name);
				text_box_puts(text_box, buf, color_gray());

				for (int j = 0; j < DNS_SERVICE_INSTANCE_TABLE_SIZE; j++) {
					dns_service_instance_t* inst_ent = &type_ent->instances[j];
					if (inst_ent->allocated) {
						text_box_puts(text_box, "\tInstance ", color_white());
						snprintf(buf, sizeof(buf), "%s\n", inst_ent->service_name.name);
						text_box_puts(text_box, buf, color_gray());
					}
				}
			}
		}

		blit_layer(window_layer, text_box->layer, text_box_frame, rect_make(point_zero(), text_box_frame.size));
		// And ask awm to draw our window
		amc_command_msg__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);
	}
	
	return 0;
}
