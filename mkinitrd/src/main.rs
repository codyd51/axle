use std::collections::HashMap;
use std::path::PathBuf;
use std::{env, error, fs, io, path};

use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug)]
struct DirectoryImage {
    files: HashMap<PathBuf, serde_bytes::ByteBuf>,
    subdirectories: HashMap<PathBuf, Box<DirectoryImage>>,
}

fn traverse<P>(dir: P) -> io::Result<Box<DirectoryImage>>
where
    P: AsRef<path::Path> + std::fmt::Debug,
{
    // Add this file contents to the tree of the parent directory
    let mut dir_contents = Box::new(DirectoryImage {
        files: HashMap::new(),
        subdirectories: HashMap::new(),
    });

    for entry in fs::read_dir(dir)?.filter_map(Result::ok) {
        if let Ok(attributes) = entry.metadata() {
            if attributes.is_dir() {
                if let Ok(subdir_image) = traverse(entry.path()) {
                    dir_contents
                        .subdirectories
                        .insert(entry.path(), subdir_image);
                }
            } else if let Ok(file_data) = fs::read(entry.path()) {
                if entry.path().ends_with(".DS_Store") {
                    continue;
                }
                dir_contents
                    .files
                    .insert(entry.path(), serde_bytes::ByteBuf::from(file_data));
            } else {
                eprintln!("Failed to read contents of file {:?}", entry);
                continue;
            }
        } else {
            eprintln!("Failed to read metadata for file: {:?}", entry);
            continue;
        }
    }
    Ok(dir_contents)
}

fn main() -> Result<(), Box<dyn error::Error>> {
    let _current_dir = env::current_dir().unwrap();
    let fs_image = traverse("/Users/philliptennen/Documents/develop/axle.nosync/axle-sysroot")
        .expect("traverse failed");
    println!("Finished generating directory image");

    let file = std::io::BufWriter::new(std::fs::File::create("./output.img").unwrap());
    bincode::serialize_into(file, &fs_image).expect("Failed to write");

    Ok(())
}
