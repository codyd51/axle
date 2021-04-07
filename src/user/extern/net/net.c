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

// Displaying network stack state and servicing RPCs
#include "arp.h"
#include "dns.h"
#include "tcp.h"
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

static void _send_nic_config_info_request(void) {
	printf("Net sending NIC config info request...\n");
	net_message_t config_msg;
	config_msg.event = NET_REQUEST_NIC_CONFIG;
	amc_message_construct_and_send(RTL8139_SERVICE_NAME, &config_msg, sizeof(net_nic_config_info_t));
}

static void _nic_config_received(net_nic_config_info_t* config) {
	printf("NIC config received! Announcing ourselves on the network...\n");
	// Save the NIC configuration
	memcpy(_net_config.nic_mac, config->mac_addr, MAC_ADDR_SIZE);
	// Set up a static IP for now
	uint8_t static_ip[IPv4_ADDR_SIZE] = {192, 168, 1, 84};
	memcpy(_net_config.ip_addr, static_ip, IPv4_ADDR_SIZE);
	// Announce ourselves to the network
	arp_announce();
	// Request the MAC of the router (at a known static IP)
	uint8_t router_ip[IPv4_ADDR_SIZE] = {192, 168, 1, 254};
	memcpy(_net_config.router_ip_addr, router_ip, IPv4_ADDR_SIZE);
	// TODO(PT): The ARP request should be in a libnet that gets unblocked when the response is received
	arp_request_mac(router_ip);
}

