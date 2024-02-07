// Note that nothing within this module is allowed to allocate memory. The data structures must
// live in reserved blocks.
// This makes it dangerous to use, for example, println or anything that will invoke string
// formatting machinery.
#![cfg_attr(target_os = "axle_kernel", no_std)]
#![feature(format_args_nl)]
#![feature(cstr_from_bytes_until_nul)]
#![feature(default_alloc_error_handler)]

extern crate ffi_bindings;

use core::ffi::CStr;
use core::usize::MAX;
use heapless::spsc::Queue;
use spin::Mutex;

#[cfg(target_os = "axle_kernel")]
use heapless::Vec;
#[cfg(not(target_os = "axle_kernel"))]
use std::vec::Vec;

use ffi_bindings::cstr_core::CString;
use ffi_bindings::{
    assert, boot_info_get, println, BootInfo, PhysicalMemoryRegionType, _panic,
    phys_addr_to_virt_ram_remap, printf, PhysicalAddr,
};

// PT: Must match the definitions in kernel/ap_bootstrap.h
const AP_BOOTSTRAP_CODE_PAGE: usize = 0x8000;
const AP_BOOTSTRAP_DATA_PAGE: usize = 0x9000;

static FRAMES_TO_HIDE_FROM_PMM: &'static [usize] = &[
    // Used for AP bootstrap code + data
    AP_BOOTSTRAP_CODE_PAGE,
    AP_BOOTSTRAP_DATA_PAGE,
];

const PAGE_SIZE: usize = 0x1000;
const MEGABYTE: usize = 1024 * 1024;
const GIGABYTE: usize = MEGABYTE * 1024;
// Maximum physical memory that the PMM can keep track of (ie. the maximum physical memory we can
// allocate)
const MAX_MEMORY_ALLOCATOR_CAN_BOOKKEEP: usize = GIGABYTE * 16;
const MAX_FRAMES_ALLOCATOR_CAN_BOOKKEEP: usize = MAX_MEMORY_ALLOCATOR_CAN_BOOKKEEP / PAGE_SIZE;
// Reserve some memory for the contiguous physical chunk pool
const CONTIGUOUS_CHUNK_POOL_SIZE: usize = MEGABYTE * 128;
const MAX_CONTIGUOUS_CHUNK_FRAMES: usize = CONTIGUOUS_CHUNK_POOL_SIZE / PAGE_SIZE;

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
struct PhysicalFrame(usize);

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
struct ContiguousChunk {
    base: PhysicalFrame,
    size: usize,
}

impl ContiguousChunk {
    fn new(base: PhysicalFrame, size: usize) -> Self {
        Self { base, size }
    }
}

struct ContiguousChunkPoolDescription {
    base: usize,
    total_size: usize,
}

impl ContiguousChunkPoolDescription {
    fn new(base: usize, total_size: usize) -> Self {
        Self { base, total_size }
    }
}

struct ContiguousChunkPool {
    pool_description: Option<ContiguousChunkPoolDescription>,

    #[cfg(target_os = "axle_kernel")]
    allocated_chunks: Vec<ContiguousChunk, MAX_CONTIGUOUS_CHUNK_FRAMES>,
    #[cfg(target_os = "axle_kernel")]
    free_chunks: Vec<ContiguousChunk, MAX_CONTIGUOUS_CHUNK_FRAMES>,
    #[cfg(not(target_os = "axle_kernel"))]
    allocated_chunks: Vec<ContiguousChunk>,
    #[cfg(not(target_os = "axle_kernel"))]
    free_chunks: Vec<ContiguousChunk>,
}
extern crate alloc;

impl ContiguousChunkPool {
    const fn new() -> Self {
        Self {
            pool_description: None,
            allocated_chunks: Vec::new(),
            free_chunks: Vec::new(),
        }
    }

    fn set_pool_description(&mut self, base: usize, total_size: usize) {
        println!("Setting pool description");
        self.pool_description = Some(ContiguousChunkPoolDescription::new(base, total_size));
        // Start off with a single free chunk the size of the entire pool
        #[cfg(target_os = "axle_kernel")]
        self.free_chunks
            .push(ContiguousChunk::new(PhysicalFrame(base), total_size))
            .unwrap();
        #[cfg(not(target_os = "axle_kernel"))]
        self.free_chunks
            .push(ContiguousChunk::new(PhysicalFrame(base), total_size));
    }

    fn is_pool_configured(&self) -> bool {
        self.pool_description.is_some()
    }

    fn alloc(&mut self, size: usize) -> usize {
        // Look for a chunk big enough to satisfy the allocation
        let mut chunk_to_drop = None;
        let mut allocated_chunk = None;
        for mut chunk in self.free_chunks.iter_mut() {
            // Is the chunk large enough to satisfy this allocation?
            if chunk.size >= size {
                let chunk_base = chunk.base.0;
                let new_size = chunk.size - size;
                if new_size == 0 {
                    // Remove the chunk entirely
                    chunk_to_drop = Some(chunk.clone());
                } else {
                    // Shrink the chunk to account for the fact that part of it is now allocated
                    chunk.base = PhysicalFrame(chunk_base + size);
                    chunk.size -= size;
                }
                // And add an allocated chunk
                allocated_chunk = Some(ContiguousChunk::new(PhysicalFrame(chunk_base), size));
                self.allocated_chunks.push(allocated_chunk.unwrap());
                break;
            }
        }
        if let Some(chunk_to_drop) = chunk_to_drop {
            self.free_chunks.retain(|i| *i == chunk_to_drop);
        }
        allocated_chunk.unwrap().base.0
    }
}

