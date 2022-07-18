use alloc::vec::Vec;
#[cfg(feature = "run_in_axle")]
use axle_rt::printf;
use axle_rt::{amc_message_send, amc_register_service, ContainsEventField, ExpectsEventField};
use cstr_core::CString;

use crate::{assembly_packer, new_try::render_elf, packer::pack_elf};
use axle_rt_derive::ContainsEventField;

// TODO(PT): Copied from initrd_fs, move to axle_rt?
// Defined in core_commands.h
#[repr(C)]
#[derive(Debug, ContainsEventField)]
struct AmcExecBuffer {
    event: u32,
    program_name: *const u8,
    buffer_addr: *const u8,
    buffer_size: u32,
}

impl AmcExecBuffer {
    fn from(program_name: &str, buf: &Vec<u8>) -> Self {
        let buffer_addr = buf.as_ptr();
        // TODO(PT): Change the C API to accept a char array instead of char pointer
        let c_str = CString::new(program_name).unwrap();
        let program_name_ptr = c_str.as_ptr() as *const u8;
        AmcExecBuffer {
            event: Self::EXPECTED_EVENT,
            program_name: program_name_ptr,
            buffer_addr,
            buffer_size: buf.len() as _,
        }
    }
}

impl ExpectsEventField for AmcExecBuffer {
    const EXPECTED_EVENT: u32 = 204;
}

pub fn main() {
    printf!("Running without std!\n");
    amc_register_service("com.axle.linker");

    let layout = Rc::new(FileLayout::new(0x400000));
    let source = "
    .global _start

.section .text

_start:
	mov $0xc, %rax		# _write syscall vector
	mov $0x1, %rbx		# File descriptor (ignored in axle, typically stdout)
	mov $msg, %rcx		# Buffer to write
	mov $msg_len, %rdx	# Length to write
	int $0x80			# invoke syscall

	mov %rax, %rbx		# _exit status code is the write() retval (# bytes written)
	mov $0xd, %rax		# _exit syscall vector
	int $0x80			# invoke syscall

.section .rodata

msg:
    .ascii \"Hello world!\n\"

.equ msg_len, . - msg

    ";
    let (data_packer, instruction_packer) = assembly_packer::parse(&layout, &source);
    let elf = render_elf(&layout, &data_packer, &instruction_packer);

    amc_message_send("com.axle.core", AmcExecBuffer::from("com.axle.runtime_generated", &elf));
}
