#include <stdint.h>
#include <std/std.h>
#include "macho.h"
#include <kernel/util/vfs/fs.h>

void mach_dump_segments(FILE* mach);

void mach_load_file(char* filename) {
	printf("Parsing Mach-O file \'%s\'\n", filename);

	FILE* mach = fopen(filename, "rb");
	mach_dump_segments(mach);
	fclose(mach);
}

static uint32_t mach_read_magic(FILE* mach, int offset) {
	uint32_t magic;
	fseek(mach, offset, SEEK_SET);
	fread(&magic, sizeof(uint32_t), 1, mach);
	return magic;
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

static void mach_dump_segment_commands(FILE* mach, int offset, int should_swap, int count) {
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
			printf("Segment[%d] = %s [%x to %x]\n", i, segment->segname, segment->vmaddr, segment->vmaddr + segment->vmsize);

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

static const char* cpu_type_name(cpu_type_t cpu_type) {
	static int cpu_type_names_size = sizeof(cpu_type_names) / sizeof(struct _cpu_type_names);
	for (int i = 0; i < cpu_type_names_size; i++) {
		if (cpu_type == cpu_type_names[i].cputype) {
			return cpu_type_names[i].cpu_name;
		}
	}
	return "unknown";
}

static void mach_dump_header(FILE* mach, int offset, int is_64, int should_swap) {
	uint32_t cmds_count = 0;
	int load_commands_offset = offset;

	if (is_64) {
		//TODO add 64-bit support
		printf("Couldn't dump 64-bit mach\n");
	}
	else {
		struct mach_header* header = mach_load_bytes(mach, offset, sizeof(struct mach_header));
		if (should_swap) {
			//mach_swap_header(header, 0);
		}

		cmds_count = header->ncmds;
		load_commands_offset += sizeof(struct mach_header);

		printf("CPU type: %s\n", cpu_type_name(header->cputype));

		kfree(header);
	}
	mach_dump_segment_commands(mach, load_commands_offset, should_swap, cmds_count);
}

void mach_dump_segments(FILE* mach) {
	uint32_t magic = mach_read_magic(mach, 0);
	bool is_64 = mach_magic_64(magic);
	bool should_swap = mach_swap_bytes(magic);
	mach_dump_header(mach, 0, is_64, should_swap);
}

