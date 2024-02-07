
# Notes that I'm not going to commit, eventually resulted in this blog post: https://axleos.com/screwing-up-my-page-tables/
task_small_t* _thread_create(void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    task_small_t* new_task = kmalloc(sizeof(task_small_t));
    // This is giving out addresses in very low pages?!
    // Allocated new_task 0000000000002110
    // Allocated new_task 00000000000045c0
    // These frames are being used to hold paging structures, and when they get overwritten, kaboom
    // Compiler is emitting a `cltq` instruction after the call to kmalloc(),
    // which truncates 0xffff900000000d70 to 0xd70
    // Jesus christ, it's because kmalloc wasn't defined
    // Another bug where the FRAME_MASK was truncating to 32 bits (0xfffff000), so when we enqueued frames that were atop
    // 4GB we were actually enqueueing the very low frames again, which are used for paging structures
    // Should probably reserve a NULL section like user-mode programs
    /*
    0000000000000000-00000000bee00000 00000000bee00000 -rw
    00000000bee00000-00000000bf000000 0000000000200000 -r-
    00000000bf000000-00000000bfa5b000 0000000000a5b000 -rw
    00000000bfa5b000-00000000bfa5c000 0000000000001000 -r-
    00000000bfa5c000-00000000bfa5e000 0000000000002000 -rw
    00000000bfa5e000-00000000bfa5f000 0000000000001000 -r-
    00000000bfa5f000-00000000bfa61000 0000000000002000 -rw
    00000000bfa61000-00000000bfa63000 0000000000002000 -r-
    00000000bfa63000-00000000bfa65000 0000000000002000 -rw
    00000000bfa65000-00000000bfa66000 0000000000001000 -r-
    00000000bfa66000-00000000bfa68000 0000000000002000 -rw
    00000000bfa68000-00000000bfac3000 000000000005b000 -r-
    00000000bfac3000-00000000bfadf000 000000000001c000 -rw
    00000000bfadf000-00000000bfae0000 0000000000001000 -r-
    00000000bfae0000-00000000bfae3000 0000000000003000 -rw
    00000000bfae3000-00000000bfae4000 0000000000001000 -r-
    00000000bfae4000-00000000bfae7000 0000000000003000 -rw
    00000000bfae7000-00000000bfae8000 0000000000001000 -r-
    00000000bfae8000-00000000bfaeb000 0000000000003000 -rw
    00000000bfaeb000-00000000bfaed000 0000000000002000 -r-
    00000000bfaed000-00000000bfc00000 0000000000113000 -rw
    00000000bfc00000-00000000bfe00000 0000000000200000 -r-
    00000000bfe00000-0000001000000000 0000000f40200000 -rw
    */
    // Another bug where, for some reason, we were copying the IDT pointer by copying the full size of the IDT, and it
    // spilled out of the AP bootstrap data page
    // Another bug where mlfq was showing a null task -- turned out to be because the array_l was buggy
    printf("Allocated new_task %p\n", new_task);




        /*
    fn set_pool_description(&mut self, base: usize, total_size: usize) {
        Looks like we're missing the top bit of the address space?!
        Mapping [phys 0x00000000adcb3000 - 0x00000000addb1fff] - [virt 0xffffffff80000000 - 0xffffffff800fefff]
        Mapping [phys 0x00000000abb97000 - 0x00000000adcaffff] - [virt 0xffffffff800ff000 - 0xffffffff82217fff]

        0000000000000000-0000001000000000 0000001000000000 -rw
        ffff800000000000-ffff801000000000 0000001000000000 -rw
        ffffffff80000000-ffffffff82200000 0000000002200000 -rw

        Cpu[0],Pid[4294967295],Clk[0]: [4294967295] Page fault at 0xffffffff82217138
        Cpu[0],Pid[4294967295],Clk[0]: |----------------------------------|
        Cpu[0],Pid[4294967295],Clk[0]: |        Page Fault in [4294967295]         |
        Cpu[0],Pid[4294967295],Clk[0]: | Unmapped read  of 0xffffffff82217138 |
        Cpu[0],Pid[4294967295],Clk[0]: |        RIP = 0xffffffff800d48a6  |
        Cpu[0],Pid[4294967295],Clk[0]: |        RSP = 0x00000000bfeba520  |
        Cpu[0],Pid[4294967295],Clk[0]: |----------------------------------|

        // The remaining_size pattern might not work when one of the structures we're populating is already partially filled
        // We think we only need 17 page tables, but since the first one is already mostly full, we under-allocate
         * Mapping [phys 0x00000000adcb2000 - 0x00000000addb1000] - [virt 0xffffffff80000000 - 0xffffffff800ff000]
        map_region [phys 0x00000000adcb2000 - 0x00000000addb1000] to [virt 0xffffffff80000000 - 0xffffffff800ff000], size 00000000000ff000
        Mapping [phys 0x00000000abb96000 - 0x00000000adcaf000] - [virt 0xffffffff800ff000 - 0xffffffff82218000]
        map_region [phys 0x00000000abb96000 - 0x00000000adcaf000] to [virt 0xffffffff800ff000 - 0xffffffff82218000], size 0000000002119000

        For some reason the rust structure was placed at the very top of the kernel address space
                                              qword_ffffffff82217138:
         0xffffffff82217138                        db  0x00 ; '.'                        ; DATA XREF=sub_ffffffff800d48a6, sub_ffffffff800d48a6+42, pmm_alloc+65, pmm_alloc+105, pmm_free+58, pmm_free+106
         */

        // Blog post: axle's task failure strategy. 'soft' failures for everything except early boot / death of critical services