#ifndef PS2_CONTROLLER_H
#define PS2_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#define PS2_RECV_PORT 0x60
#define PS2_SEND_PORT 0x64
#define PS2_CMD_PORT 0x64
#define PS2_DATA 0x60

#define PS2_CMD_ENABLE_DEVICE_1 0xAE
#define PS2_CMD_DISABLE_DEVICE_1 0xAD
#define PS2_CMD_ENABLE_DEVICE_2 0xA8
#define PS2_CMD_DISABLE_DEVICE_2 0xA7
#define PS2_CMD_TEST_CONTROLLER 0xAA
#define PS2_CMD_RESP_TEST_CONTROLLER_SUCCESS 0x55
#define PS2_CMD_RESP_TEST_CONTROLLER_FAILURE 0xFC
#define PS2_CMD_TEST_DEVICE_1 0xAB
#define PS2_CMD_TEST_DEVICE_2 0xA9
#define PS2_CMD_RESP_TEST_DEVICE_SUCCESS 0x00
#define PS2_CMD_RESET_DEVICE 0xFF
#define PS2_CMD_WRITE_NEXT_BYTE_TO_SECOND_INPUT_PORT 0xD4

#define PS2_CMD_READ_CONTROLLER_CONFIGURATION_BYTE 0x20
#define PS2_CMD_SET_CONTROLLER_CONFIGURATION_BYTE 0x60

// Device commands
#define PS2_DEV_RESET 0xFF
#define PS2_DEV_IDENTIFY 0xF2
#define PS2_DEV_ENABLE_SCAN 0xF4
#define PS2_DEV_DISABLE_SCAN 0xF5

// Device responses
#define PS2_DEV_RESET 0xFF
#define PS2_DEV_ACK 0xFA
#define PS2_DEV_RESET_ACK 0xAA

// Configuration byte
#define PS2_CFG_DEVICE_1_INTERRUPTS (1 << 0)
#define PS2_CFG_DEVICE_2_INTERRUPTS (1 << 1)
#define PS2_CFG_SYSTEM_FLAG (1 << 2)
#define PS2_CFG_DEVICE_1_ENABLED (1 << 4)
#define PS2_CFG_DEVICE_2_ENABLED (1 << 5)
#define PS2_CFG_DEVICE_1_PORT_TRANSLATION (1 << 6)
#define PS2_CFG_MUST_BE_ZERO (1 << 7)

typedef enum ps2_device_type {
    PS2_MOUSE = 0x00,
    PS2_MOUSE_SCROLL_WHEEL = 0x03,
    PS2_MOUSE_FIVE_BUTTONS = 0x04,
    PS2_KEYBOARD,
    PS2_KEYBOARD_TRANSLATED,
    PS2_DEVICE_UNKNOWN
} ps2_device_type_t;

void ps2_controller_init(void);
void ps2_enable_keyboard(void);
void ps2_enable_mouse(void);

bool ps2_expect_ack(void);
void ps2_write_device(uint32_t device, uint8_t b);
uint8_t ps2_read(uint8_t port);

#endif