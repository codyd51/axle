#include <stdint.h>
#include <std/std.h>
#include "macho.h"
#include <kernel/util/vfs/fs.h>
#include <kernel/util/multitasking/tasks/task.h>

void mach_load_segments(FILE* mach, int* entry_point, uint32_t slide);

void mach_load_file(char* filename) {
	printk("Loading Mach-O file \'%s\'\n", filename);

	//figure out virtual memory slide
	char* mach_slide = (char*)0xEFF00000;
	task_t* mach_task = task_with_pid(getpid());
	mach_task->vmem_slide = (uint32_t)mach_slide;

	FILE* mach = fopen(filename, "rb");
	int entry_point = 0;
	mach_load_segments(mach, &entry_point, mach_task->vmem_slide);
	fclose(mach);

	if (!entry_point) {
		printf_err("Couldn't find mach-o entry point");
		return;
	}

	void* mach_entry = (void*)entry_point + mach_task->vmem_slide;
	int(*mach_main)(void) = (int(*)(void))mach_entry;

	printf("jumping to mach main @ %x\n\n", mach_entry);
	mach_main();

	ASSERT(0, "returned from mach-o binary!\n");
}

static uint32_t mach_read_magic(FILE* mach, int offset) {
	uint32_t magic;
	fseek(mach, offset, SEEK_SET);
	fread(&magic, sizeof(uint32_t), 1, mach);

	fseek(mach, offset, SEEK_SET);
	char buf[4];
	for (uint32_t i = 0; i < sizeof(buf); i++) {
		buf[i] = fgetc(mach);
	}
	fseek(mach, offset, SEEK_SET);

	return magic;
}

bool mach_validate(FILE* mach) {
	//uint32_t magic = mach_read_magic(mach, 0);
	fseek(mach, 0, SEEK_SET);
	unsigned char buf[4] = {0};
	for (uint32_t i = 0; i < sizeof(buf); i++) {
		buf[i] = fgetc(mach);
	}
	fseek(mach, 0, SEEK_SET);

	if (buf[0] == 0xce &&
		buf[1] == 0xfa &&
		buf[2] == 0xed &&
		buf[3] == 0xfe) {
		return true;
	}
	return false;
}

static bool mach_magic_64(uint32_t magic) {
	return magic == MH_MAGIC_64 || magic == MH_CIGAM_64;
}

static bool mach_swap_bytes(uint32_t magic) {
	return magic == MH_CIGAM || magic == MH_CIGAM_64;
}

static void* mach_load_bytes(FILE* mach, int offset, int size) {
	void* buf = calloc(1, size);
	fseek(mach, offset, SEEK_SET);
	fread(buf, size, 1, mach);
	return buf;
}

static void mach_load_segment_commands(FILE* mach, int offset, int should_swap, int count, char* buf, int* entry_point, uint32_t slide) {
	int real = offset;
	for (int i = 0; i < count; i++) {
		struct load_command* cmd = mach_load_bytes(mach, real, sizeof(struct load_command));
		if (cmd->cmd == LC_SEGMENT_64) {
			printf("can't dump segs of 64b\n");
		}
		else if (cmd->cmd == LC_SEGMENT) {
			struct segment_command* segment = mach_load_bytes(mach, real, sizeof(struct segment_command));
			if (should_swap) {
				//swap_segment_command(segment, 0);
			}
			/*
			printf("Segment[%d] = %s [%x to %x], %d sections\n", i, 
													segment->segname, 
													slide + segment->vmaddr, 
													slide + segment->vmaddr + segment->vmsize,
													segment->nsects);
													*/

			char* segment_start = buf + segment->fileoff;
			char* vmem_seg_start = slide + (char*)segment->vmaddr;
			memset(vmem_seg_start, 0, segment->vmsize);
			memcpy(vmem_seg_start, segment_start, segment->filesize);

			if (segment->nsects) {
				for (uint32_t j = 0; j < segment->nsects; j++) {
					int sect_offset = real + sizeof(struct segment_command) + (sizeof(struct section) * j); 
					struct section* sect = mach_load_bytes(mach, sect_offset, sizeof(struct section));
					//printf("    %s section %d: addr %x size %x\n", sect->segname, j, sect->addr, sect->size);

					//if this seems to be the entry point, record it
					//TODO check for LC_MAIN once we port libSystem and don't have to define our own entry point
					//this is just a hack
					//we assume that if section is in the TEXT segment, that the first addr will be the entry point
					if (strstr(sect->segname, "TEXT") && !(*entry_point)) {
						*entry_point = sect->addr;
					}
					kfree(sect);
				}
			}

			kfree(segment);
		}

		real += cmd->cmdsize;
		kfree(cmd);
	}
}

struct _cpu_type_names {
	cpu_type_t cputype;
	const char* cpu_name;
};

#define CPU_TYPE_I386	7
#define CPU_TYPE_X86_64 1777223
#define CPU_TYPE_ARM	12
#define CPU_TYPE_ARM64	166777228

static struct _cpu_type_names cpu_type_names[] = {
	{CPU_TYPE_I386,		"i386"},
	{CPU_TYPE_X86_64,	"x86_64"},
	{CPU_TYPE_ARM,		"arm"},
	{CPU_TYPE_ARM64,	"arm64"}
};

const char* cpu_type_name(cpu_type_t cpu_type) {
	static int cpu_type_names_size = sizeof(cpu_type_names) / sizeof(struct _cpu_type_names);
	for (int i = 0; i < cpu_type_names_size; i++) {
		if (cpu_type == cpu_type_names[i].cputype) {
			return cpu_type_names[i].cpu_name;
		}
	}
	return "unknown";
}

static void mach_load_from_header(FILE* mach, int offset, int is_64, int should_swap, char* filebuf, int* entry_point, uint32_t slide) {
	uint32_t cmds_count = 0;
	int load_commands_offset = offset;

	if (is_64) {
		//TODO add 64-bit support
		printf("Couldn't dump 64-bit mach\n");
		return;
	}
	else {
		struct mach_header* header = mach_load_bytes(mach, offset, sizeof(struct mach_header));
		if (should_swap) {
			//mach_swap_header(header, 0);
		}

		cmds_count = header->ncmds;
		load_commands_offset += sizeof(struct mach_header);

		/*
		printf("CPU type: %s (%s)\n", cpu_type_name(header->cputype),
									  (header->cputype == CPU_TYPE_I386) ? "Supported" :
																			"Unsupported");
		*/

		kfree(header);
	}
	mach_load_segment_commands(mach, load_commands_offset, should_swap, cmds_count, filebuf, entry_point, slide);
}

void mach_load_segments(FILE* mach, int* entry_point, uint32_t slide) {
	uint32_t magic = mach_read_magic(mach, 0);
	bool is_64 = mach_magic_64(magic);
	bool should_swap = mach_swap_bytes(magic);

	//find file size
	fseek(mach, 0, SEEK_END);
	uint32_t size = ftell(mach);
	fseek(mach, 0, SEEK_SET);

	//map mach o from file into memory
	char* filebuf = kmalloc(size);
	for (uint32_t i = 0; i < size; i++) {
		filebuf[i] = fgetc(mach);
	}

	mach_load_from_header(mach, 0, is_64, should_swap, filebuf, entry_point, slide);
	kfree(filebuf);
}

