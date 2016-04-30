AS=i686-elf-as
CC=i686-elf-gcc



axle: boot.s linker.ld std.c kb.c kernel.c shell.c clock.c interrupt.c ide.c enableA20.s
	$(AS) boot.s -o boot.o
	nasm -f elf -o enableA20.o enableA20.s
	$(CC) -c std.c -o std.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c kb.c -o kb.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c shell.c -o shell.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c clock.c -o clock.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c interrupt.c -o interrupt.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -c ide.c -o ide.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -T linker.ld -o axle.bin -ffreestanding -O2 -nostdlib boot.o enableA20.o kernel.o shell.o kb.o std.o clock.o interrupt.o -lgcc
