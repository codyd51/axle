#include <uefi.h>

int fib(int c) {
	if (c < 2) return 1;
	return fib(c - 1) + fib(c - 2);
}

int main(int argc, char** argv) {
	printf("Hello world!\n");
	for (int i = 0; i < 30; i++) {
		printf("Fib %d: %ld\n", i, fib(i));
	}

	ST->ConOut->OutputString(ST->ConOut, L"abc\n");

	efi_gop_t* gop;
	efi_guid_t gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	int status = BS->LocateProtocol(&gop_guid, NULL, (void**)&gop);
	if (EFI_ERROR(status)) {
		printf("Unable to locate GOP!\n");
	}
	else {
		printf("Got gop 0x%08lx\n", gop);
		printf("Mode? 0x%08x\n", gop->Mode);

		uint64_t info_size = 0;
		uint64_t native_mode = 0;
		uint64_t max_mode = 0;
		efi_gop_mode_info_t* mode_info;
		status = gop->QueryMode(gop, gop->Mode == NULL ? 0 : gop->Mode->Mode, &info_size, &mode_info);
		if (status == EFI_NOT_STARTED) {
			printf("efi not started? wiki says to set mode 0\n");
			// This is what the wiki says to do:
			// https://wiki.osdev.org/GOP
			status = gop->SetMode(gop, 0);
		}
		if (EFI_ERROR(status)) {
			printf("Unable to get native mode\n");
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
	}

	return 0;
}

