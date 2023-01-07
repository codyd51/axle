use alloc::string::String;
use core::fmt::{Debug, Formatter};
use core::ops::Add;

extern "C" {
    fn ms_since_boot() -> usize;
}

/// PT: Matches the definitions in kernel/util/vmm
const KERNEL_MEMORY_BASE: usize = 0xFFFF800000000000;

#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
pub struct PhysAddr(pub usize);

impl PhysAddr {
    /// Converts a physical address to its corresponding remapped address in high memory
    pub fn to_remapped_high_memory_virt(&self) -> VirtRamRemapAddr {
        VirtRamRemapAddr(KERNEL_MEMORY_BASE + self.0)
    }
}

impl Debug for PhysAddr {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "Phys[{:#016x}]", self.0)
    }
}

impl Add<usize> for PhysAddr {
    type Output = Self;

    fn add(self, rhs: usize) -> Self::Output {
        Self(self.0 + rhs)
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
pub struct VirtRamRemapAddr(pub usize);

impl Debug for VirtRamRemapAddr {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "Virt[{:#016x}]", self.0)
    }
}

impl Add<usize> for VirtRamRemapAddr {
    type Output = Self;

    fn add(self, rhs: usize) -> Self::Output {
        Self(self.0 + rhs)
    }
}

pub fn parse_struct_at_virt_addr<T>(addr: VirtRamRemapAddr) -> &'static T {
    unsafe { &*(addr.0 as *const T) }
}

pub fn parse_struct_at_phys_addr<T>(addr: PhysAddr) -> &'static T {
    parse_struct_at_virt_addr(addr.to_remapped_high_memory_virt())
}

pub fn get_tabs(num: usize) -> String {
    "\t".repeat(num)
}

pub trait ContainsMachineWords {
    fn low_u32(&self) -> u32;
    fn high_u32(&self) -> u32;
}

impl ContainsMachineWords for u64 {
    fn low_u32(&self) -> u32 {
        *self as u32
    }
    fn high_u32(&self) -> u32 {
        (*self >> 32) as u32
    }
}

pub fn spin_for_delay_ms(delay: usize) {
    let now = unsafe { ms_since_boot() };
    loop {
        let later = unsafe { ms_since_boot() };
        if later - now >= delay {
            break;
        }
    }
}
