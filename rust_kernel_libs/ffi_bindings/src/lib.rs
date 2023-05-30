#![no_std]
#![feature(format_args_nl)]
#![feature(core_intrinsics)]
#![feature(panic_info_message)]
#![feature(default_alloc_error_handler)]

extern crate alloc;

use alloc::alloc::{alloc, GlobalAlloc, Layout};
use alloc::format;
use alloc::string::ToString;
use core::ffi::{c_char, c_void, CStr};
use core::intrinsics::size_of;
use core::mem::align_of;
use core::panic::PanicInfo;
pub use cstr_core;
use cstr_core::CString;

extern "C" {
    pub fn kmalloc(size: usize) -> *mut c_void;
    pub fn kfree(ptr: *mut c_char);

    pub fn interrupt_setup_callback(
        interrupt_vec: u8,
        callback: extern "C" fn(*const RegisterStateX86_64),
    );
    pub fn task_switch();
    pub fn task_die(exit_code: u32);
    pub fn amc_wake_sleeping_services();
    pub fn getpid() -> i32;

    // assert.c
    pub fn assert(condition: bool, message: *const u8) -> ();
    pub fn _panic(message: *const u8, file: *const u8, line: i64);

    // smp.h
    pub fn cpu_id() -> usize;

    // amc/amc.h
    pub fn amc_service_of_task(task: *const TaskControlBlock) -> *const AmcService;

    // amc/core_commands.c
    pub fn amc_core_populate_task_info_int(
        tasks_info: *mut TaskViewerGetTaskInfoResponse,
        task_idx: usize,
        task_tcb: *const TaskControlBlock,
    );

    // vmm.c
    pub fn vas_get_active_state() -> *const VasState;
    pub fn vas_load_state(state: *const VasState);
    pub fn vas_is_page_present(state: *const VasState, page_addr: u64) -> bool;

    // boot_info.h
    pub fn boot_info_get() -> *const BootInfo;

    // multitasking/tasks/mlfq.h
    pub fn mlfq_print();

    // printf.h
    pub fn printf(fmt: *const u8, args: ...) -> i32;
}

#[macro_export]
macro_rules! printf {
    ($($arg:tt)*) => ({
        let s = alloc::fmt::format(core::format_args!($($arg)*));
        for x in s.split('\0') {
            let log = ffi_bindings::cstr_core::CString::new(x).expect("printf format failed");
            unsafe { ffi_bindings::printf(log.as_ptr() as *const u8); }
        }
    })
}

#[macro_export]
macro_rules! println {
    () => ($crate::printf!("\n"));
    ($($arg:tt)*) => ({
        let s = alloc::fmt::format(core::format_args_nl!($($arg)*));
        for x in s.split('\0') {
            let log = ffi_bindings::cstr_core::CString::new(x).expect("printf format failed");
            unsafe { ffi_bindings::printf(log.as_ptr() as *const u8); }
        }
    })
}

macro_rules! internal_println {
    () => ($crate::printf!("\n"));
    ($($arg:tt)*) => ({
        let s = alloc::fmt::format(core::format_args_nl!($($arg)*));
        for x in s.split('\0') {
            let log = ::cstr_core::CString::new(x).expect("printf format failed");
            unsafe { printf(log.as_ptr() as *const u8); }
        }
    })
}

#[panic_handler]
pub fn panic(panic_info: &PanicInfo<'_>) -> ! {
    let msg = match panic_info.message() {
        Some(s) => format!("{}", s),
        None => "Box<Any>".to_string(),
    };

    if let Some(location) = panic_info.location() {
        internal_println!(
            "panic occurred in file '{}' at line {}",
            location.file(),
            location.line(),
        );
    }

    let c_to_print = CString::new(msg).expect("CString::new failed");
    let filename = CString::new("Rust file").unwrap();
    unsafe { printf(c_to_print.as_ptr() as *const u8) };
    unsafe {
        _panic(
            c_to_print.as_ptr() as *const u8,
            filename.as_ptr() as *const u8,
            0,
        );
    }
    loop {}
}

pub struct Dummy;

unsafe impl GlobalAlloc for Dummy {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        kmalloc(layout.size()) as *mut u8
    }
    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        kfree(ptr as *mut c_char);
    }
}

#[global_allocator]
pub static ALLOCATOR: Dummy = Dummy;

