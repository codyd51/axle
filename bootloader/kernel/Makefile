ARCH = x86_64
SRCS = kernel.c
TARGET = kernel.elf

all: $(TARGET)

CC = ../../x86_64-toolchain/bin/x86_64-elf-gcc
LD = ../../x86_64-toolchain/bin/x86_64-elf-ld

$(TARGET): kernel.c 
		# https://wiki.osdev.org/Creating_a_64-bit_kernel
		$(CC) -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-stack-protector -fno-stack-check -D__$(ARCH)__ -I. -c $< -o kernel.o
		$(LD) -nostdlib -z max-page-size=0x1000 -Ttext=0x01000000 kernel.o -o $(TARGET)

clean:
		rm *.o $(TARGET) 2>/dev/null || true
