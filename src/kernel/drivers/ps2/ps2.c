#include "ps2.h"
#include <stdint.h>
#include <stdbool.h>
#include <kernel/assert.h>

uint8_t ps2_read(uint8_t port);

/* Returns true if a device replied with `PS2_DEV_ACK`.
 * This is usually in reply to a command sent to that device.
 */
bool ps2_expect_ack() {
    uint8_t ret = ps2_read(PS2_DATA);

    if (ret != PS2_DEV_ACK) {
        printf("[PS2] Device failed to acknowledge command\n");
        return false;
    }

    return true;
}

static void ps2_write(uint8_t port, uint8_t byte) {
    int timer = 500;
    while ((inb(PS2_CMD_PORT) & 2) && timer-- > 0) {
        asm ("pause");
    }
    outb(port, byte);
}

uint8_t ps2_read(uint8_t port) {
    int timer = 500;
    while (!(inb(PS2_CMD_PORT) & 1) && timer-- >= 0) {
        asm ("pause");
    }
    return inb(port);
}

static uint8_t _ps2_read_without_checking_for_available_data(void) {
    return inb(PS2_RECV_PORT);
}

void ps2_device1_send(uint8_t byte) {
    outb(PS2_DATA, byte);
}

void ps2_device2_send(uint8_t byte) {
    ps2_write(PS2_CMD_PORT, PS2_CMD_WRITE_NEXT_BYTE_TO_SECOND_INPUT_PORT);
    outb(PS2_DATA, byte);
}

/* Write a byte to the specified `device` input buffer.
 * This function is used to send command to devices.
 */
void ps2_write_device(uint32_t device, uint8_t b) {
    if (device != 0) {
        ps2_write(PS2_CMD_PORT, PS2_CMD_WRITE_NEXT_BYTE_TO_SECOND_INPUT_PORT);
    }
    return ps2_write(PS2_DATA, b);
}

void ps2_controller_init(void) {
    // Reference for PS2 controller: https://wiki.osdev.org/%228042%22_PS/2_Controller
    asm("cli");

    // Disable PS2 devices during controller initialization 
    ps2_write(PS2_CMD_PORT, PS2_CMD_DISABLE_DEVICE_1);
    ps2_write(PS2_CMD_PORT, PS2_CMD_DISABLE_DEVICE_2);
    
    // If we missed an IRQ before PS2 initialization ran, 
    // handle it now by draining the PS2 output buffer
    inb(PS2_DATA);

    // Set the controller configuration byte
    ps2_write(PS2_CMD_PORT, PS2_CMD_READ_CONTROLLER_CONFIGURATION_BYTE);
    printf("Reading config byte\n");
    uint8_t controller_config = ps2_read(PS2_DATA);
    printf("PS/2 Controller config: 0x%08x\n", controller_config);
    // Check that 2 PS2 ports are supported
    bool two_ports_supported = (controller_config & (1 << 5)) != 0;
    assert(two_ports_supported, "PS2 controller only supports a single device");
    assert((controller_config & PS2_CFG_MUST_BE_ZERO) == 0, "Invalid bit set in configuration byte");

    controller_config |= PS2_CFG_SYSTEM_FLAG;
    // Disable IRQs for device 1
    controller_config &= ~PS2_CFG_FIRST_PORT;
    // Disable IRQs for device 2
    controller_config &= ~PS2_CFG_SECOND_PORT;
    // Disable port translation
    controller_config &= ~PS2_CFG_TRANSLATION;
    // Set the new configuration
    printf("PS/2 Controller new config: 0x%08x\n", controller_config);
    ps2_write(PS2_CMD_PORT, PS2_CMD_SET_CONTROLLER_CONFIGURATION_BYTE);
    ps2_write(PS2_DATA, controller_config);

    // Perform controller self-test
    printf("PS/2 Self-Test\n");
    ps2_write(PS2_CMD_PORT, PS2_CMD_TEST_CONTROLLER);
    uint8_t controller_status = ps2_read(PS2_DATA);
    assert(controller_status == PS2_CMD_RESP_TEST_CONTROLLER_SUCCESS, "PS2 controller self-test failed");

    // Restore the configuration in case running the self-test reset the controller
    printf("PS/2 Re-set new config\n");
    ps2_write(PS2_CMD_PORT, PS2_CMD_SET_CONTROLLER_CONFIGURATION_BYTE);
    ps2_write(PS2_DATA, controller_config);

    // Test device 1
    ps2_write(PS2_CMD_PORT, PS2_CMD_TEST_DEVICE_1);
    uint8_t device_status = ps2_read(PS2_DATA);
    printf("PS2 Device 1 status: 0x%08x\n", device_status);
    assert(device_status == PS2_CMD_RESP_TEST_DEVICE_SUCCESS, "PS2 Device 1 self-test failed");

    // Test device 2
    ps2_write(PS2_CMD_PORT, PS2_CMD_TEST_DEVICE_2);
    device_status = ps2_read(PS2_DATA);
    printf("PS2 Device 2 status: 0x%08x\n", device_status);
    assert(device_status == PS2_CMD_RESP_TEST_DEVICE_SUCCESS, "PS2 Device 2 self-test failed");

    ps2_write(PS2_CMD_PORT, PS2_CMD_ENABLE_DEVICE_1);
    controller_config |= PS2_CFG_FIRST_PORT;
    controller_config &= ~PS2_CFG_FIRST_CLOCK;

    ps2_write(PS2_CMD_PORT, PS2_CMD_ENABLE_DEVICE_2);
    controller_config |= PS2_CFG_SECOND_PORT;
    controller_config &= ~PS2_CFG_SECOND_CLOCK;

    ps2_write(PS2_CMD_PORT, PS2_CMD_SET_CONTROLLER_CONFIGURATION_BYTE);
    ps2_write(PS2_DATA, controller_config);

    // Reset devices
    for (uint32_t i = 0; i < 2; i++) {
        ps2_write_device(i, PS2_DEV_RESET);
        uint8_t ret = ps2_read(PS2_DATA);

        // If it fails, disable the device's port
        assert(ret == PS2_DEV_ACK && ps2_read(PS2_DATA) == PS2_DEV_RESET_ACK, "Failed to reset device");

        // For some reason, mice send an additional 0x00 byte
        /*
        if (ps2_can_read()) {
            ps2_read(0x60);
        }
        */
    }
    ps2_keyboard_enable();
    ps2_mouse_enable();

    asm("sti");
}