static void _read_nic_config(void(*callback)(net_nic_config_info_t*)) {
	_send_nic_config_info_request();
	for (int i = 0; i < 20; i++) {
		do {
			amc_message_t* msg;
			amc_message_await(RTL8139_SERVICE_NAME, &msg);
			net_message_t* config = (net_message_t*)&msg->body;
			printf("Got Realtek msg %d\n", config->event);
			if (config->event == NET_RESPONSE_NIC_CONFIG) {
				callback(&config->m.config_info);
				return;
			}
		} while (amc_has_message_from(RTL8139_SERVICE_NAME));
		// We haven't received a response from the NIC yet
		// Resend our request and wait for a bit
		// The NIC might not have been alive when we messaged it originally
		_send_nic_config_info_request();
		printf("Net will sleep..\n");
		sleep(1);
		printf("Net slept!\n");
	}

	assert(false, "Tried to send NIC config request too many times!");
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

void net_send_rpc_response(const char* service, uint32_t event, void* buf, uint32_t buf_size) {
	uint32_t response_size = sizeof(net_message_t) + buf_size;
	net_message_t* response = malloc(response_size);
	response->event = event;
	response->m.rpc.len = buf_size;
	if (buf && buf_size) {
		memcpy(response->m.rpc.data, buf, buf_size);
	}
	amc_message_construct_and_send(service, response, response_size);
	free(response);
}

typedef struct event_loop_state {
	text_box_t* info_text_box;
} event_loop_state_t;

static void _process_amc_messages(event_loop_state_t* state) {
	do {
		amc_message_t* msg;
		// Wait until we've unblocked with at least one message available
		amc_message_await_any(&msg);
		if (libamc_handle_message(msg)) {
			continue;
		}

		const char* source_service = amc_message_source(msg);

		if (!strcmp(source_service, RTL8139_SERVICE_NAME)) {
			net_message_t* packet_msg = (net_message_t*)&msg->body;
			uint8_t event = packet_msg->event;
			if (event == NET_RX_ETHERNET_FRAME) {
				uint8_t* packet_data = (uint8_t*)packet_msg->m.packet.data;
				ethernet_frame_t* eth_frame = malloc(packet_msg->m.packet.len);
				memcpy(eth_frame, packet_data, packet_msg->m.packet.len);
				packet_info_t packet_info;
				// ethernet_receive is responsible for freeing the packet
				ethernet_receive(&packet_info, eth_frame, packet_msg->m.packet.len);
			}
		}
		else if (!strncmp(source_service, AWM_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
			uint32_t cmd = amc_msg_u32_get_word(msg, 0);
			if (cmd == AWM_MOUSE_SCROLLED) {
				awm_mouse_scrolled_msg_t* m = (awm_mouse_scrolled_msg_t*)msg->body;
				bool scroll_up = m->delta_z > 0;
				int interval = scroll_up ? 10 : -10;
				for (uint32_t i = 0; i < abs(m->delta_z); i++) {
					//scrolling_layer->scroll_offset.height += interval;
					if (scroll_up) text_box_scroll_up(state->info_text_box);
					else text_box_scroll_down(state->info_text_box);
				}
			}

			// Ignore awm events
			continue;
		}

		else {
			printf("Net backend servicing RPC from %s\n", source_service);
			net_message_t* packet_msg = (net_message_t*)&msg->body;
			if (packet_msg->event == NET_RPC_COPY_LOCAL_MAC) {
				uint8_t mac[MAC_ADDR_SIZE];
				net_copy_local_mac_addr(mac);
				net_send_rpc_response(source_service, NET_RPC_RESPONSE_COPY_LOCAL_MAC, mac, MAC_ADDR_SIZE);
			}
			else if (packet_msg->event == NET_RPC_COPY_LOCAL_IPv4) {
				uint8_t ipv4[IPv4_ADDR_SIZE];
				net_copy_local_ipv4_addr(ipv4);
				net_send_rpc_response(source_service, NET_RPC_RESPONSE_COPY_LOCAL_IPv4, ipv4, IPv4_ADDR_SIZE);
			}
			else if (packet_msg->event == NET_RPC_COPY_ROUTER_IPv4) {
				uint8_t ipv4[IPv4_ADDR_SIZE];
				net_copy_router_ipv4_addr(ipv4);
				net_send_rpc_response(source_service, NET_RPC_RESPONSE_COPY_ROUTER_IPv4, ipv4, IPv4_ADDR_SIZE);
			}
			else if (packet_msg->event == NET_RPC_ARP_GET_MAC) {
				// COPY_ROUTER_MAC should do the same thing for the fixed router IP
				assert(packet_msg->m.packet.len == IPv4_ADDR_SIZE, "Provided data wasn't the size of an IPv4 address");
				arp_perform_amc_rpc__discover_mac(source_service, (uint8_t(*)[IPv4_ADDR_SIZE])packet_msg->m.packet.data);
			}
			else if (packet_msg->event == NET_RPC_DNS_GET_IPv4) {
				uint32_t domain_name_len = packet_msg->m.packet.len;
				dns_perform_amc_rpc__discover_ipv4(source_service, packet_msg->m.packet.data, domain_name_len+1);
			}
			else if (packet_msg->event == NET_RPC_TCP_OPEN) {
				printf("net_rpc_tcp_open %d %d\n", packet_msg->m.tcp_open.src_port, packet_msg->m.tcp_open.dst_port);
				tcp_perform_amc_rpc__conn_open(source_service, &packet_msg->m.tcp_open);
			}
			else if (packet_msg->event == NET_RPC_TCP_SEND) {
				printf("net_rpc_tcp_send %d %d\n", packet_msg->m.tcp_send.tcp_conn_descriptor, packet_msg->m.tcp_send.len);
				tcp_perform_amc_rpc__conn_send(source_service, &packet_msg->m.tcp_send);
			}
			else if (packet_msg->event == NET_RPC_TCP_READ) {
				printf("net_rpc_tcp_read %d %d\n", packet_msg->m.tcp_read.tcp_conn_descriptor, packet_msg->m.tcp_read.len);
				tcp_perform_amc_rpc__conn_read(source_service, &packet_msg->m.tcp_read);
			}
			else {
				printf("Unknown RPC %d\n", packet_msg->event);
			}
			continue;
		}
	} while (amc_has_message());
}

int main(int argc, char** argv) {
	amc_register_service(NET_SERVICE_NAME);

	// TODO(PT): Come up with a better way of showing multiple columns
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
	draw_rect(window_layer, window_frame, color_orange(), text_box_padding);

	event_loop_state_t state = {0};
	text_box_t* text_box = text_box_create(text_box_frame.size, color_black());
	state.info_text_box = text_box;
	text_box_puts(text_box, "Net backend initializing...\n", color_white());

    // Blit the text box to the window layer
	ca_scrolling_layer_blit(
		text_box->scroll_layer, 
		rect_make(point_zero(), text_box_frame.size), 
		window_layer, 
		text_box_frame
	);
    // And ask awm to draw our window
	amc_msg_u32_1__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);

	// Ask the NIC for its configuration
	_read_nic_config(_nic_config_received);

	// Allow protocol layers to perform their initialization
	arp_init();
	dns_init();
	tcp_init();

	amc_message_t* msg;
	while (true) {
		_process_amc_messages(&state);
		// We're out of messages to process
		// Wait for a new message to arrive on the next loop iteration

		Size orig_scroll_position = text_box->scroll_layer->scroll_offset;
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

		// Keep the scroll position the same now that we've redrawn the text box contents
		text_box->scroll_layer->scroll_offset = orig_scroll_position;
		ca_scrolling_layer_blit(
			text_box->scroll_layer, 
			rect_make(point_zero(), text_box_frame.size), 
			window_layer, 
			text_box_frame
		);
		// And ask awm to draw our window
		amc_msg_u32_1__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);

		printf("Net finished run-loop\n");
	}
	
	return 0;
}
