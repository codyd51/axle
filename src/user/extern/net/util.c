#include <stdio.h>

#include "util.h"

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

void format_ipv4_address__u32(char* out, ssize_t out_size, uint32_t ip) {
    uint8_t buf[4];
    buf[0] = (uint8_t)((ip >> 0) & 0xff);
    buf[1] = (uint8_t)((ip >> 8) & 0xff);
    buf[2] = (uint8_t)((ip >> 16) & 0xff);
    buf[3] = (uint8_t)((ip >> 24) & 0xff);
    format_ipv4_address__buf(out, out_size, buf);
}

void hexdump(const void* addr, const int len) {
    const char* desc = "";
    int i;
    unsigned char buff[65];
    const unsigned char * pc = (const unsigned char *)addr;

    // Output description if given.

    if (desc != NULL)
        printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    else if (len < 0) {
        printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }

    // Process every byte in the data.

    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 64) == 0) {
            // Don't print ASCII buffer for the "zeroth" line.

            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.

            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        //printf ("%02x", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % 64] = '.';
        else
            buff[i % 64] = pc[i];
        buff[(i % 64) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.

    while ((i % 64) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    printf ("  %s\n", buff);
}

static uint32_t _net_checksum_add(uint8_t* addr, int count) {
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)addr;

	// Sum the uint16_t words in the buffer
    while (count > 1)  {
        sum += *ptr++;
        count -= 2;
    }

	// Add the left over byte, if any
    if (count > 0) {
        sum += *(uint8_t*) ptr;
	}
    return sum;
}

static uint16_t _net_checksum_finish(uint32_t sum) {
	// Fold 32-bit sum into 16-bits
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	// Finally, XOR the sum
    return ~sum;
}

uint16_t net_checksum_tcp_udp(uint16_t proto, 
                              uint16_t length, 
                              uint8_t src_ip[IPv4_ADDR_SIZE], 
                              uint8_t dst_ip[IPv4_ADDR_SIZE],
                              void* addr, 
                              int count) {
	char b1[64];
	char b2[64];
	format_ipv4_address__buf(b1, 64, src_ip);
	format_ipv4_address__buf(b2, 64, dst_ip);
    printf("net_checksum_tcp_udp %d %d %d %s %s\n", proto, length, count, b1, b2);
    // https://gist.github.com/fxlv/81209bbd150abfeaceb1f85ff076c9f3
    uint32_t sum = 0;
    sum += _net_checksum_add((uint8_t*)addr, count);
    sum += _net_checksum_add(src_ip, IPv4_ADDR_SIZE);
    sum += _net_checksum_add(dst_ip, IPv4_ADDR_SIZE);
    sum += proto + length;
    return _net_checksum_finish(sum);
}

uint16_t net_checksum_tcp_udp2(uint8_t src_ip[IPv4_ADDR_SIZE], 
                              uint8_t dst_ip[IPv4_ADDR_SIZE],
                              uint16_t proto,
                              uint16_t len,
                              uint16_t src_port,
                              uint16_t dst_port,
                              uint8_t* data,
                              uint16_t data_size) {
    // https://gist.github.com/fxlv/81209bbd150abfeaceb1f85ff076c9f3
	char b1[64];
	char b2[64];
	format_ipv4_address__buf(b1, 64, src_ip);
	format_ipv4_address__buf(b2, 64, dst_ip);
    printf("net_checksum_tcp_udp %s %s\n", b1, b2);
    printf("proto %d udp len %d portsrc %d portdst %d data_size %d\n", proto, len, src_port, dst_port, data_size);
    uint32_t sum = 0;
    sum += _net_checksum_add(src_ip, IPv4_ADDR_SIZE);
    sum += _net_checksum_add(dst_ip, IPv4_ADDR_SIZE);
    sum += proto;
    sum += len;
    sum += src_port;
    sum += dst_port;
    sum += len;
    sum += _net_checksum_add((uint8_t*)data, data_size);
    //sum += proto + length;
    return _net_checksum_finish(sum);
}

uint16_t net_checksum_ipv4(void* addr, int count) {
	// https://tools.ietf.org/html/rfc1071
	// https://www.saminiir.com/lets-code-tcp-ip-stack-2-ipv4-icmpv4/
    uint32_t sum = _net_checksum_add((uint8_t*)addr, count);
    return _net_checksum_finish(sum);
}
