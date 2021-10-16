#include <stdint.h>
#include "axle_boot_info.h"
#include "serial/serial.h"

int _start(axle_boot_info_t* boot_info) {
    serial_init();
    uint8_t r = 120;
    uint8_t g = 60;
    uint8_t b = 0;
    uint8_t* framebuf = boot_info->framebuffer_base;
    printf("printf test!\n");
    printf("framebuf 0x%x\n", framebuf);
    printf("desc size 0x%x, map size 0x%x\n", boot_info->memory_descriptor_size, boot_info->memory_map_size);
    while (1) {
        for (uint32_t x = 0; x < 200; x++) {
            for (uint32_t y = 0; y < 100; y++) {
                uint64_t idx = (y * boot_info->framebuffer_width * boot_info->framebuffer_bytes_per_pixel) + x;
                uint8_t x = 0xff;
                framebuf[idx + 0] = x;
                framebuf[idx + 1] = x;
                framebuf[idx + 2] = x;
                framebuf[idx + 3] = 0x00;
            }
        }
    }
    while (1) {}
    return 0;
}
