#include <uefi.h>

/*** ELF64 defines and structs ***/
#define ELFMAG      "\177ELF"
#define SELFMAG     4
#define EI_CLASS    4       /* File class byte index */
#define ELFCLASS64  2       /* 64-bit objects */
#define EI_DATA     5       /* Data encoding byte index */
#define ELFDATA2LSB 1       /* 2's complement, little endian */
#define ET_EXEC     2       /* Executable file */
#define PT_LOAD     1       /* Loadable program segment */
#ifdef __x86_64__
#define EM_MACH     62      /* AMD x86-64 architecture */
#endif
#ifdef __aarch64__
#define EM_MACH     183     /* ARM aarch64 architecture */
#endif

typedef struct
{
    uint8_t  e_ident[16];   /* Magic number and other info */
    uint16_t e_type;        /* Object file type */
    uint16_t e_machine;     /* Architecture */
    uint32_t e_version;     /* Object file version */
    uint64_t e_entry;       /* Entry point virtual address */
    uint64_t e_phoff;       /* Program header table file offset */
    uint64_t e_shoff;       /* Section header table file offset */
    uint32_t e_flags;       /* Processor-specific flags */
    uint16_t e_ehsize;      /* ELF header size in bytes */
    uint16_t e_phentsize;   /* Program header table entry size */
    uint16_t e_phnum;       /* Program header table entry count */
    uint16_t e_shentsize;   /* Section header table entry size */
    uint16_t e_shnum;       /* Section header table entry count */
    uint16_t e_shstrndx;    /* Section header string table index */
} Elf64_Ehdr;

typedef struct
{
    uint32_t p_type;        /* Segment type */
    uint32_t p_flags;       /* Segment flags */
    uint64_t p_offset;      /* Segment file offset */
    uint64_t p_vaddr;       /* Segment virtual address */
    uint64_t p_paddr;       /* Segment physical address */
    uint64_t p_filesz;      /* Segment size in file */
    uint64_t p_memsz;       /* Segment size in memory */
    uint64_t p_align;       /* Segment alignment */
} Elf64_Phdr;

int main(int argc, char** argv) {
	/*
	ST->ConOut->ClearScreen(ST->ConOut);
	printf("axle OS bootloader init...\n");
	*/

	// Call with 0 to find the right memory_map_size
	uint64_t memory_map_size = 0; 
	efi_memory_descriptor_t* memory_descriptors = NULL;
	uint64_t memory_map_key = 0;
	uint64_t memory_descriptor_size = 0;
	uint32_t memory_descriptor_version = 0;
	efi_status_t status = BS->GetMemoryMap(
		&memory_map_size,
		NULL,
		&memory_map_key,
		&memory_descriptor_size,
		NULL
	);
	// This first call should never succeed as we're using it to determine the needed buffer space
	if (status != EFI_BUFFER_TOO_SMALL || !memory_map_size) {
		printf("Expected buffer to be too small...\n");
		return 0;
	}

	/*
	// Now we know how big the memory map needs to be.
	printf("Set memory map size to: %p\n", memory_map_size);
	printf("Status %p\n", status);
	printf("Memory descriptors: %p\n", memory_descriptors);
	printf("Memory map key: %p\n", memory_map_key);
	printf("Memory descriptor size: %p\n", memory_descriptor_size);
	printf("Memory descriptor version: %p\n", memory_descriptor_version);
	*/

	// Allocate the buffer for the memory descriptors, 
	// but this will change the memory map and may increase its size!
	// Reserve some extra space just in case
	memory_map_size += (memory_descriptor_size * 4);
	//printf("Reserved extra memory map space, buffer size is now %p\n", memory_map_size);
	printf("Reserved extra memory map space\n");
	// https://uefi.org/sites/default/files/resources/UEFI_Spec_2_8_final.pdf
	// The spec states that OS loaders should use the EfiLoaderData memory type
	/*
	status = BS->AllocatePool(
		EfiLoaderData,
		memory_map_size,
		(void**)memory_descriptors
	);
	printf("status 0x%p\n", status);
	if (EFI_ERROR(status)) {
		printf("Error allocating memory map buffer\n");
		while (1) {}
		return 0;
	}
	*/
	memory_descriptors = malloc(memory_map_size);
	printf("malloc done 0x%p\n", memory_descriptors);

	status = BS->GetMemoryMap(
		&memory_map_size,
		memory_descriptors,
		&memory_map_key,
		&memory_descriptor_size,
		NULL
	);
	printf("status the second %ld %p %p %p %p %p\n", status, memory_map_size, memory_descriptors, memory_map_key, memory_descriptor_size, memory_descriptor_version);
	printf("status the second\n");
	// The buffer should have been large enough...
	/*
	if (EFI_ERROR(status)) {
		printf("Error reading memory map!\n");
		return 0;
	}
	*/
	//BS->Stall(4000000);
	printf("try fopen...\n");
	FILE* f = fopen("\\EFI\\AXLE\\KERNEL.ELF", "r");
	printf("F: %p\n", f);
	exit_bs();
	while (1) {}
	while (1) {}

	return 0;
	/*
	efi_gop_t* gop = NULL;
	efi_guid_t gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	efi_status_t status = BS->LocateProtocol(&gop_guid, NULL, (void**)&gop);
	*/

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
