ARCH = x86_64
SRCS = kernel.c
TARGET = kernel.elf

all: $(TARGET)

CC = ../../x86_64-toolchain/bin/x86_64-elf-gcc
LD = ../../x86_64-toolchain/bin/x86_64-elf-ld

$(TARGET): kernel.c serial/serial.c printf.c
		# https://wiki.osdev.org/Creating_a_64-bit_kernel
		$(CC) -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-stack-protector -fno-stack-check -D__$(ARCH)__ -I. -I../ -c kernel.c -o kernel.o
		$(CC) -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-stack-protector -fno-stack-check -D__$(ARCH)__ -I. -I../ -c serial/serial.c -o serial.o
		$(CC) -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-stack-protector -fno-stack-check -D__$(ARCH)__ -I. -I../ -c printf.c -o printf.o
		$(LD) -nostdlib -z max-page-size=0x1000 -Ttext=0x01000000 kernel.o serial.o printf.o -o $(TARGET)

clean:
		rm *.o $(TARGET) 2>/dev/null || true
