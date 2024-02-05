#include "serial.h"
#include "kernel/drivers/pit/pit.h"
#include "kernel/util/spinlock/spinlock.h"
#include <kernel/smp.h>

// Ref: https://www.lookrs232.com/rs232/fcr.htm

//COM 1
#define PORT 0x3F8

// Only valid when DLAB is unset
#define DATA_REGISTER (PORT + 0)
#define INTERRUPT_ENABLE_REGISTER (PORT + 1)

// Only valid when DLAB is set
#define DLAB_DATA_BYTE_LOW_REGISTER (PORT + 0)
#define DLAB_DATA_BYTE_HIGH_REGISTER (PORT + 1)

#define INTERRUPT_AND_FIFO_CONTROL_REGISTER (PORT + 2)
#define LINE_CONTROL_REGISTER (PORT + 3)
#define MODEM_CONTROL_REGISTER (PORT + 4)

#define BUF_SIZE (1028*8)
static char buffer[BUF_SIZE] = {0};
static int idx = 0;

int serial_waiting() {
	return inb(PORT + 5) & 1;
}

char serial_get() {
	while (serial_waiting() == 0);
	return inb(PORT);
}

bool is_transmitting() {
	return inb(PORT + 5) & 0x20;
}

void __serial_putchar(char c) {
	while (is_transmitting() == 0);
	if (c == '\n') {
        // Add an extra carriage return
        outb(PORT, '\r');
    }
	outb(PORT, c);
}

void __serial_writestring(char* str) {
	char* ptr = str;
	while (*ptr) {
		__serial_putchar(*(ptr++));
	}
}

static void serial_flush() {
	__serial_writestring(buffer);
    // It's only necessary to memset up to where we last wrote
	memset(buffer, 0, idx);
	idx = 0;
}

void serial_putchar(char c) {
	if (idx + 1 >= BUF_SIZE) {
		//buffer full, flush to real serial
		serial_flush();
	}
	//append c to buffer
	buffer[idx+0] = c;
	//buffer[idx+1] = '\0';
	idx++;
	//also flush on newline
	if (c == '\n') {
		serial_flush();
	}
}

void serial_puts_int(char* str, bool print_prefix) {
    if (print_prefix) {
        char prefix[64] = {0};
        snprintf(prefix, sizeof(prefix), "Cpu[%d],Pid[%d],Clk[%d]: ", cpu_id(), getpid(), tick_count());

        // Hold a lock, so we don't get output intermixed from other cores
        // Note that the lock is held before our two 'inner' calls, rather than within the inner calls
        // Otherwise, there is a race between the two calls
        static spinlock_t serial_output_lock = {.name = "Serial output"};
        spinlock_acquire(&serial_output_lock);

        serial_puts_int(prefix, false);
        serial_puts_int(str, false);

        spinlock_release(&serial_output_lock);

        return;
    }

    char* ptr = str;
    while (*ptr) {
        serial_putchar(*(ptr++));
    }
}


void serial_puts(char* str) {
    static bool previous_output_contained_newline = false;
    serial_puts_int(str, previous_output_contained_newline);
    // If this output ended on a newline, the next line should come with the info prefix
    previous_output_contained_newline = str[strlen(str) - 1] == '\n';
}

void serial_init() {
	printf_info("Initializing serial driver...");

	memset(buffer, 0, BUF_SIZE);

    // Disable interrupts
	outb(INTERRUPT_ENABLE_REGISTER, 0x00);

    // Enable DLAB bit, as we'll set the frequency divisor next
	outb(LINE_CONTROL_REGISTER, 0x80);
    // Set divisor to 1, so we transmit at the serial clock speed
    // Low divisor byte
	outb(DLAB_DATA_BYTE_LOW_REGISTER, 0x01);
    // High divisor byte
	outb(DLAB_DATA_BYTE_HIGH_REGISTER, 0x00);

    // Bit pattern:
    // [0, 1]: Set 7 data bits (we're just sending ASCII so this should work out cleanly)
    // 2: Set 1 stop bit
    // 3: No parity bit
    outb(LINE_CONTROL_REGISTER, 0b0010);

    // Bit pattern:
    // 0: Enable FIFO's
    // 1: Clear receive FIFO
    // 2: Clear transmit FIFO
    // 3: DMA mode-select
    // 4: Reserved
    // 5: Disable model-specific 64-byte FIFO
    // [6-7]: Set interrupt trigger level to 14 bytes
	outb(INTERRUPT_AND_FIFO_CONTROL_REGISTER, 0b11000111);

    // Bit pattern:
    // 0: Data Terminal Ready
    // 1: Request to Send
    // 2: OUT1, unused in PC implementations
    // 3: Set the OUT2 hardware pin which is used to enable the IRQ in PC implementations
    // 4: Disable loopback
    // [5-7]: Unused
	outb(MODEM_CONTROL_REGISTER, 0b00001011);

    // TODO(PT): Switch to an interrupt-based model for this controller, instead of polling
    /*
    // Enable interrupts.
    // Bit pattern:
    // : Interrupt when the transmitter is empty (so we know when we can send more)
    interrupt_setup_callback(INT_VECTOR_APIC_4, (int_callback_t)_handle_irq);
    outb(INTERRUPT_ENABLE_REGISTER, 0b10);
    */
}
