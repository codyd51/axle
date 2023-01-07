use crate::utils::{ContainsMachineWords, PhysAddr};
use bitvec::prelude::*;
use core::num::TryFromIntError;
use ffi_bindings::println;

extern "C" {
    fn outb(port: u16, val: u8);
    fn x86_msr_get(msr: u32, lo: *mut u32, hi: *mut u32);
    fn x86_msr_set(msr: u32, lo: u32, hi: u32);
}

pub fn apic_disable_pic() {
    // Ref: https://blog.wesleyac.com/posts/ioapic-interrupts
    // Ref: https://zygomatic.sourceforge.net/devref/group__arch__ia32__apic.html
    unsafe {
        // Select the interrupt mode control register
        outb(0x22, 0x70);
        // This forces NMI and INTR signals to flow through the APIC instead of the PIC
        outb(0x23, 0x01);
    }
}

pub fn apic_enable() {
    let mut lo: u32 = 0;
    let mut hi: u32 = 0;
    let apic_base_msr = 0x1b;
    unsafe {
        x86_msr_get(apic_base_msr, &mut lo as *mut u32, &mut hi as *mut u32);
        println!("    Got APIC MSR {lo:#016x}:{hi:#016x}");
        lo |= 1 << 11;
        println!("Setting APIC MSR {lo:#016x}:{hi:#016x}");
        x86_msr_set(apic_base_msr, lo, hi);
    }
}

#[no_mangle]
pub fn apic_signal_end_of_interrupt(int_no: u8) {
    //println!("apic_signal_end_of_interrupt({int_no})");
    // TODO(PT): We should retrieve this from somewhere
    let local_apic = ProcessorLocalApic::new(PhysAddr(0xfee00000));
    local_apic.send_end_of_interrupt();
}

pub struct ProcessorLocalApic {
    base: PhysAddr,
}

impl ProcessorLocalApic {
    const EOI_REGISTER_IDX: usize = 0xb;
    const SPURIOUS_INTERRUPT_VECTOR_REGISTER_IDX: usize = 0xf;
    const INTERRUPT_COMMAND_LOW_REGISTER_IDX: usize = 0x30;
    const INTERRUPT_COMMAND_HIGH_REGISTER_IDX: usize = 0x31;

    pub fn new(base: PhysAddr) -> Self {
        Self { base }
    }

    fn read_register(&self, register_idx: usize) -> u32 {
        let register_addr = (self.base + (register_idx * 0x10)).to_remapped_high_memory_virt();
        let register: &'static u32 = unsafe { &*(register_addr.0 as *const u32) };
        *register
    }

    fn write_register(&self, register_idx: usize, val: u32) {
        let register_addr = (self.base + (register_idx * 0x10)).to_remapped_high_memory_virt();
        let register: &'static mut u32 = unsafe { &mut *(register_addr.0 as *mut u32) };
        *register = val;
    }

    fn send_end_of_interrupt(&self) {
        // We can write any value here, we just need to trigger a write
        self.write_register(Self::EOI_REGISTER_IDX, 1);
    }

    fn read_spurious_int_vector_register(&self) -> u32 {
        self.read_register(Self::SPURIOUS_INTERRUPT_VECTOR_REGISTER_IDX)
    }

    fn write_spurious_int_vector_register(&self, val: u32) {
        self.write_register(Self::SPURIOUS_INTERRUPT_VECTOR_REGISTER_IDX, val)
    }

    pub fn id(&self) -> u32 {
        self.read_register(2)
    }

    pub fn version(&self) -> u32 {
        self.read_register(3)
    }

    pub fn enable(&self) {
        // Software-disable bit of the local APIC is bit 8 of the spurious interrupt vector register
        let mut spurious_iv_reg_contents = self.read_spurious_int_vector_register();
        println!("Read spurious IV reg contents {spurious_iv_reg_contents:#08x}");
        //spurious_iv_reg_contents |= 1 << 8;
        //spurious_iv_reg_contents |= 1 << 8;
        spurious_iv_reg_contents |= 0xff;
        println!("Writing spurious IV reg contents {spurious_iv_reg_contents:#08x}");
        self.write_spurious_int_vector_register(spurious_iv_reg_contents);
    }

