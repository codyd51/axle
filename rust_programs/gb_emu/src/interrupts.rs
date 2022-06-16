use core::{
    cell::RefCell,
    fmt::{self, Display},
};

use crate::{
    gameboy::GameBoyHardwareProvider,
    mmu::{Addressable, Mmu},
};

#[derive(Copy, Clone)]
pub enum InterruptType {
    VBlank,
    LCDStat,
    Timer,
    Serial,
    Joypad,
}

impl Display for InterruptType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match self {
            InterruptType::VBlank => "VBlank",
            InterruptType::LCDStat => "LCDStat",
            InterruptType::Timer => "Timer",
            InterruptType::Serial => "Serial",
            InterruptType::Joypad => "Joypad",
        };
        write!(f, "{}", name)
    }
}

pub struct InterruptController {
    interrupt_master_enable_flag: RefCell<bool>,
    interrupt_enable_register: RefCell<u8>,
    interrupt_flag_register: RefCell<u8>,
}

impl InterruptController {
    const INTERRUPT_ENABLE_REGISTER_ADDR: u16 = 0xffff;
    const INTERRUPT_FLAG_REGISTER_ADDR: u16 = 0xff0f;

    pub fn new() -> Self {
        Self {
            interrupt_master_enable_flag: RefCell::new(true),
            interrupt_enable_register: RefCell::new(0),
            interrupt_flag_register: RefCell::new(0),
        }
    }

    pub fn set_interrupts_globally_disabled(&self) {
        *(self.interrupt_master_enable_flag.borrow_mut()) = false
    }

    pub fn set_interrupts_globally_enabled(&self) {
        *(self.interrupt_master_enable_flag.borrow_mut()) = true
    }

    pub fn are_interrupts_globally_enabled(&self) -> bool {
        *(self.interrupt_master_enable_flag.borrow())
    }

    pub fn step(&self, system: &dyn GameBoyHardwareProvider) {
        // If interrupts are disabled and the CPU isn't halted, we have nothing to do
        if !self.are_interrupts_globally_enabled() && !system.get_cpu().borrow().is_halted {
            return;
        }

        // Check whether we should trigger any interrupts
        let mut flags_register = self.interrupt_flag_register.borrow_mut();
        let enabled_mask_register = *(self.interrupt_enable_register.borrow());
        for bit_index in 0..=4 {
            let interrupt_type = self.interrupt_type_for_bit_index(bit_index);
            // Has an interrupt been requested in the flags register?
            if *flags_register & (1 << bit_index) != 0 {
                // Is this interrupt enabled in the bitmask?
                if enabled_mask_register & (1 << bit_index) != 0 {
                    // If the CPU is in HALT-mode and has the IME flag disabled,
                    // just un-halt the CPU.
                    let cpu_ref = system.get_cpu();
                    if !self.are_interrupts_globally_enabled() && cpu_ref.borrow().is_halted {
                        cpu_ref.borrow_mut().set_halted(false);
                        return;
                    }

                    // Reset this IF bit
                    *flags_register &= !(1 << bit_index);

                    // Reset the IME flag during the interrupt handler
                    self.set_interrupts_globally_disabled();

                    // Push PC to the stack
                    cpu_ref.borrow_mut().call_interrupt_vector(interrupt_type);

                    // We've handled one interrupt - don't try to handle more interrupts now
                    // TODO(PT): Unit test?
                    break;
                }
            }
        }
    }

    pub fn trigger_interrupt(&self, int_type: InterruptType) {
        let bit_index = self.bit_index_for_interrupt_type(int_type);
        *(self.interrupt_flag_register.borrow_mut()) |= (1 << bit_index);
    }

    fn bit_index_for_interrupt_type(&self, int_type: InterruptType) -> u8 {
        match int_type {
            InterruptType::VBlank => 0,
            InterruptType::LCDStat => 1,
            InterruptType::Timer => 2,
            InterruptType::Serial => 3,
            InterruptType::Joypad => 4,
        }
    }

    fn interrupt_type_for_bit_index(&self, bit_index: u8) -> InterruptType {
        match bit_index {
            0 => InterruptType::VBlank,
            1 => InterruptType::LCDStat,
            2 => InterruptType::Timer,
            3 => InterruptType::Serial,
            4 => InterruptType::Joypad,
            _ => panic!("Invalid index"),
        }
    }
}

impl Addressable for InterruptController {
    fn contains(&self, addr: u16) -> bool {
        match addr {
            InterruptController::INTERRUPT_ENABLE_REGISTER_ADDR => true,
            InterruptController::INTERRUPT_FLAG_REGISTER_ADDR => true,
            _ => false,
        }
    }

    fn read(&self, addr: u16) -> u8 {
        match addr {
            InterruptController::INTERRUPT_ENABLE_REGISTER_ADDR => {
                *(self.interrupt_enable_register.borrow())
            }
            InterruptController::INTERRUPT_FLAG_REGISTER_ADDR => {
                *(self.interrupt_flag_register.borrow())
            }
            _ => panic!("Unrecognised address"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            InterruptController::INTERRUPT_ENABLE_REGISTER_ADDR => {
                *(self.interrupt_enable_register.borrow_mut()) = val
            }
            InterruptController::INTERRUPT_FLAG_REGISTER_ADDR => {
                *(self.interrupt_flag_register.borrow_mut()) = val
            }
            _ => panic!("Unrecognised address"),
        }
    }
}

#[cfg(test)]
mod tests {
    use std::{cell::RefCell, rc::Rc};

    use crate::{
        cpu::{CpuState, RegisterName},
        gameboy::GameBoyHardwareProvider,
        interrupts::InterruptController,
        joypad::Joypad,
        mmu::{Mmu, Ram},
        ppu::Ppu,
    };

