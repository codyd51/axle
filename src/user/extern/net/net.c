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
	amc_message_t* receive_framebuf;
	amc_message_await("com.axle.awm", &receive_framebuf);
	uint32_t event = amc_msg_u32_get_word(receive_framebuf, 0);
	if (event != AWM_CREATED_WINDOW_FRAMEBUFFER) {
		printf("Invalid state. Expected framebuffer command\n");
	}
	uint32_t framebuffer_addr = amc_msg_u32_get_word(receive_framebuf, 1);

	printf("Received framebuffer from awm: %d 0x%08x\n", event, framebuffer_addr);
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

static net_config_t _net_config = {0};

static void _send_nic_config_info_request(int* transmission_count) {
	if (!transmission_count) {
		return;
	}
	net_nic_config_info_t config_msg;
	config_msg.common.event = NET_REQUEST_NIC_CONFIG;
	amc_message_construct_and_send(RTL8139_SERVICE_NAME, &config_msg, sizeof(net_nic_config_info_t));
	(*transmission_count)--;
}

static void _read_nic_config(void) {
	int transmission_count = 20;
	_send_nic_config_info_request(&transmission_count);
	amc_message_t* msg;
	while (true) {
		amc_message_await(RTL8139_SERVICE_NAME, &msg);
		net_nic_config_info_t* resp = &msg->body;
		if (resp->common.event == NET_RESPONSE_NIC_CONFIG) {
			printf("NIC config received! Announcing ourselves on the network...\n");
			// Save the NIC configuration
			memcpy(_net_config.nic_mac, resp->mac_addr, MAC_ADDR_SIZE);
			// Set up a static IP for now
			uint8_t static_ip[IPv4_ADDR_SIZE] = {192, 168, 1, 84};
			memcpy(_net_config.ip_addr, static_ip, IPv4_ADDR_SIZE);
			// Announce ourselves to the network
			arp_announce();
			// Request the MAC of the router (at a known static IP)
			uint8_t router_ip[IPv4_ADDR_SIZE] = {192, 168, 1, 254};
			memcpy(_net_config.router_ip_addr, router_ip, IPv4_ADDR_SIZE);
			arp_request_mac(router_ip);
			break;
		}
		printf("Discarding message from NIC because it was the wrong type: %d\n", resp->common.event);
		// Send the request again in case the NIC wasn't alive when we messaged it originally
		_send_nic_config_info_request(&transmission_count);
	}
}

void net_copy_local_mac_addr(uint8_t dest[MAC_ADDR_SIZE]) {
	memcpy(dest, _net_config.nic_mac, MAC_ADDR_SIZE);
}

void net_copy_local_ipv4_addr(uint8_t dest[IPv4_ADDR_SIZE]) {
	memcpy(dest, _net_config.ip_addr, IPv4_ADDR_SIZE);
}

uint32_t net_copy_local_ipv4_addr__u32(void) {
	return _net_config.ip_addr[3] << 24 |
		   _net_config.ip_addr[2] << 16 |
		   _net_config.ip_addr[1] << 8 |
		   _net_config.ip_addr[0] << 0;
}

void net_copy_router_ipv4_addr(uint8_t dest[IPv4_ADDR_SIZE]) {
	memcpy(dest, _net_config.router_ip_addr, IPv4_ADDR_SIZE);
}

