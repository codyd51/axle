use std::{
    cell::RefCell,
    mem,
    rc::{Rc, Weak},
};

// Ref for the concept of an Addressable interface:
// https://blog.tigris.fr/2019/07/28/writing-an-emulator-memory-management/
pub trait Addressable {
    fn contains(&self, addr: u16) -> bool;
    fn read(&self, addr: u16) -> u8;
    fn write(&self, addr: u16, val: u8);
}

pub struct Mmu {
    // Regions are allowed to overlap, priority is defined by list-order
    regions: Vec<Rc<dyn Addressable>>,
}

impl Mmu {
    pub fn new(regions: Vec<Rc<dyn Addressable>>) -> Self {
        Self { regions }
    }

    pub fn read(&self, addr: u16) -> u8 {
        for region in &self.regions {
            if region.contains(addr) {
                return region.read(addr);
            }
        }
        println!("Read of unbound memory address 0x{addr:04x}");
        0xff
    }

    pub fn read_u16(&self, addr: u16) -> u16 {
        let mut val = 0u16;
        for i in 0..mem::size_of::<u16>() {
            let byte = self.read(addr + (i as u16));
            val |= (byte as u16) << ((i as u16) * 8);
        }
        val
    }

    pub fn write(&self, addr: u16, val: u8) {
        for region in &self.regions {
            if region.contains(addr) {
                region.write(addr, val);
                return;
            }
        }
    }

    pub fn write_u16(&self, addr: u16, val: u16) {
        self.write(addr, val as u8);
        self.write(addr + 1, (val >> 8) as u8);
    }
}

pub struct BootRom {
    data: Vec<u8>,
    is_disabled: RefCell<bool>,
}

impl BootRom {
    const BANK_REGISTER_ADDR: u16 = 0xff50;

    pub fn new(bootrom_path: &str) -> Self {
        Self {
            is_disabled: RefCell::new(false),
            data: std::fs::read(bootrom_path).unwrap(),
        }
    }

    fn from_bytes(bytes: &[u8]) -> Self {
        Self {
            is_disabled: RefCell::new(false),
            data: bytes.to_vec(),
        }
    }
}

impl Addressable for BootRom {
    fn contains(&self, addr: u16) -> bool {
        if *self.is_disabled.borrow() {
            return false;
        }
        match addr {
            BootRom::BANK_REGISTER_ADDR => true,
            addr => (addr as usize) < self.data.len(),
        }
    }
    fn read(&self, addr: u16) -> u8 {
        match addr {
            BootRom::BANK_REGISTER_ADDR => panic!("Reads are not supported on the bank register"),
            _ => self.data[addr as usize],
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            BootRom::BANK_REGISTER_ADDR => match val {
                0 => panic!("0 is not a valid value to write to the BANK register"),
                _ => *self.is_disabled.borrow_mut() = true,
            },
            _ => panic!("Cannot write to boot ROM"),
        }
    }
}

pub struct GameRom {
    data: RefCell<Vec<u8>>,
}

impl GameRom {
    pub fn new(rom_path: &str) -> Self {
        let data = std::fs::read(rom_path).unwrap();
        assert!(data.len() <= 0x8000, "Cannot load ROMs larger than 32k yet");
        Self {
            data: RefCell::new(data),
        }
    }
}

impl Addressable for GameRom {
    fn contains(&self, addr: u16) -> bool {
        (addr as usize) < self.data.borrow().len()
    }
    fn read(&self, addr: u16) -> u8 {
        self.data.borrow()[addr as usize]
    }

    fn write(&self, addr: u16, val: u8) {
        panic!("Cannot write to game ROM")
    }
}

pub struct Ram {
    start_addr: u16,
    end_addr: u16,
    data: RefCell<Vec<u8>>,
}

impl Ram {
    pub fn new(start_addr: u16, size: u16) -> Self {
        let data = vec![0; size as usize];
        Self {
            start_addr,
            end_addr: start_addr + size,
            data: RefCell::new(data),
        }
    }
}

impl Addressable for Ram {
    fn contains(&self, addr: u16) -> bool {
        addr >= self.start_addr && addr < self.end_addr
    }
    fn read(&self, addr: u16) -> u8 {
        self.data.borrow()[(addr - self.start_addr) as usize]
    }

    fn write(&self, addr: u16, val: u8) {
        self.data.borrow_mut()[(addr - self.start_addr) as usize] = val
    }
}

pub struct EchoRam {
    backing_ram: Rc<Ram>,
    start_addr: u16,
    end_addr: u16,
}

impl EchoRam {
    pub fn new(backing_ram: Rc<Ram>, start_addr: u16, size: u16) -> Self {
        Self {
            backing_ram,
            start_addr,
            end_addr: start_addr + size,
        }
    }
}

