#ifndef NET_MESSAGES_H
#define NET_MESSAGES_H

#include <kernel/amc.h>

#define NET_SERVICE_NAME "com.axle.net"

// Sent from the NIC to the net stack
#define NET_RX_ETHERNET_FRAME (1 << 0)

typedef struct net_message {
    amc_msg_header_t header;
    uint8_t event;
    uint32_t len;
    uint8_t data[AMC_MESSAGE_PAYLOAD_SIZE- sizeof(uint8_t) - sizeof(uint32_t) - sizeof(amc_msg_header_t)];
} net_message_t;
ASSERT_AMC_MSG_BODY_SIZE(amc_msg_body_charlist_t);

#endif