int main(int argc, char** argv) {
	amc_register_service(NET_SERVICE_NAME);

	Size window_size = size_make(720, 640);
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

	text_box_t* text_box = text_box_create(text_box_frame.size, color_black());
	text_box_puts(text_box, "Net backend initializing...\n", color_white());

    // Blit the text box to the window layer
    blit_layer(window_layer, text_box->layer, text_box_frame, rect_make(point_zero(), text_box_frame.size));
    // And ask awm to draw our window
	amc_msg_u32_1__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);

	// Ask the NIC for its configuration
	_read_nic_config();

	amc_message_t* msg;
	while (true) {
		do {
			// Wait until we've unblocked with at least one message available
			amc_message_await_any(&msg);
            const char* source_service = amc_message_source(msg);

			if (!strcmp(source_service, RTL8139_SERVICE_NAME)) {
				net_packet_t* packet_msg = (net_packet_t*)msg->body;
				uint8_t event = packet_msg->common.event;
				if (event == NET_RX_ETHERNET_FRAME) {
					uint8_t* packet_data = (uint8_t*)packet_msg->data;
					ethernet_frame_t* eth_frame = malloc(packet_msg->len);
					memcpy(eth_frame, packet_data, packet_msg->len);
					packet_info_t packet_info;
					// ethernet_receive is responsible for freeing the packet
					ethernet_receive(&packet_info, eth_frame, packet_msg->len);
				}
			}
			else if (!strcmp(source_service, "com.axle.awm")) {
				uint32_t cmd = amc_msg_u32_get_word(msg, 0);
				char ch = amc_msg_u32_get_word(msg, 1);

				if (cmd == AWM_KEY_DOWN) {
					if (ch == 'r') {
						printf("scroll up\n");
						text_box_scroll_up(text_box);
					}
					else if (ch == 's') {
						printf("scroll down\n");
						text_box_scroll_down(text_box);
					}
					else if (ch == 't') {
						text_box_scroll_to_bottom(text_box);
					}
					else if (ch == 'q') {
						// Holding the send key on dns_send crashes,
						// But holding the send key on the simpler ethernet call
						// does not crash.
						dns_send();
						/*
						const uint8_t dst = {10, 0, 0, 1};
						char wrapper[40];
						ipv4_send(dst, 0x11, wrapper, 40);

						/*
						uint8_t router_mac[MAC_ADDR_SIZE] = {0x34, 0x27, 0x92, 0x36, 0x8c, 0x61};
						char wrapper[40];
						ethernet_send(router_mac, ETHTYPE_IPv4, wrapper, 40);
						*/
					}
				}
			}

			else {
				// Ignore messages from other services
				printf("Net stack received unknown message from %s\n", source_service);
				continue;
			}

			// Draw the new ARP table
			text_box_clear(text_box);
			text_box_set_cursor_y(text_box,  0);

			text_box_puts(text_box, "Local Link\n", color_white());

			char text_buf[64];
			char mac_buf[MAC_ADDR_SIZE];
			net_copy_local_mac_addr(mac_buf);
			format_mac_address(text_buf, sizeof(text_buf), mac_buf);
			text_box_puts(text_box, "\tMAC  ", color_white());
			text_box_puts(text_box, text_buf, color_gray());
			text_box_puts(text_box, "\n", color_white());

			char ipv4_buf[IPv4_ADDR_SIZE];
			net_copy_local_ipv4_addr(ipv4_buf);
			format_ipv4_address__buf(text_buf, sizeof(text_buf), ipv4_buf);
			text_box_puts(text_box, "\tIPv4 ", color_white());
			text_box_puts(text_box, text_buf, color_gray());
			text_box_puts(text_box, "\n\n", color_gray());

			text_box_puts(text_box, "ARP Table\n", color_white());
			arp_entry_t* arp_ents = arp_table();
			for (int i = 0; i < ARP_TABLE_SIZE; i++) {
				arp_entry_t* ent = &arp_ents[i];
				if (ent->allocated) {
					char buf[64];
					Color c = color_white();
					text_box_puts(text_box, "\tIP ", c);
					format_ipv4_address__buf(buf, sizeof(buf), ent->ip_addr);
					text_box_puts(text_box, buf, color_gray());

					text_box_puts(text_box, " = MAC ", c);
					format_mac_address(buf, sizeof(buf), ent->mac_addr);
					text_box_puts(text_box, buf, color_gray());
					text_box_puts(text_box, "\n", c);
				}
			}

			text_box_puts(text_box, "\nDNS Records\n", color_white());
			dns_domain_t* dns_domain_ents = dns_domain_records();
			for (int i = 0; i < DNS_DOMAIN_RECORDS_TABLE_SIZE; i++) {
				dns_domain_t* domain_ent = &dns_domain_ents[i];
				if (domain_ent->allocated) {
					char buf[256];
					snprintf(buf, sizeof(buf), "\t%s ", domain_ent->name.name);
					text_box_puts(text_box, buf, color_white());

					format_ipv4_address__buf(buf, sizeof(buf), domain_ent->a_record);
					text_box_puts(text_box, buf, color_gray());
					text_box_puts(text_box, "\n", color_white());
				}
			}

			text_box_puts(text_box, "\nDNS Services\n", color_white());
			dns_service_type_t* dns_service_type_ents = dns_service_type_table();
			for (int i = 0; i < DNS_SERVICE_TYPE_TABLE_SIZE; i++) {
				dns_service_type_t* type_ent = &dns_service_type_ents[i];
				if (type_ent->allocated) {
					text_box_puts(text_box, "\tType ", color_white());
					char buf[256];
					snprintf(buf, sizeof(buf), "%s\n", type_ent->type_name.name);
					text_box_puts(text_box, buf, color_gray());

					for (int j = 0; j < DNS_SERVICE_INSTANCE_TABLE_SIZE; j++) {
						dns_service_instance_t* inst_ent = &type_ent->instances[j];
						if (inst_ent->allocated) {
							text_box_puts(text_box, "\t\tInstance ", color_white());
							snprintf(buf, sizeof(buf), "%s\n", inst_ent->service_name.name);
							text_box_puts(text_box, buf, color_gray());
						}
					}
				}
			}
		} while (amc_has_message());
		// We're out of messages to process
		// Wait for a new message to arrive on the next loop iteration

		printf("Net finished run-loop\n");
		blit_layer(window_layer, text_box->layer, text_box_frame, rect_make(point_zero(), text_box_frame.size));
		// And ask awm to draw our window
		amc_msg_u32_1__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);
	}
	
	return 0;
}
