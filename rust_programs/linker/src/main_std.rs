use std::{env, error, fs, io};

use crate::packer::pack_elf;

pub fn main() -> Result<(), Box<dyn error::Error>> {
    println!("Running with std");

    let current_dir = env::current_dir().unwrap();
    let sysroot = current_dir
        .ancestors()
        .nth(2)
        .ok_or(io::Error::new(
            io::ErrorKind::NotFound,
            "Failed to find parent",
        ))?
        .join("axle-sysroot");
    println!("Sysroot: {:?}", sysroot);
    let output_file = sysroot.join("usr").join("applications").join("output_elf");

    let elf = pack_elf();
    println!("Got elf of len {}\n", elf.len());

    fs::write(output_file, elf).unwrap();

    Ok(())
}
