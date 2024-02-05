#include <uefi.h>
#include <stdbool.h>

#include "elf.h"
#include "axle_boot_info.h"
#include "paging.h"

// Define _fltused, since we're not linking against the MS C runtime, but use
// floats.
// https://fantashit.com/undefined-symbol-fltused-when-compiling-to-x86-64-unknown-uefi/
int _fltused = 0;

// TODO(PT): Expose the kernel ELF sections in the boot info, so the PMM can reserve them and we can store the symbol table
uint64_t kernel_map_elf(const char* kernel_filename, pml4e_t* vas_state, axle_boot_info_t* out_boot_info) {
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

	// Load ELF segments
	uint8_t* section_headers_base = (uint8_t*)(kernel_buf + elf->e_shoff);
	Elf64_Shdr* string_table_section_header = (Elf64_Shdr*)(section_headers_base + (elf->e_shstrndx * elf->e_shentsize));
	Elf64_Shdr* s = string_table_section_header;
	uint8_t* string_table = kernel_buf + string_table_section_header->offset;
	for (uint64_t i = 0; i < elf->e_shnum; i++) {
		Elf64_Shdr* section_header = (Elf64_Shdr*)(section_headers_base + (i * elf->e_shentsize));
		//printf("Section header at 0x%p, off 0x%p size 0x%p, name 0x%p %s\n", section_header->addr, section_header->offset, section_header->size, section_header->name, &string_table[section_header->name]);
		const char* section_name = &string_table[section_header->name];

		if (!strncmp(section_name, ".strtab", 8) || !strncmp(section_name, ".symtab", 8)) {
			int page_count = ROUND_TO_NEXT_PAGE(section_header->size) / PAGE_SIZE;
			efi_physical_address_t section_data_buf = 0;
			efi_status_t status = BS->AllocatePages(AllocateAnyPages, EFI_PAL_CODE, page_count, &section_data_buf);
			if (EFI_ERROR(status)) {
				printf("Failed to allocate memory for kernel section data! %ld\n", status);
				return false;
			}
			memcpy((void*)section_data_buf, (kernel_buf + section_header->offset), section_header->size);

			if (!strncmp(section_name, ".strtab", 8)) {
				out_boot_info->kernel_string_table_base = section_data_buf;
				out_boot_info->kernel_string_table_size = section_header->size;
			}
			else {
				out_boot_info->kernel_symbol_table_base = section_data_buf;
				out_boot_info->kernel_symbol_table_size = section_header->size;
			}
		}
	}
	for (uint64_t i = 0; i < elf->e_phnum; i++) {
		Elf64_Phdr* phdr = (Elf64_Phdr*)(kernel_buf + elf->e_phoff + (i * elf->e_phentsize));
		//printf("PH at 0x%p\n", phdr);
		if (phdr->p_type == PT_LOAD) {
			uint64_t bss_size = phdr->p_memsz - phdr->p_filesz;
			//printf("ELF segment %p %d bytes (bss %d bytes)\n", phdr->p_vaddr, phdr->p_filesz, bss_size);

			efi_physical_address_t segment_phys_base = 0;
			int segment_size = phdr->p_memsz;
			int segment_size_page_padded = ROUND_TO_NEXT_PAGE(phdr->p_memsz);
			int page_count = segment_size_page_padded / PAGE_SIZE;
			//printf("\tAllocating %ld pages for ELF segment %ld\n", page_count, i);
			efi_status_t status = BS->AllocatePages(AllocateAnyPages, EFI_PAL_CODE, page_count, &segment_phys_base);
			if (EFI_ERROR(status)) {
				printf("Failed to map kernel segment at requested address\n");
				printf("Status: %ld\n", status);
				return 0;
			}

			memcpy((void*)segment_phys_base, kernel_buf + phdr->p_offset, phdr->p_filesz);
			memset((void*)(segment_phys_base + phdr->p_filesz), 0, bss_size);

			printf("Mapping [phys 0x%p - 0x%p] - [virt 0x%p - 0x%p]\n", segment_phys_base, segment_phys_base + segment_size_page_padded, phdr->p_vaddr, phdr->p_vaddr + segment_size_page_padded);
			map_region_4k_pages(vas_state, phdr->p_vaddr, segment_size_page_padded, segment_phys_base);
		}
	}

	uintptr_t kernel_entry_point = elf->e_entry;
	free(kernel_buf);
	return kernel_entry_point;
}

void print_time(void) {
	efi_time_t time = {0};
	RT->GetTime(&time, NULL);
	printf("%02d/%02d/%04d %02d:%02d:%02d.%04d", time.Day, time.Month, time.Year, time.Hour, time.Minute, time.Second, time.Nanosecond);
}

void wait(void) {
	efi_time_t start = {0};
	RT->GetTime(&start, NULL);
	while (true) {
		efi_time_t now = {0};
		RT->GetTime(&now, NULL);
		if (now.Minute != start.Minute || now.Second >= start.Second + 3) {
			break;
		}
	}
}

