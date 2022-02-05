use std::{cell::RefCell, rc::Rc};

use crate::{
    cpu::CpuState,
    interrupts::InterruptController,
    joypad::Joypad,
    mmu::{BootRom, DmaController, Mmu},
    ppu::Ppu,
    timer::Timer,
    SerialDebugPort,
};

pub trait GameBoyHardwareProvider {
    fn get_cpu(&self) -> Rc<RefCell<CpuState>>;
    fn get_mmu(&self) -> Rc<Mmu>;
    fn get_ppu(&self) -> Rc<Ppu>;
    fn get_interrupt_controller(&self) -> Rc<InterruptController>;
    fn get_joypad(&self) -> Rc<Joypad>;
}

pub struct GameBoy {
    pub mmu: Rc<Mmu>,
    pub cpu: Rc<RefCell<CpuState>>,
    pub ppu: Rc<Ppu>,
    pub interrupt_controller: Rc<InterruptController>,
    pub serial_debug_port: Rc<SerialDebugPort>,
    pub timer: Rc<Timer>,
    pub joypad: Rc<Joypad>,
    pub dma_controller: Rc<DmaController>,
    pub cpu_disabled: RefCell<bool>,
}

impl GameBoy {
    pub fn new(
        mmu: Rc<Mmu>,
        cpu: CpuState,
        ppu: Rc<Ppu>,
        interrupt_controller: Rc<InterruptController>,
        serial_debug_port: Rc<SerialDebugPort>,
        timer: Rc<Timer>,
        joypad: Rc<Joypad>,
        dma_controller: Rc<DmaController>,
    ) -> Self {
        Self {
            mmu,
            cpu: Rc::new(RefCell::new(cpu)),
            ppu,
            interrupt_controller,
            serial_debug_port,
            timer,
            joypad,
            dma_controller,
            cpu_disabled: RefCell::new(false),
        }
    }

    pub fn mock_bootrom(&self) {
        // Disable the boot ROM
        self.mmu.write(BootRom::BANK_REGISTER_ADDR, 0x01);
        // TODO(PT): This should belong to the timer?
        // Ref: https://www.reddit.com/r/EmuDev/comments/h8x0pj/gameboy_initi
        //self.mmu.write(0xFF04, 0xab);
        // Set PC where the boot ROM passes control
        let mut cpu = self.cpu.borrow_mut();
        cpu.set_mock_bootrom_state();
        //cpu.enable_debug();
    }

    pub fn step(&self) {
        self.interrupt_controller.step(self);
        // TODO(PT): Handle when CPU is halted?
        let instr_info = self.cpu.borrow_mut().step(self);
        for i in 0..instr_info.cycle_count {
            self.ppu.step(self);
            self.joypad.step(self);
            self.dma_controller.step(self);
            self.timer.step(self);
        }
        //self.serial_debug_port.step(self);
    }
}

impl GameBoyHardwareProvider for GameBoy {
    fn get_mmu(&self) -> Rc<Mmu> {
        Rc::clone(&self.mmu)
    }

    fn get_ppu(&self) -> Rc<Ppu> {
        Rc::clone(&self.ppu)
    }

    fn get_cpu(&self) -> Rc<RefCell<CpuState>> {
        Rc::clone(&self.cpu)
    }

    fn get_interrupt_controller(&self) -> Rc<InterruptController> {
        Rc::clone(&self.interrupt_controller)
    }

    fn get_joypad(&self) -> Rc<Joypad> {
        Rc::clone(&self.joypad)
    }
}
