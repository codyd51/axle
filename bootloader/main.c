#include <uefi.h>

#include "elf.h"
#include "axle_boot_info.h"

uint64_t kernel_map_elf(const char* kernel_filename) {
	FILE* kernel_file = fopen("\\EFI\\AXLE\\KERNEL.ELF", "r");
	if (!kernel_file) {
		printf("Unable to open KERNEL.ELF!\n");
		return 0;
	}

	// Read the kernel file contents into a buffer
	fseek(kernel_file, 0, SEEK_END);
	uint64_t kernel_size = ftell(kernel_file);
	fseek(kernel_file, 0, SEEK_SET);
	uint8_t* kernel_buf = malloc(kernel_size);
	if (!kernel_buf) {
		printf("Failed to allocate memory!\n");
		return 0;
	}
	fread(kernel_buf, kernel_size, 1, kernel_file);
	fclose(kernel_file);

	// Validate the ELF 
	Elf64_Ehdr* elf = (Elf64_Ehdr*)kernel_buf;
	if (memcmp(elf->e_ident, ELFMAG, SELFMAG)) {
		printf("ELF magic wrong!\n");
		return 0;
	}
	if (elf->e_ident[EI_CLASS] != ELFCLASS64) {
		printf("Not 64 bit\n");
		return 0;
	}
	if (elf->e_ident[EI_DATA] != ELFDATA2LSB) {
		printf("Not LSB (endianness?)\n");
		return 0;
	}
	if (elf->e_type != ET_EXEC) {
		printf("Not executable\n");
		return 0;
	}
	if (elf->e_machine != EM_MACH) {
		printf("Wrong arch\n");
		return 0;
	}
	if (elf->e_phnum <= 0) {
		printf("No program headers\n");
		return 0;
	}
	printf("Valid ELF!\n");

	// Load ELF segments
	for (uint64_t i = 0; i < elf->e_phnum; i++) {
		Elf64_Phdr* phdr = (Elf64_Phdr*)(kernel_buf + elf->e_phoff + (i * elf->e_phentsize));
		printf("PH at 0x%16x\n", phdr);
		if (phdr->p_type == PT_LOAD) {
			uint64_t bss_size = phdr->p_memsz - phdr->p_filesz;
			printf("ELF segment %p %d bytes (bss %d bytes)\n", phdr->p_vaddr, phdr->p_filesz, bss_size);
			memcpy((void*)phdr->p_vaddr, kernel_buf + phdr->p_offset, phdr->p_filesz);
			memset((void*)phdr->p_vaddr + phdr->p_filesz, 0, bss_size); 
		}
	}

	uintptr_t kernel_entry_point = elf->e_entry;
	free(kernel_buf);
	return kernel_entry_point;
}

#define PAGE_SIZE 0x1000