void draw(axle_boot_info_t* bi, int color) {
	uint32_t* base = (uint32_t*)bi->framebuffer_base;
	for (uint32_t y = 0; y < bi->framebuffer_height; y++) {
		for (uint32_t x = 0; x < bi->framebuffer_width; x++) {
			base[y * bi->framebuffer_width + x] = color;
		}
	}
	//wait();
}

bool map_file(const char* file_path, uint64_t* out_base, uint64_t* out_size) {
	FILE* file = fopen(file_path, "r");
	if (!file) {
		printf("Failed to open %s!\n", file_path);
		return false;
	}
	fseek(file, 0, SEEK_END);
	uint64_t file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	uint64_t file_page_count = (file_size / PAGE_SIZE) + 1;
	printf("%s size 0x%p, page count %ld\n", file_path, file_size, file_page_count);
	efi_physical_address_t file_buf = 0;
	efi_status_t status = BS->AllocatePages(AllocateAnyPages, EFI_PAL_CODE, file_page_count, &file_buf);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate memory for %s! %ld\n", file_path, status);
		return false;
	}
	printf("Allocated buffer for %s: 0x%p\n", file_path, file_buf);
	print_time();
	printf(": Reading %s buffer...\n", file_path);
	fread((void*)file_buf, file_size, 1, file);
	print_time();
	printf(": Mapped %s\n", file_path);
	fclose(file);

	*out_base = file_buf;
	*out_size = file_size;

	return true;
}

