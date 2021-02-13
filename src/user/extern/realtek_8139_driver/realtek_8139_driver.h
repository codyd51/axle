#ifndef RTL_8139_DRIVER_H
#define RTL_8139_DRIVER_H

// 4 bytes, R/W. First 4 bytes of the MAC
#define RTL_REG_ID_0 0x00
// 2 bytes, R/W. Last 2 bytes of the MAC
#define RTL_REG_ID_4 0x04

// 4 bytes, R/W. Physical start address of transmit descriptor 0
#define RTL_REG_TX_0_PHYS_START     0x20
#define RTL_REG_TX_1_PHYS_START     0x24
#define RTL_REG_TX_2_PHYS_START     0x28
#define RTL_REG_TX_3_PHYS_START     0x2c

// 4 bytes, R/W. Physical start address of the receive buffer
#define RTL_REG_RX_BUFFER_PHYS_START    0x30

// 1 byte, R/W. Send commands to the device
#define RTL_REG_COMMAND_REGISTER    0x37

// 2 bytes, R/W. Current address of packet read in receive ring buffer
#define RTL_REG_RX_CURRENT_ADDR_PACKET_READ		0x38
// 2 bytes, R. Current size stored in receive ring buffer
#define RTL_REG_RX_CURRENT_BUFFER_ADDRESS		0x3a

// 2 bytes. Controls the event types that will raise an interrupt
#define RTL_REG_INTERRUPT_MASK		0x3c
// 2 bytes, R/W. Interrupt statu 
#define RTL_REG_INTERRUPT_STATUS	0x3e

// 4 bytes. R/W. Receive configuration
#define RTL_REG_RX_CONFIG   0x44

// 1 bit. Indicates successful completion of packet reception
#define RTL_ISR_FLAG_ROK	(1 << 0)
// 1 bit. Indicates if the received packet has a CRC error or frame alignment error
#define RTL_ISR_FLAG_RER	(1 << 1)
// 1 bit. Indicates that a packet transmission completed successfully
#define RTL_ISR_FLAG_TOK	(1 << 2)
// 1 bit. Indicates that a packet transmission was aborted
#define RTL_ISR_FLAG_TER	(1 << 3)
// 1 bit. Set when receive ring buffer is full
#define RTL_ISR_FLAG_RX_BUFFER_OVERFLOW     (1 << 4)
#define RTL_ISR_FLAG_LINK_CHANGE    (1 << 5)
#define RTL_ISR_FLAG_RX_FIFO_OVERFLOW     (1 << 6)

#define RTL_RX_PACKET_STATUS_ROK					(1 << 0)
#define RTL_RX_PACKET_STATUS_FRAME_ALIGNMENT_ERROR	(1 << 1)
#define RTL_RX_PACKET_STATUS_CRC_ERROR				(1 << 2)
#define RTL_RX_PACKET_STATUS_LONG					(1 << 3)
#define RTL_RX_PACKET_STATUS_RUNT					(1 << 4)
#define RTL_RX_PACKET_STATUS_INVALID_SYMBOL_ERROR	(1 << 5)
#define RTL_RX_PACKET_STATUS_IS_BROADCAST			(1 << 13)
#define RTL_RX_PACKET_STATUS_MAC_ADDRESS_MATCHES	(1 << 14)
#define RTL_RX_PACKET_STATUS_IS_MULTICAST			(1 << 15)

#define RTL_RX_CONFIG_FLAG_ACCEPT_ALL_PACKETS (1 << 0)
#define RTL_RX_CONFIG_FLAG_ACCEPT_MAC_MATCH_PACKETS (1 << 1)
#define RTL_RX_CONFIG_FLAG_ACCEPT_MULTICAST_PACKETS (1 << 2)
#define RTL_RX_CONFIG_FLAG_ACCEPT_BROADCAST_PACKETS (1 << 3)
#define RTL_RX_CONFIG_FLAG_ACCEPT_RUNT_PACKETS (1 << 4)
#define RTL_RX_CONFIG_FLAG_ACCEPT_ERROR_PACKETS (1 << 5)
#define RTL_RX_CONFIG_FLAG_DO_NOT_WRAP (1 << 7)

// Command bits that can be sent to the command register
#define RTL_CMD_REG_RESET (1 << 4)
#define RTL_CMD_REG_ENABLE_RECEIVE (1 << 3)
#define RTL_CMD_REG_ENABLE_TRANSMIT (1 << 2)
#define RTL_CMD_REG_IS_RX_BUFFER_EMPTY (1 << 0)

#endif