impl Addressable for EchoRam {
    fn contains(&self, addr: u16) -> bool {
        addr >= self.start_addr && addr < self.end_addr
    }
    fn read(&self, addr: u16) -> u8 {
        let offset = addr - self.start_addr;
        self.backing_ram.read(self.backing_ram.start_addr + offset)
    }

    fn write(&self, addr: u16, val: u8) {
        let offset = addr - self.start_addr;
        self.backing_ram.write(self.backing_ram.start_addr + offset, val)

pub struct DmaController {
    dma_transfer_start_address_factor: RefCell<u8>,
    dma_in_progress: RefCell<bool>,
    dma_cycle_count: RefCell<usize>,
}

impl DmaController {
    const DMA_ADDR: u16 = 0xff46;

    pub fn new() -> Self {
        Self {
            dma_transfer_start_address_factor: RefCell::new(0),
            dma_in_progress: RefCell::new(false),
            dma_cycle_count: RefCell::new(0),
        }
    }

    pub fn step(&self, system: &dyn GameBoyHardwareProvider) {
        let mmu = system.get_mmu();
        if *self.dma_in_progress.borrow() {
            if *self.dma_cycle_count.borrow() == 0 {
                //println!("Executing the DMA transfer...");
                // Execute the DMA transfer
                let source_address =
                    (*self.dma_transfer_start_address_factor.borrow() as u16) * 0x100;
                let oam_base = 0xfe00;
                for i in 0..40 {
                    let a = mmu.read(source_address + i + 0);
                    let b = mmu.read(source_address + i + 1);
                    let c = mmu.read(source_address + i + 2);
                    let d = mmu.read(source_address + i + 3);
                    //mmu.write(oam_base + i, mmu.read(source_address + i));
                    mmu.write(oam_base + i + 0, a);
                    mmu.write(oam_base + i + 1, b);
                    mmu.write(oam_base + i + 2, c);
                    mmu.write(oam_base + i + 3, d);
                }
            } else if *self.dma_cycle_count.borrow() == 160 {
                //println!("Finishing the DMA");
                *self.dma_in_progress.borrow_mut() = false;
            }
            *self.dma_cycle_count.borrow_mut() += 1;
        }
    }
}

impl Addressable for DmaController {
    fn contains(&self, addr: u16) -> bool {
        match addr {
            DmaController::DMA_ADDR => true,
            _ => false,
        }
    }

    fn read(&self, addr: u16) -> u8 {
        match addr {
            DmaController::DMA_ADDR => *self.dma_transfer_start_address_factor.borrow(),
            _ => panic!("Invalid address"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            DmaController::DMA_ADDR => {
                if *self.dma_in_progress.borrow() {
                    println!("Ignoring DMA write because DMA is already in progress");
                } else {
                    //println!("Enqueue DMA");
                    *self.dma_in_progress.borrow_mut() = true;
                    *self.dma_cycle_count.borrow_mut() = 0;
                    *self.dma_transfer_start_address_factor.borrow_mut() = val;
                }
            }
            _ => panic!("Invalid address"),
        }
    }
}

#[cfg(test)]
mod tests {
    use std::rc::Rc;

    use crate::mmu::{Addressable, BootRom, Mmu};

    use super::Ram;

    #[test]
    fn test_bootrom_reads() {
        let bootrom = Rc::new(BootRom::from_bytes(&[0x11, 0x22]));
        let mmu = Mmu::new(vec![bootrom]);
        assert_eq!(mmu.read(0x00), 0x11);
        assert_eq!(mmu.read(0x01), 0x22);
        assert_eq!(mmu.read(0x02), 0xff);
        assert_eq!(mmu.read(0x5566), 0xff);
    }

    #[test]
    fn test_bank_register() {
        // Given an MMU containing a boot ROM
        let bootrom = Rc::new(BootRom::from_bytes(&[0x11, 0x22]));
        // And some more RAM that overlaps the boot ROM region
        let ram = Rc::new(Ram::new(0, 32));
        let ram_clone = Rc::clone(&ram);
        // And the boot rom has precedence
        let mmu = Mmu::new(vec![bootrom, ram_clone]);
        // And the RAM is filled with a value
        for i in 0..32 {
            ram.write(i, 0xcc);
        }

        // Then when I read from the MMU an address that is within the boot ROM
        assert_eq!(mmu.read(0), 0x11);
        // And when I read from the MMU an address that is outside the boot ROM
        assert_eq!(mmu.read(2), 0xcc);

        // When I write to the BANK register to disable the boot ROM
        mmu.write(0xff50, 1);

        // Then reading from the region previously mapped to the boot ROM
        // reads from the overlapping RAM instead
        assert_eq!(mmu.read(0), 0xcc);
    }
}