int main(int argc, char** argv) {
	ST->ConOut->ClearScreen(ST->ConOut);

	// Step 1: Allocate the buffer we'll use to pass all the info to the kernel
	// We've got to do this before reading the memory map, as allocations modify
	// the memory map.
	axle_boot_info_t* boot_info = calloc(1, sizeof(axle_boot_info_t));

	// Step 3: Select a graphics mode
	efi_gop_t* gop = NULL;
	efi_guid_t gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	efi_status_t status = BS->LocateProtocol(&gop_guid, NULL, (void**)&gop);
	if (EFI_ERROR(status)) {
		printf("Failed to locate GOP!\n");
		return 0;
	}

	uint64_t gop_mode_info_size = 0;
	efi_gop_mode_info_t* gop_mode_info = NULL;
	uint64_t best_mode = gop->Mode->Mode;
	uint64_t best_mode_res_x = 0;
	uint64_t max_res_x = 1600;
	// Desired aspect ratio is 16:9
	double desired_aspect_ratio = 16.0 / 9.0;
	double min_distance = 1000000.0;
	
	for (uint64_t i = gop->Mode->Mode; i < gop->Mode->MaxMode; i++) {
		gop->QueryMode(gop, i,  &gop_mode_info_size,  &gop_mode_info);
		/*
		printf(
			"\tMode %ld: %ldx%ld, %ld bpp, %d px/scanline, %p info, (%p, %p, %p, %p), %p version\n", 
			i, 
			gop_mode_info->HorizontalResolution, 
			gop_mode_info->VerticalResolution, 
			gop_mode_info->PixelFormat, 
			gop_mode_info->PixelsPerScanLine,
			gop_mode_info->PixelInformation,
			gop_mode_info->PixelInformation.RedMask,
			gop_mode_info->PixelInformation.GreenMask,
			gop_mode_info->PixelInformation.BlueMask,
			gop_mode_info->PixelInformation.ReservedMask,
			gop_mode_info->Version
		);
		*/
		double aspect_ratio = gop_mode_info->HorizontalResolution / (double)gop_mode_info->VerticalResolution;
		// Found a more precise fit for our desired aspect ratio?
		if (abs(desired_aspect_ratio - aspect_ratio) <= min_distance) {
			// Higher resolution than our previous best?
			if (gop_mode_info->HorizontalResolution > best_mode_res_x && gop_mode_info->HorizontalResolution <= max_res_x) {
				//printf("\tFound new preferred resolution: mode #%ld @ %ldx%ld\n", i, gop_mode_info->HorizontalResolution, gop_mode_info->VerticalResolution);
				best_mode = i;
				best_mode_res_x = gop_mode_info->HorizontalResolution;
				min_distance = abs(desired_aspect_ratio - aspect_ratio);
			}
		}
	}
	gop->QueryMode(gop, best_mode,  &gop_mode_info_size,  &gop_mode_info);
	gop->SetMode(gop, best_mode);
	boot_info->framebuffer_base = gop->Mode->FrameBufferBase;
	boot_info->framebuffer_width = gop->Mode->Information->HorizontalResolution;
	boot_info->framebuffer_height = gop->Mode->Information->VerticalResolution;
	// TODO(PT): Update me
	boot_info->framebuffer_bytes_per_pixel = 4;
	boot_info->framebuffer_pixels_per_scanline = gop->Mode->Information->PixelsPerScanLine;

	//ST->ConOut->ClearScreen(ST->ConOut);
	//draw(boot_info, 0x00ED93D4);
	draw(boot_info,   0x00e3a3d4);

	printf("axle OS bootloader init...\n");
	printf("Desired aspect ratio: %f\n", desired_aspect_ratio);
	printf("Selected Mode %ld: %ldx%ld, %ld bpp\n", best_mode, gop_mode_info->HorizontalResolution, gop_mode_info->VerticalResolution, gop_mode_info->PixelFormat);

	pml4e_t* page_mapping_level4 = map2();
	boot_info->boot_pml4 = (uint64_t)page_mapping_level4;

	// Step 2: Map the kernel ELF into memory
	uint64_t kernel_entry_point = kernel_map_elf("\\EFI\\AXLE\\KERNEL.ELF", page_mapping_level4, boot_info);
	if (!kernel_entry_point) {
		printf("Failed to map kernel\n");
		return 0;
	}

	// Step 3: Map the filesystem server and its ramdisk into memory
	if (!map_file("\\EFI\\AXLE\\FS_SERVER.ELF", &boot_info->file_server_elf_base, &boot_info->file_server_elf_size)) {
		printf("Failed to map FS server!\n");
		return 0;
	}
	if (!map_file("\\EFI\\AXLE\\INITRD.IMG", &boot_info->initrd_base, &boot_info->initrd_size)) {
		printf("Failed to map initrd!\n");
		return 0;
	}
    if (!map_file("\\EFI\\AXLE\\AP_BOOTSTRAP.BIN", &boot_info->ap_bootstrap_base, &boot_info->ap_bootstrap_size)) {
        printf("Failed to map AP bootstrap!\n");
        return 0;
    }

	// Step 5: Read the memory map
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
	/*
	printf("Set memory map size to: %p\n", memory_map_size);
	printf("Memory descriptors: %p\n", memory_descriptors);
	printf("Memory map key: %p\n", memory_map_key);
	printf("Memory descriptor size: %p\n", memory_descriptor_size);
	printf("Memory descriptor version: %p\n", memory_descriptor_version);
	*/

	// Allocate the buffer for the memory descriptors, 
	// but this will change the memory map and may increase its size!
	// Reserve some extra space just in case
	memory_map_size += (memory_descriptor_size * 4);
	printf("Reserved extra memory map space, memory map buffer size: %p\n", memory_map_size);
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

	// Note that the memory layout must be identical between 
	// efi_memory_descriptor_t and axle_efi_memory_descriptor_t, since we cast it here
	if (sizeof(efi_memory_descriptor_t) != sizeof(axle_efi_memory_descriptor_t)) {
		printf("efi_memory_descriptor_t and axle_efi_memory_descriptor_t were different sizes!\n");
		return 0;
	}
	boot_info->memory_descriptors = (axle_efi_memory_descriptor_t*)memory_descriptors;
	boot_info->memory_descriptor_size = memory_descriptor_size;
	boot_info->memory_map_size = memory_map_size;

    // Next, find the ACPI RSDP
    efi_guid_t acpi_1_0_rsdp_guid = (efi_guid_t){
        .Data1 = 0xeb9d2d30,
        .Data2 = 0x2d88,
        .Data3 = 0x11d3,
        .Data4 = {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d},
    };
    efi_guid_t acpi_2_0_rsdp_guid = (efi_guid_t){
            .Data1 = 0x8868e871,
            .Data2 = 0xe4f1,
            .Data3 = 0x11d3,
            .Data4 = {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81},
    };
    void* acpi_2_0_rsdp = NULL;

    efi_configuration_table_t* config_table = ST->ConfigurationTable;
    for (int i = 0; i < ST->NumberOfTableEntries; i++) {
        efi_configuration_table_t* table = &config_table[i];
        //printf("Looking at table %d at %x\n", i, table);
        efi_guid_t* guid = &table->VendorGuid;
        //printf("guid %08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x\n", guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
        if (!memcmp(guid, &acpi_1_0_rsdp_guid, sizeof(acpi_1_0_rsdp_guid))) {
            printf("\tFound ACPI 1.0 RSDP at index %d\n", i);
        }
        if (!memcmp(guid, &acpi_2_0_rsdp_guid, sizeof(acpi_2_0_rsdp_guid))) {
            acpi_2_0_rsdp = table->VendorTable;
            printf("\tFound ACPI 2.0 RSDP at index %d: 0x%x\n", i, (uintptr_t)acpi_2_0_rsdp);
        }
    }
    boot_info->acpi_rsdp = (uint64_t)acpi_2_0_rsdp;

    // Finally, exit UEFI-land and jump to the kernel
	printf("Jumping to kernel entry point at %p\n", kernel_entry_point);

	status = exit_bs();
	if (status != 0) {
		printf("Status %d\n", status);
		// Red
		draw(boot_info, 0x0000ffff);
		printf("Failed to exit boot services!\n");
		while (1) {}
		return 0;
	}

	asm volatile("movq %0, %%cr3" : : "r"(page_mapping_level4));

	// This should never return...
	(*((int(* __attribute__((sysv_abi)))(axle_boot_info_t*))(kernel_entry_point)))(boot_info);

	// If we got here, the kernel returned control to the bootloader
	// This should never happen with a well-behaved kernel...
	return 0;
}
