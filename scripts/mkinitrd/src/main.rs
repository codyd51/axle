use std::collections::BTreeMap;
use std::io::Write;
use std::{env, error, fs, io, path};
use libfs::DirectoryImage;

fn path_relative_to(path: &path::Path, pivot: &path::Path) -> String {
    if path == pivot {
        return "/".to_string();
    }
    let rel = pathdiff::diff_paths(path, pivot).unwrap();
    rel.into_os_string().into_string().unwrap()
}

fn filename<P>(path: &P) -> Option<String>
where
    P: AsRef<path::Path>,
{
    Some(path.as_ref().file_name()?.to_str()?.to_owned())
}

fn filename_with_fallback<P>(path: &P) -> String
where
    P: AsRef<path::Path>,
{
    filename(path).unwrap_or("<Unknown>".to_string())
}

fn traverse<P1, P2>(sysroot: &P1, dir: &P2) -> io::Result<DirectoryImage>
where
    P1: AsRef<path::Path> + std::fmt::Debug + std::cmp::PartialEq,
    P2: AsRef<path::Path> + std::fmt::Debug + std::cmp::PartialEq,
{
    // Add this file contents to the tree of the parent directory
    let mut dir_contents = DirectoryImage {
        name: filename_with_fallback(dir),
        files: BTreeMap::new(),
        subdirectories: BTreeMap::new(),
    };

    // The sysroot should get the name "/"
    if dir.as_ref() == sysroot.as_ref() {
        dir_contents.name = "/".to_string();
    }

    for entry in fs::read_dir(dir)?.filter_map(Result::ok) {
        if let Ok(attributes) = entry.metadata() {
            if attributes.is_dir() {
                if let Ok(subdir_image) = traverse(sysroot, &entry.path()) {
                    dir_contents
                        .subdirectories
                        .insert(filename_with_fallback(&entry.path()), subdir_image);
                }
            } else if let Ok(file_data) = fs::read(entry.path()) {
                if entry.path().ends_with(".DS_Store") {
                    continue;
                }
                dir_contents
                    .files
                    .insert(filename_with_fallback(&entry.path()), file_data);
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

fn print_tree(depth: usize, dir: &DirectoryImage) -> () {
    let tabs = std::iter::repeat(" ").take(depth).collect::<String>();
    println!("{}├─ {}", tabs, dir.name);
    for file in &dir.files {
        println!("{}│  ├─ {}", tabs, file.0);
    }
    println!("{}│", tabs);
    for subdir in &dir.subdirectories {
        print_tree(depth + 1, &subdir.1);
    }
}

fn main() -> Result<(), Box<dyn error::Error>> {
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
    let fs_image = traverse(&sysroot, &sysroot).expect("traverse failed");
    println!("Finished generating directory image");

    let mut file = std::io::BufWriter::new(std::fs::File::create("./output.img").unwrap());
    let v = postcard::to_allocvec(&fs_image).expect("Failed to encode");
    file.write(&v).expect("Failed to write to file");

    print_tree(0, &fs_image);

    Ok(())
}
