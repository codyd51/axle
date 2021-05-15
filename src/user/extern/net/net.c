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
#include <libgui/libgui.h>

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
	uint8_t router_ip[IPv4_ADDR_SIZE] = {192, 168, 1, 1};
	memcpy(_net_config.router_ip_addr, router_ip, IPv4_ADDR_SIZE);
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
	gui_text_view_t* local_link_view;
	gui_text_view_t* arp_view;
	gui_text_view_t* dns_view;
	gui_text_view_t* dns_services_view;
} event_loop_state_t;

static event_loop_state_t _g_state = {0};

void net_ui_local_link_append_str(char* str, Color c) {
	gui_text_view_puts(_g_state.local_link_view, str, c);
}

void net_ui_arp_table_draw(void) {
	gui_text_view_t* text_view = _g_state.arp_view;
	//gui_text_view_clear_and_erase_history(text_view);

	gui_text_view_puts(text_view, "ARP Table\n", color_purple());
	arp_entry_t* arp_ents = arp_table();
	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
		arp_entry_t* ent = &arp_ents[i];
		if (ent->allocated) {
			char buf[64];
			Color c = color_white();
			gui_text_view_puts(text_view, "\tIP ", c);
			format_ipv4_address__buf(buf, sizeof(buf), ent->ip_addr);
			gui_text_view_puts(text_view, buf, color_gray());

			gui_text_view_puts(text_view, " = MAC ", c);
			format_mac_address(buf, sizeof(buf), ent->mac_addr);
			gui_text_view_puts(text_view, buf, color_gray());
			gui_text_view_puts(text_view, "\n", c);
		}
	}
}

void net_ui_dns_records_table_draw(void) {
	gui_text_view_t* text_view = _g_state.dns_view;
	//gui_text_view_clear_and_erase_history(text_view);

	gui_text_view_puts(text_view, "\nDNS A Records\n", color_purple());
	dns_domain_t* dns_domain_ents = dns_domain_records();
	for (int i = 0; i < DNS_DOMAIN_RECORDS_TABLE_SIZE; i++) {
		dns_domain_t* domain_ent = &dns_domain_ents[i];
		if (domain_ent->allocated) {
			char buf[256];
			snprintf(buf, sizeof(buf), "\t%s ", domain_ent->name.name);
			gui_text_view_puts(text_view, buf, color_white());

			format_ipv4_address__buf(buf, sizeof(buf), domain_ent->a_record);
			gui_text_view_puts(text_view, buf, color_gray());
			gui_text_view_puts(text_view, "\n", color_white());
		}
	}
}

void net_ui_dns_services_table_draw(void) {
	gui_text_view_t* text_view = _g_state.dns_services_view;
	//gui_text_view_clear_and_erase_history(text_view);

	gui_text_view_puts(text_view, "\nDNS Services\n", color_purple());
	dns_service_type_t* dns_service_type_ents = dns_service_type_table();
	for (int i = 0; i < DNS_SERVICE_TYPE_TABLE_SIZE; i++) {
		dns_service_type_t* type_ent = &dns_service_type_ents[i];
		if (type_ent->allocated) {
			gui_text_view_puts(text_view, "\tType ", color_white());
			char buf[256];
			snprintf(buf, sizeof(buf), "%s\n", type_ent->type_name.name);
			gui_text_view_puts(text_view, buf, color_gray());

			for (int j = 0; j < DNS_SERVICE_INSTANCE_TABLE_SIZE; j++) {
				dns_service_instance_t* inst_ent = &type_ent->instances[j];
				if (inst_ent->allocated) {
					gui_text_view_puts(text_view, "\t\tInstance ", color_white());
					snprintf(buf, sizeof(buf), "%s\n", inst_ent->service_name.name);
					gui_text_view_puts(text_view, buf, color_gray());
				}
			}
		}
	}
}


static Rect _local_link_view_sizer(gui_text_view_t* tv, Size window_size) {
	return rect_make(
		point_zero(), 
		size_make(
			window_size.width,
			(window_size.height / 5)
		)
	);
}

static Rect _arp_view_sizer(gui_text_view_t* tv, Size window_size) {
	return rect_make(
		point_make(
			0,
			rect_max_y(_g_state.local_link_view->frame)
		),
		size_make(
			window_size.width / 2,
			((window_size.height / 5) * 2)
		)
	);
}

static Rect _dns_view_sizer(gui_text_view_t* tv, Size window_size) {
	return rect_make(
		point_make(
			window_size.width / 2,
			rect_max_y(_g_state.local_link_view->frame)
		),
		size_make(
			window_size.width / 2,
			((window_size.height / 5) * 2)
		)
	);
}

static Rect _dns_services_view_sizer(gui_text_view_t* tv, Size window_size) {
	uint32_t local_link_max_y = rect_max_y(_g_state.local_link_view->frame);
	uint32_t arp_max_y = rect_max_y(_g_state.arp_view->frame);
	uint32_t height = ((window_size.height / 5) * 2);
	return rect_make(
		point_make(
			0,
			arp_max_y
		),
		size_make(
			window_size.width,
			height
		)
	);
}

static void _amc_message_received(gui_window_t* window, amc_message_t* msg) {
    const char* source_service = msg->source;
	if (!strncmp(source_service, RTL8139_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
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
	}
}

static gui_window_t* _g_window = NULL;

gui_window_t* net_main_window(void) {
	assert(_g_window != NULL, "Called net_main_window before window was initialized");
	return _g_window;
}

int main(int argc, char** argv) {
	amc_register_service(NET_SERVICE_NAME);

	// Allow protocol layers to perform their initialization
	arp_init();
	dns_init();
	tcp_init();

	_g_window = gui_window_create("Network Backend", 860, 400);

	_g_state.local_link_view = gui_text_view_create(
		_g_window,
		(gui_window_resized_cb_t)_local_link_view_sizer
	);
	_g_state.arp_view = gui_text_view_create(
		_g_window,
		(gui_window_resized_cb_t)_arp_view_sizer
	);
	_g_state.dns_view = gui_text_view_create(
		_g_window,
		(gui_window_resized_cb_t)_dns_view_sizer
	);
	_g_state.dns_services_view = gui_text_view_create(
		_g_window,
		(gui_window_resized_cb_t)_dns_services_view_sizer
	);

	// Ask the NIC for its configuration
	_read_nic_config(_nic_config_received);

	net_ui_local_link_append_str("Local Link\n", color_purple());
	char mac_buf[MAC_ADDR_SIZE];
	net_copy_local_mac_addr(mac_buf);
	net_ui_local_link_append_str("\tMAC   ", color_white());

	char text_buf[64];
	format_mac_address(text_buf, sizeof(text_buf), mac_buf);
	net_ui_local_link_append_str(text_buf, color_gray());
	net_ui_local_link_append_str("\n", color_white());

	char ipv4_buf[IPv4_ADDR_SIZE];
	net_copy_local_ipv4_addr(ipv4_buf);
	format_ipv4_address__buf(text_buf, sizeof(text_buf), ipv4_buf);
	net_ui_local_link_append_str("\tIPv4 ", color_white());
	net_ui_local_link_append_str(text_buf, color_gray());
	net_ui_local_link_append_str("\n", color_white());

	net_ui_arp_table_draw();
	net_ui_dns_records_table_draw();
	net_ui_dns_services_table_draw();

	gui_add_message_handler(_g_window, _amc_message_received);
	gui_enter_event_loop(_g_window);
	
	return 0;
}
