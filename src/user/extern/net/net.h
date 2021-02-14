#ifndef NET_H
#define NET_H

#define MAC_ADDR_SIZE   6
#define IPv4_ADDR_SIZE  4

typedef struct nic_config {
    uint8_t nic_mac[MAC_ADDR_SIZE];
} nic_config_t;

void copy_nic_mac(uint8_t dest[MAC_ADDR_SIZE]);

#endif