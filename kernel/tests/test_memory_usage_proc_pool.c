#include <stdint.h>
#include <kernel/pmm/pmm.h>
#include <kernel/multitasking/tasks/task_small.h>

static int cnt = 0;
static void _exiting(const char* p, uint32_t arg2, uint32_t arg3) {
    char buf[32];
    snprintf(buf, sizeof(buf), "com.axle.exit-%d", cnt);
    cnt += 1;
    amc_register_service(buf);
    task_die(0);
}

#include <kernel/util/vfs/vfs.h>
#include <kernel/boot_info.h>
static void _launch_program(const char* program_name, uint32_t arg2, uint32_t arg3) {
    static int i = 0;
    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", i);
    char* argv[] = {program_name, buf, NULL};
    i += 1;
    /*
	fs_node_t* file = finddir_fs(fs_root, (char*)program_name);

    FILE* fp = initrd_fopen(program_name, "rb");
    assert(fp, "Failed to open file");
    elf_load_file(program_name, fp, argv);
    */

    initrd_fs_node_t* node = vfs_find_initrd_node_by_name(program_name);
    uint32_t address = node->initrd_offset;
	elf_load_buffer(program_name, address, node->size, argv);
	panic("noreturn");
    //task_die(0);
}

static void run_test(uint32_t i) {
    uint32_t start = ms_since_boot();
    printf("TEST %d: Before tasks spawn\n", i);
    /* 
    pmm_dump();
    liballoc_dump();
    */

    char* program_name = "empty";
    for (uint32_t i = 0; i < 64; i++) {
        task_spawn__with_args(program_name, _launch_program, program_name, 0, 0);
    }
    printf("TEST %d: Sleeping...\n", i);
    while (ms_since_boot() < start + 1000) {
    }
    printf("TEST %d: Woke from sleep! now = %d start = %d\n", i, ms_since_boot(), start);
    printf("TEST %d: After tasks spawn\n", i);
    /*
    pmm_dump();
    liballoc_dump();
    */
}

void test_memory_usage_proc_pool(void) {
    pmm_dump();
    liballoc_dump();

    uint32_t pmm_start = pmm_allocated_memory();
    uint32_t heap_start = kheap_allocated_memory();
    for (uint32_t i = 0; i < 8; i++) {
        run_test(i);
        uint32_t now = ms_since_boot();
        while (ms_since_boot() < now + 2000) {
        }
    }
    printf("Finished all tests!\n");
    pmm_dump();
    liballoc_dump();

    uint32_t pmm_end = pmm_allocated_memory();
    uint32_t heap_end = kheap_allocated_memory();
    // We expect one page to be taken up by reaper
    // 4kb is used the first time a task is killed (so reaper can map in the remote PDir), 
    // so total memory usage should be (Initial) -> (Initial plus 4k), where the 4k is mapped on the first proc teardown
    uint32_t pmm_growth = (pmm_end - pmm_start - PAGING_FRAME_SIZE);
    uint32_t heap_growth = heap_end - heap_start;

    printf("*** PMM Growth:  0x%08x\n", pmm_growth);
    printf("*** Heap Growth: 0x%08x\n", heap_growth);

    tasking_print_processes();

    assert(pmm_growth == 0, "PMM grew!");
    assert(heap_growth == 0, "Kernel heap grew!");
    //while (1) {}
}
