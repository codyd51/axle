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
    buf[1] = (uint8_t)((ip >> 1) & 0xff);
    buf[2] = (uint8_t)((ip >> 2) & 0xff);
    buf[3] = (uint8_t)((ip >> 3) & 0xff);
    format_ipv4_address__buf(out, out_size, buf);
}

void hexdump(const void* addr, const int len) {
    const char* desc = "";
    int i;
    unsigned char buff[17];
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

        if ((i % 16) == 0) {
            // Don't print ASCII buffer for the "zeroth" line.

            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.

            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.

    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    printf ("  %s\n", buff);
}