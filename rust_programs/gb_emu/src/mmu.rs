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
}

impl BootRom {
    pub fn new(bootrom_path: &str) -> Self {
        Self {
            data: std::fs::read(bootrom_path).unwrap(),
        }
    }
    fn from_bytes(bytes: &[u8]) -> Self {
        Self {
            data: bytes.to_vec(),
        }
    }
}

impl Addressable for BootRom {
    fn contains(&self, addr: u16) -> bool {
        (addr as usize) < self.data.len()
    }
    fn read(&self, addr: u16) -> u8 {
        self.data[addr as usize]
    }

    fn write(&self, addr: u16, val: u8) {
        panic!("Cannot write to boot ROM")
    }
}

pub struct GameRom {
    data: Vec<u8>,
}

impl GameRom {
    pub fn new(rom_path: &str) -> Self {
        let data = std::fs::read(rom_path).unwrap();
        assert!(data.len() <= 0x8000, "Cannot load ROMs larger than 32k yet");
        Self { data }
    }
}

impl Addressable for GameRom {
    fn contains(&self, addr: u16) -> bool {
        (addr as usize) < self.data.len()
    }
    fn read(&self, addr: u16) -> u8 {
        self.data[addr as usize]
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
        //println!("RAM write 0x{addr:04x}: {val:02x}");
        self.data.borrow_mut()[(addr - self.start_addr) as usize] = val
    }
}

#[test]
fn test_bootrom_reads() {
    //let bootrom = BootRom::new("/Users/philliptennen/Downloads/DMG_ROM.bin");
    let bootrom = Rc::new(BootRom::from_bytes(&[0x11, 0x22]));
    let mmu = Mmu::new(vec![bootrom]);
    assert_eq!(mmu.read(0x00), 0x11);
    assert_eq!(mmu.read(0x01), 0x22);
    assert_eq!(mmu.read(0x02), 0xff);
    assert_eq!(mmu.read(0x5566), 0xff);
}