    use super::InterruptType;

    struct InterruptControllerTestSystem {
        pub mmu: Rc<Mmu>,
        pub cpu: Rc<RefCell<CpuState>>,
        interrupt_controller: Rc<InterruptController>,
    }

    impl InterruptControllerTestSystem {
        pub fn new() -> Self {
            let ram = Rc::new(Ram::new(0, 0xffff));
            let interrupt_controller = Rc::new(InterruptController::new());
            let interrupt_controller_clone = Rc::clone(&interrupt_controller);
            let mmu = Rc::new(Mmu::new(vec![interrupt_controller_clone, ram]));

            let mut cpu = CpuState::new(Rc::clone(&mmu));
            // Set up a stack for the CPU
            cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfffa);
            cpu.enable_debug();

            Self {
                mmu,
                cpu: Rc::new(RefCell::new(cpu)),
                interrupt_controller,
            }
        }
    }

    impl GameBoyHardwareProvider for InterruptControllerTestSystem {
        fn get_mmu(&self) -> Rc<Mmu> {
            Rc::clone(&self.mmu)
        }

        fn get_ppu(&self) -> Rc<Ppu> {
            panic!("PPU not supported in this test harness")
        }

        fn get_cpu(&self) -> Rc<RefCell<CpuState>> {
            Rc::clone(&self.cpu)
        }

        fn get_interrupt_controller(&self) -> Rc<crate::interrupts::InterruptController> {
            Rc::clone(&self.interrupt_controller)
        }

        fn get_joypad(&self) -> Rc<Joypad> {
            panic!("Joypad not supported in this test harness")
        }
    }

    #[test]
    fn test_request_interrupts() {
        let gb = InterruptControllerTestSystem::new();

        let interrupt_to_expected_bit = vec![
            (InterruptType::VBlank, 0),
            (InterruptType::LCDStat, 1),
            (InterruptType::Timer, 2),
            (InterruptType::Serial, 3),
            (InterruptType::Joypad, 4),
        ];
        for (interrupt_type, bit_index) in interrupt_to_expected_bit {
            // When an interrupt is requested
            gb.get_interrupt_controller()
                .trigger_interrupt(interrupt_type);
            // Then the corresponding flag is set in the flags register
            dbg!(gb.get_mmu().read(0xff0f));
            dbg!(gb.get_mmu().read(0xff0f) & (1 << bit_index));
            assert!(gb.get_mmu().read(0xff0f) & (1 << bit_index) != 0);
        }
    }

    #[test]
    fn test_dispatch_interrupt() {
        let gb = InterruptControllerTestSystem::new();

        // Given the IME flag is enabled
        let int_controller = gb.get_interrupt_controller();
        int_controller.set_interrupts_globally_enabled();

        let mmu = gb.get_mmu();
        // And an interrupt is enabled in the mask register
        mmu.write(0xffff, 0b10000);

        // When the interrupt is requested in the flags register
        mmu.write(0xff0f, 0b10000);

        // And the interrupt controller is stepped
        int_controller.step(&gb);

        // Then the interrupt has been disabled in the flags register
        assert_eq!(mmu.read(0xff0f), 0x0);
        // And the IME flag has been disabled
        assert!(!int_controller.are_interrupts_globally_enabled());
        // And the CPU has jumped to the interrupt vector
        assert_eq!(gb.get_cpu().borrow().get_pc(), 0x60);
    }

    #[test]
    fn test_ignore_interrupt_due_to_disabled_ime() {
        let gb = InterruptControllerTestSystem::new();

        // Given the IME flag is disabled
        let int_controller = gb.get_interrupt_controller();
        int_controller.set_interrupts_globally_disabled();

        let mmu = gb.get_mmu();
        // And an interrupt is enabled in the mask register
        mmu.write(0xffff, 0b10000);

        // When the interrupt is requested in the flags register
        mmu.write(0xff0f, 0b10000);

        // And the interrupt controller is stepped
        int_controller.step(&gb);

        // Then the interrupt flags register is untouched
        assert_eq!(mmu.read(0xff0f), 0b10000);
        // And the IME flag has not been touched
        assert!(!int_controller.are_interrupts_globally_enabled());
        // And the CPU's PC has not been touched
        assert_eq!(gb.get_cpu().borrow().get_pc(), 0x0);
    }

    #[test]
    fn test_unhalt_cpu() {
        /*
        // If the CPU is in HALT-mode and has the IME flag disabled,
        // just un-halt the CPU.
        let cpu_ref = system.get_cpu();
        if !self.are_interrupts_globally_enabled() && cpu_ref.borrow().is_halted {
            cpu_ref.borrow_mut().set_halted(false);
            return;
        }
        */
        let gb = InterruptControllerTestSystem::new();

        // Given the CPU is in HALT-mode
        let cpu = gb.get_cpu();
        cpu.borrow_mut().set_halted(true);

        // Given the IME flag is disabled
        let int_controller = gb.get_interrupt_controller();
        int_controller.set_interrupts_globally_disabled();

        let mmu = gb.get_mmu();
        // And an interrupt is enabled in the mask register
        mmu.write(0xffff, 0b10000);

        // When the interrupt is requested in the flags register
        mmu.write(0xff0f, 0b10000);

        // And the interrupt controller is stepped
        int_controller.step(&gb);

        // Then the interrupt flags register is untouched
        assert_eq!(mmu.read(0xff0f), 0b10000);
        // And the IME flag has not been touched
        assert!(!int_controller.are_interrupts_globally_enabled());
        // And the CPU has been un-halted
        assert!(!gb.get_cpu().borrow().is_halted);
    }
}
