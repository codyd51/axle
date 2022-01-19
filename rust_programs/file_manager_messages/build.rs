use std::collections::BTreeMap;
use std::io::Write;
use std::{env, error, fs, io, path};

fn main() {
    let current_dir = env::current_dir().unwrap();
    let c_message_definitions_rel_path = path::PathBuf::from("src/file_server_messages.h");
    let c_message_definitions_abs_path = current_dir.join(&c_message_definitions_rel_path);

    println!(
        "cargo:rerun-if-changed={}",
        c_message_definitions_abs_path.to_str().unwrap()
    );

    let sysroot = current_dir
        .ancestors()
        .nth(2)
        .ok_or(io::Error::new(
            io::ErrorKind::NotFound,
            "Failed to find parent",
        ))
        .unwrap()
        .join("axle-sysroot");
    let mut destination = sysroot.clone();
    destination.extend(&["usr", "include", "file_server", "file_server_messages.h"]);
    fs::create_dir_all(&destination.parent().unwrap()).unwrap();
    fs::copy(&c_message_definitions_abs_path, destination).unwrap();
}
