use std::{cell::RefCell, rc::Rc};

use crate::{cpu::CpuState, mmu::Mmu, ppu::Ppu};

pub struct GameBoy {
    pub mmu: Rc<Mmu>,
    pub cpu: RefCell<CpuState>,
    pub ppu: Rc<Ppu>,
    pub cpu_disabled: RefCell<bool>,
}

impl GameBoy {
    pub fn new(mmu: Rc<Mmu>, cpu: CpuState, ppu: Rc<Ppu>) -> Self {
        Self {
            mmu,
            cpu: RefCell::new(cpu),
            ppu: ppu,
            cpu_disabled: RefCell::new(false),
        }
    }

    pub fn step(&self) {
        let mut d = self.cpu_disabled.borrow_mut();
        if !*d {
            let instr_info = self.cpu.borrow_mut().step();
            if instr_info.cycle_count == 0 {
                *d = true;
            }
        }
        self.ppu.step(&self.mmu);
    }
}
