#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <kernel/amc.h>
#include <net/net_messages.h>
#include <libutils/assert.h>

#include "libnet.h"

void net_copy_local_mac_addr(uint8_t dest[MAC_ADDR_SIZE]) {
    net_message_t msg;
    msg.event = NET_RPC_COPY_LOCAL_MAC;
    msg.m.rpc.len = 0;
    amc_message_send(NET_SERVICE_NAME, &msg, sizeof(net_message_t));

    // Wait for the response
    amc_message_t* resp;
    amc_message_await(NET_SERVICE_NAME, &resp);
    net_message_t* net_msg = (net_message_t*)&resp->body;
    assert(net_msg->event == NET_RPC_RESPONSE_COPY_LOCAL_MAC, "Expected local MAC from net backend");
    assert(net_msg->m.rpc.len == MAC_ADDR_SIZE, "Expected message body to be MAC address size");
    memcpy(dest, net_msg->m.rpc.data, MAC_ADDR_SIZE);
}

void net_copy_local_ipv4_addr(uint8_t dest[IPv4_ADDR_SIZE]) {
    net_message_t msg;
    msg.event = NET_RPC_COPY_LOCAL_IPv4;
    msg.m.rpc.len = 0;
    amc_message_send(NET_SERVICE_NAME, &msg, sizeof(net_message_t));

    // Wait for the response
    amc_message_t* resp;
    amc_message_await(NET_SERVICE_NAME, &resp);
    net_message_t* net_msg = (net_message_t*)&resp->body;
    assert(net_msg->event == NET_RPC_RESPONSE_COPY_LOCAL_IPv4, "Expected local IPv4 from net backend");
    assert(net_msg->m.rpc.len == IPv4_ADDR_SIZE, "Expected message body to be IPv4 address size");
    memcpy(dest, net_msg->m.rpc.data, IPv4_ADDR_SIZE);
}

void net_copy_router_ipv4_addr(uint8_t dest[IPv4_ADDR_SIZE]) {
    net_message_t msg;
    msg.event = NET_RPC_COPY_ROUTER_IPv4;
    msg.m.rpc.len = 0;
    amc_message_send(NET_SERVICE_NAME, &msg, sizeof(net_message_t));

    // Wait for the response
    amc_message_t* resp;
    amc_message_await(NET_SERVICE_NAME, &resp);
    net_message_t* net_msg = (net_message_t*)&resp->body;
    assert(net_msg->event == NET_RPC_RESPONSE_COPY_ROUTER_IPv4, "Expected router IPv4 from net backend");
    assert(net_msg->m.rpc.len == IPv4_ADDR_SIZE, "Expected message body to be IPv4 address size");
    memcpy(dest, net_msg->m.rpc.data, IPv4_ADDR_SIZE);
}

void net_get_mac_from_ipv4(uint8_t ipv4_addr[IPv4_ADDR_SIZE], uint8_t out_mac_addr[MAC_ADDR_SIZE]) {
    uint32_t message_size = sizeof(net_message_t) + IPv4_ADDR_SIZE;
    net_message_t* msg = malloc(message_size);
    msg->event = NET_RPC_ARP_GET_MAC;
    msg->m.rpc.len = IPv4_ADDR_SIZE;
    memcpy(msg->m.rpc.data, ipv4_addr, IPv4_ADDR_SIZE);
    amc_message_send(NET_SERVICE_NAME, msg, message_size);
    free(msg);

    // Wait for the response
    amc_message_t* resp;
    amc_message_await(NET_SERVICE_NAME, &resp);
    net_message_t* net_msg = (net_message_t*)&resp->body;
    assert(net_msg->event == NET_RPC_RESPONSE_ARP_GET_MAC, "Expected ARP resolution from net backend");
    assert(net_msg->m.rpc.len == MAC_ADDR_SIZE, "Expected message body to be MAC address size");
    memcpy(out_mac_addr, net_msg->m.rpc.data, MAC_ADDR_SIZE);
}

