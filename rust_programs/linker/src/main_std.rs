use std::rc::Rc;
use std::{env, error, fs, io};

use crate::new_try::FileLayout;
use crate::{assembly_packer, new_try::render_elf};

pub fn main() -> Result<(), Box<dyn error::Error>> {
    println!("Running with std");

    let current_dir = env::current_dir().unwrap();
    let sysroot = current_dir
        .ancestors()
        .nth(2)
        .ok_or(io::Error::new(io::ErrorKind::NotFound, "Failed to find parent"))?
        .join("axle-sysroot");
    println!("Sysroot: {:?}", sysroot);
    let output_file = sysroot.join("usr").join("applications").join("output_elf");

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
    println!("Got elf of len {}\n", elf.len());

    fs::write(output_file, elf).unwrap();

    Ok(())
}
