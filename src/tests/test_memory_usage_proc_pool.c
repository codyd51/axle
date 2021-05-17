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

static void run_test(uint32_t i) {
    uint32_t start = ms_since_boot();
    printf("TEST %d: Before tasks spawn\n", i);
    /* 
    pmm_dump();
    liballoc_dump();
    */

    for (uint32_t i = 0; i < 16; i++) {
        task_spawn__with_args(_exiting, "abc", i, 0, "");
    }
    printf("TEST %d: Sleeping...\n", i);
    //while (ms_since_boot() < start + 500) {
    //}
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
    for (uint32_t i = 0; i < 512; i++) {
        run_test(i);
        uint32_t now = ms_since_boot();
        while (ms_since_boot() < now + 1500) {
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