/// If we have a maximum of 16GB of RAM tracked, each array to track the frames will occupy 512kb.
static mut CONTIGUOUS_CHUNK_POOL: ContiguousChunkPool = ContiguousChunkPool::new();
static mut FREE_FRAMES: Mutex<Queue<PhysicalAddr, MAX_FRAMES_ALLOCATOR_CAN_BOOKKEEP>> =
    Mutex::new(Queue::new());

fn page_ceil(mut addr: usize) -> usize {
    ((addr) + PAGE_SIZE - 1) & !(PAGE_SIZE - 1)
}

fn page_floor(mut addr: usize) -> usize {
    (addr - (addr % PAGE_SIZE))
}

#[no_mangle]
pub unsafe fn pmm_init() {
    let boot_info = {
        let boot_info_raw = boot_info_get();
        &*boot_info_raw
    };

    // Mark usable sections of the address space
    // TODO(PT): Will there be any bug with PMM allocating the frame used for the init kernel stack?
    let mut free_frames_queue = FREE_FRAMES.lock();
    for region in &boot_info.mem_regions[..boot_info.mem_region_count as usize] {
        if region.region_type != PhysicalMemoryRegionType::Usable {
            continue;
        }
        // Floor each region to a frame boundary
        // This will cut off a bit of usable memory, but we'll only lose a few frames at most
        let base = page_ceil(region.addr);
        // Subtract whatever extra we got by aligning to a frame boundary above
        let mut region_size = page_floor(region.len - (base - region.addr));

        if !CONTIGUOUS_CHUNK_POOL.is_pool_configured() && region_size >= CONTIGUOUS_CHUNK_POOL_SIZE
        {
            printf("Found a free memory region that can serve as the contiguous chunk pool %p to %p\n\0".as_ptr() as *const u8, base, base + region_size);
            CONTIGUOUS_CHUNK_POOL.set_pool_description(base, CONTIGUOUS_CHUNK_POOL_SIZE);
            // Trim the contiguous chunk pool from the region and allow the rest of the frames to
            // be given to the general-purpose allocator
            region_size -= CONTIGUOUS_CHUNK_POOL_SIZE;
        }

        let page_count = (region_size + (PAGE_SIZE - 1)) / PAGE_SIZE;
        for page_idx in 0..page_count {
            let frame_addr = base + (page_idx * PAGE_SIZE);
            if FRAMES_TO_HIDE_FROM_PMM.contains(&frame_addr) {
                continue;
            }
            // Keep track of the frame
            free_frames_queue.enqueue(PhysicalAddr(frame_addr));
        }
    }
}

#[no_mangle]
pub unsafe fn pmm_alloc() -> usize {
    let mut free_frames_queue = FREE_FRAMES.lock();
    if free_frames_queue.is_empty() {
        // Invoke _panic directly, since the panic! macro will invoke string formatting
        // machinery (which is not allowed here)
        _panic(
            "Exhausted available physical frames\0".as_ptr() as *const u8,
            "pmm/lib.rs\0".as_ptr() as *const u8,
            0,
        );
    }
    let allocated_frame = free_frames_queue.dequeue().unwrap();

    // Memset the frame to all zeroes, for convenience
    let frame_in_ram_remap = phys_addr_to_virt_ram_remap(allocated_frame);
    let frame_slice_in_vmem = {
        let raw_slice =
            core::ptr::slice_from_raw_parts_mut(frame_in_ram_remap.0 as *mut u8, PAGE_SIZE);
        &mut *raw_slice
    };
    frame_slice_in_vmem.fill(0);

    return allocated_frame.0;
}

#[no_mangle]
pub unsafe fn pmm_free(frame_addr: usize) {
    let mut free_frames_queue = FREE_FRAMES.lock();
    free_frames_queue.enqueue(PhysicalAddr(frame_addr)).unwrap();
}

#[no_mangle]
pub unsafe fn pmm_alloc_continuous_range(size: usize) -> usize {
    let ret = CONTIGUOUS_CHUNK_POOL.alloc(size);
    printf(
        "pmm_alloc_contiguous_range(0x%p) = 0x%p\n\0".as_ptr() as *const u8,
        size,
        ret,
    );
    ret
}

#[cfg(test)]
mod test {
    use crate::{ContiguousChunk, ContiguousChunkPool, PhysicalFrame};

    #[test]
    fn basic_allocation() {
        // Given an empty pool
        let mut pool = Box::new(ContiguousChunkPool::new());
        pool.set_pool_description(0x10000, 0x20000);
        // When I allocate a block
        let allocated_chunk = pool.alloc(0x4000);
        // Then it's allocated at the beginning
        assert_eq!(allocated_chunk, 0x10000);
        // And the free chunks are split as expected
        assert_eq!(
            pool.free_chunks,
            vec![ContiguousChunk::new(PhysicalFrame(0x14000), 0x1c000)]
        );
    }
}
