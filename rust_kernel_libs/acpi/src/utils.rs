use alloc::string::String;

/// PT: Matches the definitions in kernel/util/vmm
const KERNEL_MEMORY_BASE: usize = 0xFFFF800000000000;

/// Converts a physical address to its corresponding remapped address in high memory
fn phys_addr_to_remapped_high_memory_virt_addr(phys_addr: usize) -> usize {
    KERNEL_MEMORY_BASE + phys_addr
}

pub fn parse_struct_at_virt_addr<T>(virt_addr: usize) -> &'static T {
    unsafe { &*(virt_addr as *const T) }
}

pub fn parse_struct_at_phys_addr<T>(phys_addr: usize) -> &'static T {
    let virt_addr = phys_addr_to_remapped_high_memory_virt_addr(phys_addr);
    parse_struct_at_virt_addr(virt_addr)
}

pub fn get_tabs(num: usize) -> String {
    "\t".repeat(num)
}
