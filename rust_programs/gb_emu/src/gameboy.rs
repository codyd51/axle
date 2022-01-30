use std::{cell::RefCell, rc::Rc};

use crate::{cpu::CpuState, interrupts::InterruptController, mmu::Mmu, ppu::Ppu};

pub trait GameBoyHardwareProvider {
    fn get_mmu(&self) -> Rc<Mmu>;
    fn get_ppu(&self) -> Rc<Ppu>;
    fn get_interrupt_controller(&self) -> Rc<InterruptController>;
}

pub struct GameBoy {
    pub mmu: Rc<Mmu>,
    pub cpu: RefCell<CpuState>,
    pub ppu: Rc<Ppu>,
    pub interrupt_controller: Rc<InterruptController>,
    pub cpu_disabled: RefCell<bool>,
}

impl GameBoy {
    pub fn new(
        mmu: Rc<Mmu>,
        cpu: CpuState,
        ppu: Rc<Ppu>,
        interrupt_controller: Rc<InterruptController>,
    ) -> Self {
        Self {
            mmu,
            cpu: RefCell::new(cpu),
            ppu: ppu,
            interrupt_controller,
            cpu_disabled: RefCell::new(false),
        }
    }

    pub fn step(&self) {
        let mut d = self.cpu_disabled.borrow_mut();
        if !*d {
            let instr_info = self.cpu.borrow_mut().step(self);
            if instr_info.cycle_count == 0 {
                *d = true;
            }
        }
        self.ppu.step(&self.mmu);
    }
}

impl GameBoyHardwareProvider for GameBoy {
    fn get_mmu(&self) -> Rc<Mmu> {
        Rc::clone(&self.mmu)
    }

    fn get_ppu(&self) -> Rc<Ppu> {
        Rc::clone(&self.ppu)
    }

    fn get_interrupt_controller(&self) -> Rc<InterruptController> {
        Rc::clone(&self.interrupt_controller)
    }
}
