#include "ps2.h"
#include <stdint.h>
#include <stdbool.h>
#include <kernel/assert.h>

#define PS2_MOUSE_CMD_SET_DEFAULT_SETTINGS 0xF7
#define PS2_MOUSE_CMD_ENABLE_DATA_REPORTING 0xF4
#define PS2_MOUSE_RESP_ACKNOWLEDGE 0xFA

static uint8_t ps2_read_status_register(void) {
    return inb(PS2_SEND_PORT);
}

static void ps2_poll_until_ready_for_write(void) {
    // Poll until "input buffer full" flag is unset
    while ((ps2_read_status_register() & (1 << 1)) != 0) {
        printf("ps/2 polling for d1\n");
        ;
    }
}

static void ps2_poll_until_ready_for_read(void) {
    // Poll until "output buffer full" flag is unset
    int i = 0;
    while ((ps2_read_status_register() & (1 << 0)) == 0) {
        //printf("ps/2 polling for d0\n");
        if (i++ % 10000 == 0) {
            printf("PS/2 polling until ready to read (i = %d)\n", i);
        }
        ;
    }
}

static void ps2_send(uint8_t byte) {
    int timer = 500;
    while ((inb(PS2_CMD_PORT) & 2) && timer-- > 0) {
        asm ("pause");
    }
    //ps2_poll_until_ready_for_write();
    outb(PS2_CMD_PORT, byte);
}

static void ps2_write(uint8_t port, uint8_t byte) {
    int timer = 500;
    while ((inb(PS2_CMD_PORT) & 2) && timer-- > 0) {
        asm ("pause");
    }
    outb(port, byte);
}

uint8_t ps2_readp(uint8_t port) {
    int timer = 500;
    while (!(inb(PS2_CMD_PORT) & 1) && timer-- >= 0) {
        asm ("pause");
    }
    return inb(port);
}

uint8_t ps2_read(void) {
    ps2_poll_until_ready_for_read();
    return inb(PS2_RECV_PORT);
}

static uint8_t _ps2_read_without_checking_for_available_data(void) {
    return inb(PS2_RECV_PORT);
}

void ps2_device1_send(uint8_t byte) {
    //ps2_poll_until_ready_for_write();
    // Not a typo; to write to devices, write to 0x60 (named recv port)
    // Should be renamed to Data port
    // 0x64 should be renamed to IO port
    outb(PS2_DATA, byte);
}

void ps2_device2_send(uint8_t byte) {
    //ps2_poll_until_ready_for_write();
    ps2_write(PS2_CMD_PORT, PS2_CMD_WRITE_NEXT_BYTE_TO_SECOND_INPUT_PORT);
    //ps2_poll_until_ready_for_write();
    outb(PS2_DATA, byte);
}

void ps2_enable_keyboard(void) {
    asm("cli");
    // Enable PS2 device 1 (keyboard)
    ps2_write(PS2_CMD_PORT, PS2_CMD_ENABLE_DEVICE_1);

    // Enable IRQs for device 1
    ps2_write(PS2_CMD_PORT, PS2_CMD_READ_CONTROLLER_CONFIGURATION_BYTE);
    uint8_t controller_config = ps2_readp(PS2_DATA);
    printf("Got controller config 0x%08x\n", controller_config);
    controller_config |= (1 << 0);

    ps2_write(PS2_CMD_PORT, PS2_CMD_SET_CONTROLLER_CONFIGURATION_BYTE);
    ps2_write(PS2_DATA, controller_config);

    // Reset the device
    ps2_device1_send(PS2_CMD_RESET_DEVICE);
    uint8_t resp = ps2_readp(PS2_DATA);
    printf("Response after reset: %d\n", resp);
    assert(resp == PS2_DEV_ACK, "PS2 keyboard didn't ack after reset");

    asm("sti");
}