void net_get_ipv4_of_domain_name(const char* domain_name, uint32_t domain_name_len, uint8_t out_ipv4[IPv4_ADDR_SIZE]) {
    uint32_t message_size = sizeof(net_message_t) + domain_name_len;
    net_message_t* msg = malloc(message_size);
    msg->event = NET_RPC_DNS_GET_IPv4;
    msg->m.rpc.len = domain_name_len;
    memcpy(msg->m.rpc.data, domain_name, domain_name_len);
    amc_message_send(NET_SERVICE_NAME, msg, message_size);
    free(msg);

    // Wait for the response
    amc_message_t* resp;
    amc_message_await(NET_SERVICE_NAME, &resp);
    net_message_t* net_msg = (net_message_t*)&resp->body;
    assert(net_msg->event == NET_RPC_RESPONSE_DNS_GET_IPv4, "Expected DNS answer from net backend");
    assert(net_msg->m.rpc.len == IPv4_ADDR_SIZE, "Expected message body to be IPv4 address size");
    memcpy(out_ipv4, net_msg->m.rpc.data, IPv4_ADDR_SIZE);
}

uint16_t net_find_free_port(void) {
    //return 12344;
    return rand();
}

uint32_t net_tcp_conn_init(uint16_t src_port, uint16_t dst_port, uint8_t dest_ipv4[IPv4_ADDR_SIZE]) {
    printf("net_tcp_conn_init src %d dst %d\n", src_port, dst_port);
    uint32_t message_size = sizeof(net_message_t);
    net_message_t* msg = malloc(message_size);
    msg->event = NET_RPC_TCP_OPEN;
    msg->m.tcp_open.src_port = src_port;
    msg->m.tcp_open.dst_port = dst_port;
    memcpy(msg->m.tcp_open.dst_ipv4, dest_ipv4, IPv4_ADDR_SIZE);
    amc_message_send(NET_SERVICE_NAME, msg, message_size);
    free(msg);

    // Wait for the response
    // TODO(PT): We should enter some kind of event loop to complete any libnet calls?
    // Or should we throw away anything that's not the one we're waiting on?
    amc_message_t* resp;
    amc_message_await(NET_SERVICE_NAME, &resp);
    net_message_t* net_msg = (net_message_t*)&resp->body;
    assert(net_msg->event == NET_RPC_RESPONSE_TCP_OPEN, "Expected TCP open response from net backend");
    return net_msg->m.tcp_open_response.tcp_conn_descriptor;
}

void net_tcp_conn_send(uint32_t conn_descriptor, uint8_t* data, uint32_t len) {
    printf("net_tcp_conn_send conn_descriptor %d len %d\n", conn_descriptor, len);
    uint32_t message_size = sizeof(net_message_t) + len;
    net_message_t* msg = malloc(message_size);
    msg->event = NET_RPC_TCP_SEND;
    msg->m.tcp_send.tcp_conn_descriptor = conn_descriptor;
    msg->m.tcp_send.len = len;
    memcpy(msg->m.tcp_send.data, data, len);
    amc_message_send(NET_SERVICE_NAME, msg, message_size);
    free(msg);
}

uint32_t net_tcp_conn_read(uint32_t conn_descriptor, uint8_t* buf, uint32_t len) {
    printf("net_tcp_conn_read %d 0x%08x %d\n", conn_descriptor, buf, len);
    uint32_t message_size = sizeof(net_message_t);
    net_message_t* msg = malloc(message_size);
    msg->event = NET_RPC_TCP_READ;
    msg->m.tcp_read.tcp_conn_descriptor = conn_descriptor;
    msg->m.tcp_read.len = len;
    amc_message_send(NET_SERVICE_NAME, msg, message_size);
    free(msg);

    // Wait for the response
    // TODO(PT): We should enter some kind of event loop to complete any libnet calls?
    // Or should we throw away anything that's not the one we're waiting on?
    amc_message_t* resp;
    amc_message_await(NET_SERVICE_NAME, &resp);
    net_message_t* net_msg = (net_message_t*)&resp->body;
    printf("net_msg->event = %d\n", net_msg->event);
    assert(net_msg->event == NET_RPC_RESPONSE_TCP_READ, "Expected TCP read response from net backend");
    memcpy(buf, net_msg->m.tcp_read_response.data, net_msg->m.tcp_read_response.len);
    return net_msg->m.tcp_read_response.len;
}

void format_mac_address(char* out, ssize_t out_size, uint8_t mac_addr[6]) {
	snprintf(
		out, 
		out_size, 
		"%02x:%02x:%02x:%02x:%02x:%02x", 
		mac_addr[0],
		mac_addr[1],
		mac_addr[2],
		mac_addr[3],
		mac_addr[4],
		mac_addr[5]
	);
}

void format_ipv4_address__buf(char* out, ssize_t out_size, uint8_t ip_addr[4]) {
	snprintf(
		out, 
		out_size, 
		"%d.%d.%d.%3d", 
		ip_addr[0],
		ip_addr[1],
		ip_addr[2],
		ip_addr[3]
	);
}