    pub fn send_ipi(&self, ipi: InterProcessorInterruptDescription) {
        // Intel SDM §10.6.1
        // > The act of writing to the low doubleword of the ICR causes the IPI to be sent.
        // Therefore, we need to write the high word first so we know we're ready
        let ipi_as_u64: u64 = ipi.into();
        println!("ipi as u64 {ipi_as_u64:#016x}");
        self.write_register(
            Self::INTERRUPT_COMMAND_HIGH_REGISTER_IDX,
            ipi_as_u64.high_u32(),
        );
        self.write_register(
            Self::INTERRUPT_COMMAND_LOW_REGISTER_IDX,
            ipi_as_u64.low_u32(),
        );
    }
}

/// Ref: 82093AA datasheet
pub struct IoApic {
    base: PhysAddr,
}

impl IoApic {
    const ID_REG_IDX: u32 = 0;
    const VERSION_REG_IDX: u32 = 1;
    pub fn new(base: PhysAddr) -> Self {
        Self { base }
    }

    fn select_register(&self, reg_idx: u32) {
        // IOREGSEL is at offset 0x00
        let register_select_ptr = self.base.to_remapped_high_memory_virt();
        let register_select: &'static mut u32 =
            unsafe { &mut *(register_select_ptr.0 as *mut u32) };
        *register_select = reg_idx;
    }

    fn read_word(&self) -> u32 {
        // IOWIN is at offset 0x10
        let data_window_ptr = (self.base + 0x10).to_remapped_high_memory_virt();
        let data_window: &'static u32 = unsafe { &*(data_window_ptr.0 as *const u32) };
        *data_window
    }

    fn write_word(&self, val: u32) {
        // IOWIN is at offset 0x10
        let data_window_ptr = (self.base + 0x10).to_remapped_high_memory_virt();
        let data_window: &'static mut u32 = unsafe { &mut *(data_window_ptr.0 as *mut u32) };
        *data_window = val
    }

    pub fn read_register(&self, reg_idx: u32) -> u32 {
        self.select_register(reg_idx);
        self.read_word()
    }

    pub fn write_register(&self, reg_idx: u32, val: u32) {
        println!("Writing {val:#016x} to {reg_idx}");
        self.select_register(reg_idx);
        self.write_word(val)
    }

    pub fn id(&self) -> u32 {
        let id_val = self.read_register(Self::ID_REG_IDX);
        let id_bits = BitArray::<u32, Lsb0>::new(id_val);
        id_bits[24..28].load()
    }

    pub fn version(&self) -> u32 {
        let version_val = self.read_register(Self::VERSION_REG_IDX);
        let version_bits = BitArray::<u32, Lsb0>::new(version_val);
        version_bits[..8].load()
    }

    pub fn max_redirection_entry(&self) -> u32 {
        let version_val = self.read_register(Self::VERSION_REG_IDX);
        let version_bits = BitArray::<u32, Lsb0>::new(version_val);
        version_bits[16..24].load()
    }

    pub fn remap_irq(&self, remap: RemapIrqDescription) {
        // TODO(PT): Only override the relevant bits of the registers instead of overwriting
        // whatever was there previously
        // Or, ensure that we're writing entirely sane values instead of relying on whatever was
        // there before!
        let mut remap_as_bits = remap.as_bits();
        let low_reg = 0x10 + (remap.irq_vector as u32 * 2);
        let high_reg = low_reg + 1;
        /*
        println!("Remap as bits {remap_as_bits:#016x}");
        println!("Regs {low_reg}, {high_reg}");

        println!(
            "Reg contents {}, {}",
            self.read_register(low_reg),
            self.read_register(high_reg)
        );
        println!(
            "Writing {} to {low_reg}",
            (remap_as_bits & 0xffffffff) as u32
        );
        println!(
            "Writing {} to {high_reg}",
            (remap_as_bits >> 32 & 0xffffffff) as u32
        );
        */
        self.write_register(low_reg, (remap_as_bits & 0xffffffff) as u32);
        self.write_register(high_reg, (remap_as_bits >> 32 & 0xffffffff) as u32);
    }
}

type RemapIrqDescriptionRaw = BitArr!(for 64, in u32, Lsb0);

#[derive(Debug, Copy, Clone)]
pub struct RemapIrqDescription {
    inner: RemapIrqDescriptionRaw,
    /// The hardware IRQ vector we're remapping.
    /// This will determine which entry in the redirection table we use.
    irq_vector: u8,
}