void ps2_enable_mouse(void) {
    asm("cli");

    // Enable PS2 device 2 (mouse)
    ps2_write(PS2_CMD_PORT, PS2_CMD_ENABLE_DEVICE_2);

    // Enable IRQs for device 2
    ps2_write(PS2_CMD_PORT, PS2_CMD_READ_CONTROLLER_CONFIGURATION_BYTE);
    uint8_t controller_config = ps2_readp(PS2_DATA);
    controller_config |= (1 << 1);
    ps2_write(PS2_CMD_PORT, PS2_CMD_SET_CONTROLLER_CONFIGURATION_BYTE);
    ps2_write(PS2_DATA, controller_config);

    // Reset the device
    ps2_device2_send(PS2_CMD_RESET_DEVICE);

	// Mouse should use default settings
	ps2_device2_send(PS2_MOUSE_CMD_SET_DEFAULT_SETTINGS);
	// Acknowledge
	uint8_t resp = ps2_readp(PS2_DATA);
    printf("PS2 mouse set-default-settings response: 0x%02x\n", resp);
	assert(resp == PS2_MOUSE_RESP_ACKNOWLEDGE, "PS2 Mouse received unexpected response");

	// Enable data reporting
	ps2_device2_send(PS2_MOUSE_CMD_ENABLE_DATA_REPORTING);
    asm("sti");
}

void ps2_init(void) {
    // Reference for PS2 controller: https://wiki.osdev.org/%228042%22_PS/2_Controller
    asm("cli");

    // Disable PS2 devices during controller initialization 
    ps2_write(PS2_CMD_PORT, PS2_CMD_DISABLE_DEVICE_1);
    ps2_write(PS2_CMD_PORT, PS2_CMD_DISABLE_DEVICE_2);
    
    // If we missed an IRQ before PS2 initialization ran, 
    // handle it now by draining the PS2 output buffer
    inb(PS2_DATA);
    /*
    for (int i = 0; i < 1; i++) {
        printf("reading from ps/2 port %d\n", i);
        _ps2_read_without_checking_for_available_data();
    }
    */

    // Set the controller configuration byte
    ps2_write(PS2_CMD_PORT, PS2_CMD_READ_CONTROLLER_CONFIGURATION_BYTE);
    printf("Reading config byte\n");
    uint8_t controller_config = ps2_readp(PS2_DATA);
    printf("PS/2 Controller config: 0x%08x\n", controller_config);
    // Check that 2 PS2 ports are supported
    bool two_ports_supported = (controller_config & (1 << 5)) != 0;
    assert(two_ports_supported, "PS2 controller only supports a single device");

    // Disable IRQs for device 1
    controller_config &= ~(1 << 0);
    // Disable IRQs for device 2
    controller_config &= ~(1 << 1);
    // Disable port translation
    controller_config &= ~(1 << 6);
    // Set the new configuration
    printf("PS/2 Controller new config: 0x%08x\n", controller_config);
    ps2_write(PS2_CMD_PORT, PS2_CMD_SET_CONTROLLER_CONFIGURATION_BYTE);
    ps2_write(PS2_DATA, controller_config);

    // Perform controller self-test
    printf("PS/2 Self-Test\n");
    ps2_write(PS2_CMD_PORT, PS2_CMD_TEST_CONTROLLER);
    uint8_t controller_status = ps2_readp(PS2_DATA);
    assert(controller_status == PS2_CMD_RESP_TEST_CONTROLLER_SUCCESS, "PS2 controller self-test failed");

    // Restore the configuration in case running the self-test reset the controller
    printf("PS/2 Re-set new config\n");
    ps2_write(PS2_CMD_PORT, PS2_CMD_SET_CONTROLLER_CONFIGURATION_BYTE);
    ps2_write(PS2_DATA, controller_config);

    // Test device 1
    ps2_write(PS2_CMD_PORT, PS2_CMD_TEST_DEVICE_1);
    uint8_t device_status = ps2_readp(PS2_DATA);
    printf("PS2 Device 1 status: 0x%08x\n", device_status);
    assert(device_status == PS2_CMD_RESP_TEST_DEVICE_SUCCESS, "PS2 Device 1 self-test failed");

    // Test device 2
    ps2_write(PS2_CMD_PORT, PS2_CMD_TEST_DEVICE_2);
    device_status = ps2_readp(PS2_DATA);
    printf("PS2 Device 2 status: 0x%08x\n", device_status);
    assert(device_status == PS2_CMD_RESP_TEST_DEVICE_SUCCESS, "PS2 Device 2 self-test failed");

    asm("sti");
}