#[repr(C)]
#[derive(Debug)]
pub struct RegisterStateX86_64 {
    return_ds: u64,
    rax: u64,
    rcx: u64,
    rdx: u64,
    rbx: u64,
    rbp: u64,
    rsi: u64,
    rdi: u64,
    r8: u64,
    r9: u64,
    r10: u64,
    r11: u64,
    r12: u64,
    r13: u64,
    r14: u64,
    r15: u64,
    pub int_no: u64,
    err_code: u64,
    is_external_interrupt: u64,
    pub return_rip: u64,
    cs: u64,
    rflags: u64,
    pub return_rsp: u64,
    ss: u64,
}

/// Represents elf_t
#[repr(C)]
#[derive(Debug, Copy, Clone)]
struct ElfSymbolTableInfo {
    symbol_table: usize,
    symbol_table_size: u32,
    string_table: usize,
    string_table_size: u32,
}

/// Represents task_state_t
#[repr(C)]
#[derive(Debug, Copy, Clone)]
enum TaskState {
    Unknown = (0 << 0),
    Runnable = (1 << 0),
    Zombie = (1 << 1),
    KbWait = (1 << 2),
    PitWait = (1 << 3),
    MouseWait = (1 << 4),
    ChildWait = (1 << 5),
    PipeFull = (1 << 6),
    PipeEmpty = (1 << 7),
    IrqWait = (1 << 8),
    AmcAwaitMessage = (1 << 9),
    VmmModify = (1 << 10),
    AmcAwaitTimestamp = (1 << 11),
}

/// Represents task_block_state_t
#[repr(C)]
#[derive(Debug, Copy, Clone)]
struct TaskBlockState {
    status: TaskState,
    wake_timestamp: u32,
    unblock_reason: TaskState,
}

/// PT: Represents task_small_t
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct TaskControlBlock {
    pub pid: u32,
    pub name: usize,
    pub machine_state: *const TaskContext,
    blocked_info: TaskBlockState,
    next: *mut TaskControlBlock,
    current_timeslice_start_date: u64,
    current_timeslice_end_date: u64,
    queue: u32,
    lifespan: u32,
    is_thread: bool,
    pub vas_state: *const VasState,
    sbrk_base: usize,
    sbrk_current_break: usize,
    bss_segment_addr: usize,
    sbrk_current_page_head: usize,
    kernel_stack: usize,
    kernel_stack_malloc_head: usize,
    elf_symbol_table: ElfSymbolTableInfo,
    is_managed_by_parent: bool,
    managing_parent_service_name: usize,
    cpu_id: usize,
    is_currently_executing: bool,
    lock: Spinlock,
}

unsafe impl Send for TaskControlBlock {}
unsafe impl Sync for TaskControlBlock {}

/// Represents cpu_core_private_info_t
#[repr(C)]
#[derive(Debug)]
pub struct CpuCorePrivateInfo {
    pub processor_id: usize,
    pub apic_id: usize,
    pub local_apic_phys_addr: usize,
    pub base_vas: usize,
    pub loaded_vas_state: usize,
    pub current_task: usize,
    pub scheduler_enabled: bool,
    pub tss: usize,
    pub lapic_timer_ticks_per_ms: usize,
    pub idle_task: usize,
}

/// Represents spinlock_t
#[repr(C)]
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq)]
pub struct Spinlock {
    flag: u32,
    name: usize,
    interrupts_enabled_before_acquire: bool,
    owner_pid: u32,
    nest_count: u32,
    rflags: u64,
}

/// Represents amc_service_t
#[repr(C)]
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq)]
pub struct AmcService {
    pub name: usize,
    task: usize,
    message_queue: usize,
    spinlock: Spinlock,
    delivery_pool: usize,
    shmem_regions: usize,
    services_to_notify_upon_death: usize,
    delivery_enabled: bool,
}

impl AmcService {
    pub unsafe fn name(&self) -> &str {
        CStr::from_ptr(self.name as *const c_char).to_str().unwrap()
    }
}

/// Represents amc_service_t
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct AmcMessage {
    pub source: [u8; Self::MAX_SERVICE_NAME_LEN],
    dest: [u8; Self::MAX_SERVICE_NAME_LEN],
    // VLA
    pub len: u32,
    pub body: [u8; 0],
}

impl AmcMessage {
    pub const MAX_SERVICE_NAME_LEN: usize = 64;