int main(int argc, char** argv) {
	ST->ConOut->ClearScreen(ST->ConOut);
	printf("axle OS bootloader init...\n");

	// Step 1: Map the kernel ELF into memory
	uint64_t kernel_entry_point = kernel_map_elf("\\EFI\\AXLE\\KERNEL.ELF");

	// Step 2: Map the initrd into memory
	FILE* initrd_file = fopen("\\EFI\\AXLE\\INITRD.IMG", "r");
	if (!initrd_file) {
		printf("Failed to open initrd!\n");
		return 0;
	}
	fseek(initrd_file, 0, SEEK_END);
	uint64_t initrd_size = ftell(initrd_file);
	fseek(initrd_file, 0, SEEK_SET);
	uint64_t initrd_page_count = (initrd_size / PAGE_SIZE) + 1;
	printf("Initrd size 0x%p, page count %ld\n", initrd_size, initrd_page_count);
	efi_physical_address_t initrd_buf = 0;
	efi_status_t status = BS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, initrd_page_count, &initrd_buf);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate memory for initrd! %ld\n", status);
		while (1) {}
		return 0;
	}
	printf("Allocated buffer for initrd: 0x%p\n", initrd_buf);
	fread(initrd_buf, initrd_size/2, 1, initrd_file);
	printf("Mapped initrd\n");
	fclose(initrd_file);
	// PT: The mformat cmd wants to operate on floppy disks and thus is too small to hold an initrd

	// Step 2: Select a graphics mode
	efi_gop_t* gop = NULL;
	efi_guid_t gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	status = BS->LocateProtocol(&gop_guid, NULL, (void**)&gop);
	if (EFI_ERROR(status)) {
		printf("Failed to locate GOP!\n");
		return 0;
	}

	axle_boot_info_t* boot_info = malloc(sizeof(axle_boot_info_t));
	boot_info->framebuffer_base = gop->Mode->FrameBufferBase;
	boot_info->framebuffer_width = gop->Mode->Information->HorizontalResolution;
	boot_info->framebuffer_height = gop->Mode->Information->VerticalResolution;
	boot_info->framebuffer_bytes_per_pixel = 4;

	// Calling GetMemoryMap with an invalid buffer allows us to read info on 
	// how much memory we'll need to store the memory map.
	uint64_t memory_map_size = 0; 
	efi_memory_descriptor_t* memory_descriptors = NULL;
	uint64_t memory_map_key = 0;
	uint64_t memory_descriptor_size = 0;
	uint32_t memory_descriptor_version = 0;
	status = BS->GetMemoryMap(
		&memory_map_size,
		NULL,
		&memory_map_key,
		&memory_descriptor_size,
		&memory_descriptor_version
	);
	// This first call should never succeed as we're using it to determine the needed buffer space
	if (status != EFI_BUFFER_TOO_SMALL || !memory_map_size) {
		printf("Expected buffer to be too small...\n");
		return 0;
	}

	// Now we know how big the memory map needs to be.
	printf("Set memory map size to: %p\n", memory_map_size);
	printf("Status %p\n", status);
	printf("Memory descriptors: %p\n", memory_descriptors);
	printf("Memory map key: %p\n", memory_map_key);
	printf("Memory descriptor size: %p\n", memory_descriptor_size);
	printf("Memory descriptor version: %p\n", memory_descriptor_version);

	// Allocate the buffer for the memory descriptors, 
	// but this will change the memory map and may increase its size!
	// Reserve some extra space just in case
	memory_map_size += (memory_descriptor_size * 4);
	printf("Reserved extra memory map space, buffer size is now %p\n", memory_map_size);
	// https://uefi.org/sites/default/files/resources/UEFI_Spec_2_8_final.pdf
	memory_descriptors = malloc(memory_map_size);

	status = BS->GetMemoryMap(
		&memory_map_size,
		memory_descriptors,
		&memory_map_key,
		&memory_descriptor_size,
		NULL
	);
	// The buffer should have been large enough...
	if (EFI_ERROR(status)) {
		printf("Error reading memory map!\n");
		return 0;
	}

	boot_info->memory_descriptors = memory_descriptors;
	boot_info->memory_descriptor_size = memory_descriptor_size;
	boot_info->memory_map_size = memory_map_size;

	printf("Jumping to kernel entry point at %p\n", kernel_entry_point);
	if (exit_bs()) {
		printf("Failed to exit boot services!\n");
		while (1) {}
		return 0;
	}

	// This should never return...
	(*((int(* __attribute__((sysv_abi)))(axle_boot_info_t*))(kernel_entry_point)))(boot_info);

	// If we got here, the kernel returned control to the bootloader
	// This should never happen with a well-behaved kernel...
	return 0;

	//uint64_t info_size = 0;
	//efi_gop_mode_info_t* mode_info = NULL;
	//status = gop->QueryMode(gop, gop->Mode == NULL ? 0 : gop->Mode->Mode, &info_size, &mode_info);

	/*
	const char* efi_memory_types[] = {
        "EfiReservedMemoryType",
        "EfiLoaderCode",
        "EfiLoaderData",
        "EfiBootServicesCode",
        "EfiBootServicesData",
        "EfiRuntimeServicesCode",
        "EfiRuntimeServicesData",
        "EfiConventionalMemory",
        "EfiUnusableMemory",
        "EfiACPIReclaimMemory",
        "EfiACPIMemoryNVS",
        "EfiMemoryMappedIO",
        "EfiMemoryMappedIOPortSpace",
        "EfiPalCode"
    };

	printf("Address\tSize\tType\n");
	efi_memory_descriptor_t* mement;
	for (efi_memory_descriptor_t* mement = memory_map; (uint8_t*)mement < (uint8_t*)memory_map + memory_map_size; mement = NextMemoryDescriptor(mement, desc_size)) {
		printf("0x%08x - 0x%08x (0x%08x), %02x, %s\n", mement->PhysicalStart, mement->PhysicalStart + (mement->NumberOfPages * 0x1000), (mement->NumberOfPages * 0x1000), mement->Type, efi_memory_types[mement->Type]);
	}
	printf("freeing mem map...\n");
	free(memory_map);
	*/

	//FILE* kernel = fopen("\\EFI\\AXLE\\KERNEL.ELF", "r");

	/*
	char* a = malloc(1024);
	printf("got malloc memory %p\n", a);
	free(a);
	*/

	/*
	uint64_t ret = 0;
	status = BS->AllocatePool(EfiLoaderData, 2, &ret);
	printf("alloc pool %p\n", ret);
	BS->FreePool(ret);
	printf("freed pool\n");
	//fclose(kernel);
	/*
	if (!kernel) {
		printf("Unable to open Kernel.ELF!\n");
		return 0;
	}
	printf("Opened kernel at 0x%16x\n", kernel);
	fseek(kernel, 0, SEEK_END);
	uint64_t kernel_size = ftell(kernel);
	fseek(kernel, 0, SEEK_SET);
	uint8_t* kernel_buf = malloc(kernel_size);
	if (!kernel_buf) {
		printf("Failed to allocate memory!\n");
		return 0;
	}
	fread(kernel_buf, kernel_size, 1, kernel);
	fclose(kernel);

	// Valid ELF for this architecture?
	Elf64_Ehdr* elf = (Elf64_Ehdr*)kernel_buf;
	if (memcmp(elf->e_ident, ELFMAG, SELFMAG)) {
		printf("ELF magic wrong!\n");
		return 0;
	}
	if (elf->e_ident[EI_CLASS] != ELFCLASS64) {
		printf("Not 64 bit\n");
		return 0;
	}
	if (elf->e_ident[EI_DATA] != ELFDATA2LSB) {
		printf("Not LSB (endianness?)\n");
		return 0;
	}
	if (elf->e_type != ET_EXEC) {
		printf("Not executable\n");
		return 0;
	}
	if (elf->e_machine != EM_MACH) {
		printf("Wrong arch\n");
		return 0;
	}
	if (elf->e_phnum <= 0) {
		printf("No program headers\n");
		return 0;
	}
	printf("Valid ELF!\n");

	// Load ELF segments
	for (uint64_t i = 0; i < elf->e_phnum; i++) {
		Elf64_Phdr* phdr = (Elf64_Phdr*)(kernel_buf + elf->e_phoff + (i * elf->e_phentsize));
		printf("PH at 0x%16x\n", phdr);
		if (phdr->p_type == PT_LOAD) {
			uint64_t bss_size = phdr->p_memsz - phdr->p_filesz;
			printf("ELF segment %p %d bytes (bss %d bytes)\n", phdr->p_vaddr, phdr->p_filesz, bss_size);
			memcpy((void*)phdr->p_vaddr, kernel_buf + phdr->p_offset, phdr->p_filesz);
			//memset((void*)phdr->p_vaddr + phdr->p_filesz, 0, bss_size); 
		}
		else {
			native_mode = gop->Mode->Mode;
			max_mode = gop->Mode->MaxMode;

			printf("Native mode: %ld, Max Mode: %ld\n", native_mode, max_mode);

			for (int i = native_mode; i < max_mode; i++) {
				gop->QueryMode(gop, i, &info_size, &mode_info);
				printf("Mode %ld: %ldx%ld, %ld bpp\n", i, mode_info->HorizontalResolution, mode_info->VerticalResolution, mode_info->PixelFormat);
			}

			int i = 29;
			printf("Trying mode %ld...\n", i);
			gop->SetMode(gop, i);
			printf("Mode set to %ld!\n", i);
			sleep(1);
			/*
			for (int i = native_mode; i < max_mode; i++) {
				printf("Trying mode %ld...\n", i);
				gop->SetMode(gop, i);
				printf("Mode set to %ld!\n", i);
				sleep(1);
			}
			*/
		/*
	}
	uintptr_t kernel_entry_point = elf->e_entry;
	free(kernel_buf);

		//gop->SetMode(gop, 1);
	/*
	uint8_t* framebuf = gop->Mode->FrameBufferBase;
	uint32_t horizontal_resolution = mode_info->HorizontalResolution;
	//DEBUG("test debug\n");
	//ASSERT("test assert\n");

	if (exit_bs()) {
		printf("Failed to exit boot services!\n");
		while (1) {}
		return 0;
	}

	for (uint32_t x = 0; x < 200; x++) {
		for (uint32_t y = 0; y < 100; y++) {
			uint64_t idx = (y * horizontal_resolution * 4) + x;
			framebuf[idx + 0] = 0xff;
			framebuf[idx + 1] = 0x33;
			framebuf[idx + 2] = 0xff;
			framebuf[idx + 3] = 0x77;
		}
	}
	*/

	//int kernel_entry_point = 0;
	//printf("Jumping to kernel entry point at %p\n", kernel_entry_point);
	/*
	int ret = (*((int(* __attribute__((sysv_abi)))(void))(kernel_entry_point)))();
	printf("Kernel returned %d\n", ret);
	*/

	return 0;
}