impl RemapIrqDescription {
    pub fn new(irq_vector: u8, remapped_int_vector: u8, dest_local_apic_id: u32) -> Self {
        let inner = BitArray::new([0, 0]);
        let mut ret = Self { inner, irq_vector };

        ret.set_remapped_int_vector(remapped_int_vector);
        // Set delivery mode (000 is fixed)
        ret.inner[8..11].store(0);
        // Set destination mode (0 means destination APICs are referred to by their APIC IDs)
        ret.inner.set(11, false);
        // Do not mask this interrupt (i.e. enable it)
        ret.inner.set(16, false);
        ret.set_destination_local_apic_id(dest_local_apic_id);

        ret
    }

    /// The internal interrupt vector (index into the IDT) that we're assigning the IRQ to
    fn set_remapped_int_vector(&mut self, remapped_int_vector: u8) {
        self.inner[..8].store(remapped_int_vector)
    }

    /// The local APIC ID to deliver the interrupt to
    fn set_destination_local_apic_id(&mut self, apic_id: u32) {
        self.inner[53..64].store(apic_id);
    }

    pub fn as_bits(&self) -> u64 {
        self.inner.load()
    }
}

#[derive(Debug, Copy, Clone)]
pub enum InterProcessorInterruptDeliveryMode {
    Fixed,
    Init,
    Startup,
}

impl From<InterProcessorInterruptDeliveryMode> for u8 {
    fn from(value: InterProcessorInterruptDeliveryMode) -> Self {
        // Ref: Intel SDM Figure 10-12
        match value {
            InterProcessorInterruptDeliveryMode::Fixed => 0b000,
            InterProcessorInterruptDeliveryMode::Init => 0b101,
            InterProcessorInterruptDeliveryMode::Startup => 0b110,
        }
    }
}

#[derive(Debug, Copy, Clone)]
pub enum InterProcessorInterruptDestination {
    /// Holds an APIC ID
    OtherProcessor(usize),
    ThisProcessor,
    AllProcessorsIncludingThis,
    AllProcessorsExcludingThis,
}

impl From<InterProcessorInterruptDestination> for u8 {
    fn from(value: InterProcessorInterruptDestination) -> Self {
        // Ref: Intel SDM Figure §10.6.1
        match value {
            InterProcessorInterruptDestination::OtherProcessor(_) => 0b00,
            InterProcessorInterruptDestination::ThisProcessor => 0b01,
            InterProcessorInterruptDestination::AllProcessorsIncludingThis => 0b10,
            InterProcessorInterruptDestination::AllProcessorsExcludingThis => 0b11,
        }
    }
}

type InterProcessorInterruptDescriptionRaw = BitArr!(for 64, in u32, Lsb0);

#[derive(Debug, Copy, Clone)]
pub struct InterProcessorInterruptDescription {
    inner: InterProcessorInterruptDescriptionRaw,
}

impl InterProcessorInterruptDescription {
    pub fn new(
        int_vector: u8,
        delivery_mode: InterProcessorInterruptDeliveryMode,
        destination: InterProcessorInterruptDestination,
    ) -> Self {
        let inner = BitArray::new([0, 0]);
        let mut ret = Self { inner };

        ret.set_int_vector(int_vector);
        ret.set_delivery_mode(delivery_mode);
        ret.set_destination(destination);
        // Set destination mode to Physical
        ret.inner.set(11, false);
        // Set level to Assert if this isn't an Init
        let should_assert = !matches!(delivery_mode, InterProcessorInterruptDeliveryMode::Init);
        //let should_assert = true;
        ret.inner.set(14, should_assert);
        // Set trigger mode to edge
        ret.inner.set(15, false);

        ret
    }

    fn set_int_vector(&mut self, int_vector: u8) {
        self.inner[..8].store(int_vector)
    }

    fn set_delivery_mode(&mut self, delivery_mode: InterProcessorInterruptDeliveryMode) {
        self.inner[8..11].store(u8::from(delivery_mode))
    }

    fn set_destination(&mut self, destination: InterProcessorInterruptDestination) {
        // Set the 'destination shorthand' field
        self.inner[18..21].store(u8::from(destination));
        if let InterProcessorInterruptDestination::OtherProcessor(apic_id) = destination {
            // We'll need to set the destination field
            self.inner[56..].store(apic_id)
        }
    }
}

impl From<InterProcessorInterruptDescription> for u64 {
    fn from(value: InterProcessorInterruptDescription) -> Self {
        value.inner.load()
    }
}