    pub unsafe fn source(&self) -> &str {
        CStr::from_ptr(self.source.as_ptr() as *const c_char)
            .to_str()
            .unwrap()
    }
}

/// Represents task_viewer_get_task_info_response_t
#[repr(C)]
pub struct TaskViewerGetTaskInfoResponse {
    event: u32,
    pub task_info_count: u32,
    pub tasks: [TaskViewerTaskInfo; 0],
}

impl TaskViewerGetTaskInfoResponse {
    const EXPECTED_EVENT: u32 = 777;
    pub unsafe fn new(tasks_count: usize) -> *mut TaskViewerGetTaskInfoResponse {
        let total_size = size_of::<TaskViewerGetTaskInfoResponse>()
            + (size_of::<TaskViewerTaskInfo>() * tasks_count);
        let layout = Layout::from_size_align(total_size, align_of::<usize>()).unwrap();
        let mut tasks_info = alloc(layout) as *mut TaskViewerGetTaskInfoResponse;
        (*tasks_info).event = Self::EXPECTED_EVENT;
        (*tasks_info).task_info_count = tasks_count as u32;
        // The tasks must be filled in by the caller
        tasks_info
    }
}

/// Represents task_info_t
#[repr(C)]
pub struct TaskViewerTaskInfo {
    pub name: [u8; 64],
    pub pid: u32,
    pub rip: u64,
    pub vas_range_count: u64,
    pub vas_ranges: [VasRange; 16],
    pub user_mode_rip: u64,
    pub has_amc_service: bool,
    pub pending_amc_messages: u64,
}

/// Represents vas_range_t
#[repr(C)]
pub struct VasRange {
    pub start: u64,
    pub size: u64,
}

/// Represents task_context_t
#[repr(C)]
pub struct TaskContext {
    pub rbp: u64,
    pub rdi: u64,
    pub rsi: u64,
    pub rbx: u64,
    pub rax: u64,
    pub rip: u64,
}

/// Represents vas_state_t
#[repr(C)]
pub struct VasState {
    pml4_phys: usize,
    pub range_count: u32,
    max_range_count: u32,
    pub ranges: [VasRange; 0],
}

/// Represents physical_memory_region_type
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum PhysicalMemoryRegionType {
    Usable,
    Reserved,
    ReservedAcpiNvm,
    ReservedAxleKernelCodeAndData,
    ReservedAcpiTables,
}

/// Represents physical_memory_region_t
#[repr(C)]
#[derive(Debug)]
pub struct PhysicalMemoryRegion {
    pub region_type: PhysicalMemoryRegionType,
    pub addr: usize,
    pub len: usize,
}

/// Represents framebuffer_info_t
#[repr(C)]
#[derive(Debug)]
pub struct FramebufferInfo {
    address: usize,
    width: usize,
    height: usize,
    bits_per_pixel: u8,
    bytes_per_pixel: u8,
    pixels_per_scanline: u32,
    size: usize,
}

/// Represents boot_info_t
#[repr(C)]
#[derive(Debug)]
pub struct BootInfo {
    kernel_image_start: usize,
    kernel_image_end: usize,
    kernel_image_size: usize,

    file_server_elf_start: usize,
    file_server_elf_end: usize,
    file_server_elf_size: usize,

    initrd_start: usize,
    initrd_end: usize,
    initrd_size: usize,

    pub mem_region_count: u32,
    pub mem_regions: [PhysicalMemoryRegion; 256],

    kernel_elf_symbol_table: ElfSymbolTableInfo,
    framebuffer: FramebufferInfo,

    // Actually a vas_state_t*
    vas_kernel: usize,

    ms_per_pit_tick: u32,

    acpi_rsdp: usize,

    ap_bootstrap_base: usize,
    ap_bootstrap_size: usize,

    // Actually an smp_info_t*
    smp_info: usize,
}

#[derive(Debug, Copy, Clone)]
pub struct PhysicalAddr(pub usize);
#[derive(Debug, Copy, Clone)]
pub struct VirtualAddr(pub usize);

// Defined in vmm.h
pub const KERNEL_MEMORY_BASE: usize = 0xFFFF800000000000;

/// Equates to vmm.h:PMA_TO_VMA
pub fn phys_addr_to_virt_ram_remap(phys_addr: PhysicalAddr) -> VirtualAddr {
    VirtualAddr(phys_addr.0 + KERNEL_MEMORY_BASE)
}
