#![no_std]
extern crate alloc;
#[cfg(test)]
extern crate std;

use alloc::borrow::ToOwned;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::string::ToString;
use alloc::format;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

#[derive(Debug)]
pub struct PathNotFoundError {
    path: String,
}

impl PathNotFoundError {
    fn new(path: &str) -> Self {
        PathNotFoundError {
            path: path.to_string(),
        }
    }
}

impl alloc::fmt::Display for PathNotFoundError {
    fn fmt(&self, f: &mut alloc::fmt::Formatter) -> alloc::fmt::Result {
        write!(f, "{}", self.path)
    }
}

#[cfg(test)]
impl std::error::Error for PathNotFoundError {
    fn description(&self) -> &str {
        &self.path
    }
}

#[derive(Serialize, Deserialize, Debug)]
pub struct DirectoryImage {
    pub name: String,
    pub files: BTreeMap<String, Vec<u8>>,
    pub subdirectories: BTreeMap<String, DirectoryImage>,
}

#[derive(Debug)]
pub struct FsEntry<'a> {
    pub path: String,
    pub is_dir: bool,
    pub dir_image: Option<&'a DirectoryImage>,
    pub file_data: Option<&'a Vec<u8>>,
}

pub fn fs_entry_find<'a>(root_dir: &'a DirectoryImage, path: &str) -> Option<FsEntry<'a>> {
    // Start off at the root directory
    let mut dir_iter = root_dir;
    for component in path.split("/") {
        if component.len() == 0 {
            // Infix or trailing slash - skip it
            continue;
        }

        match dir_iter.subdirectories.get(component) {
            // There's no subdirectory with this name, but is there a file?
            None => match dir_iter.files.get(component) {
                // Path doesn't exist
                None => return None,
                // We encountered a file in the traversal
                // TODO(PT): We want to ensure that this is the end of the provided path
                Some(file) => {
                    return Some(FsEntry {
                        path: path.to_owned(),
                        is_dir: false,
                        dir_image: None,
                        file_data: Some(file),
                    })
                }
            },
            // Follow to the next subfolder of the path
            Some(subdir) => dir_iter = subdir,
        }
    }
    // We reached the end of the path traversal and were left with a directory
    Some(FsEntry {
        path: path.to_owned(),
        is_dir: true,
        dir_image: Some(dir_iter),
        file_data: None,
    })
}

#[cfg(test)]
fn get_root_directory_from_image() -> DirectoryImage {
    let current_dir = std::env::current_dir().unwrap();
    let image_path = current_dir
        .ancestors()
        .nth(2)
        .ok_or(std::io::Error::new(
            std::io::ErrorKind::NotFound,
            "Failed to find parent",
        ))
        .unwrap()
        .join("scripts")
        .join("mkinitrd")
        .join("output.img");

    postcard::from_bytes(std::fs::read(image_path).unwrap().as_slice()).unwrap()
}

#[test]
fn test_find_root() {
    // When I request the root directory by its path
    let root = get_root_directory_from_image();
    let path = "/";
    let found_entry = fs_entry_find(&root, path).unwrap();
    // Then the root directory is returned
    assert!(found_entry.is_dir);
    let found_dir = found_entry.dir_image.unwrap();
    assert_eq!(found_dir.name, path);
    assert_eq!(found_dir as *const _, &root as *const _);
}

#[test]
fn test_find_top_level_dir() {
    // Given a top-level directory under the root
    let root = get_root_directory_from_image();
    let usr = root.subdirectories.get("usr").unwrap();

    let entry_name = "usr";
    let path = format!("/{}", entry_name);
    assert_eq!(usr.name, entry_name);

    // When I request a top-level directory under the root
    let found_entry = fs_entry_find(&root, &path).unwrap();

    // Then the correct directory is returned
    assert!(found_entry.is_dir);
    let found_dir = found_entry.dir_image.unwrap();
    assert_eq!(found_dir.name, entry_name);
    assert_eq!(found_dir as *const _, usr as *const _);
}

#[test]
fn test_find_top_level_dir_with_trailing_slash() {
    // Given a top-level directory under the root
    let root = get_root_directory_from_image();
    let usr = root.subdirectories.get("usr").unwrap();

    let entry_name = "usr";
    let path = format!("/{}/", entry_name);
    assert_eq!(usr.name, entry_name);

    // When I request a top-level directory under the root
    // And the path I request has a trailing slash
    let found_entry = fs_entry_find(&root, &path).unwrap();

    // Then the correct directory is returned
    assert!(found_entry.is_dir);
    let found_dir = found_entry.dir_image.unwrap();
    assert_eq!(found_dir.name, entry_name);
    assert_eq!(found_dir as *const _, usr as *const _);
}

#[test]
fn test_find_second_level_dir() {
    // Given a directory with a couple levels of nesting
    let root = get_root_directory_from_image();
    let usr = root.subdirectories.get("usr").unwrap();
    let include = usr.subdirectories.get("include").unwrap();

    let entry_name = "include";
    let path = format!("/usr/{}", entry_name);
    assert_eq!(include.name, entry_name);

    // When I try to find the path
    let found_entry = fs_entry_find(&root, &path).unwrap();

    // Then the correct directory is returned
    assert!(found_entry.is_dir);
    let found_dir = found_entry.dir_image.unwrap();
    assert_eq!(found_dir.name, entry_name);
    assert_eq!(found_dir as *const _, include as *const _);
}

/*
fn get_subdir_by_name<'a>(
    dir: &'a DirectoryImage,
    subdir_name: &str,
) -> Result<&'a DirectoryImage, PathNotFoundError> {
    dir.subdirectories
        .get(subdir_name)
        .ok_or(PathNotFoundError::new(subdir_name))
}
    let root = get_root_directory_from_image();
    let mouse = get_subdir_by_name(&root, "usr").and_then(|x| {
        get_subdir_by_name(x, "include").and_then(|x| {
            get_subdir_by_name(x, "drivers").and_then(|x| get_subdir_by_name(x, "mouse"))
        })
    });
    let root = get_root_directory_from_image();
    let get_target_dir = || -> Result<&DirectoryImage, PathNotFoundError> {
        let usr = get_subdir_by_name(&root, "usr")?;
        let include = get_subdir_by_name(&usr, "include")?;
        let drivers = get_subdir_by_name(&include, "drivers")?;
        Ok(get_subdir_by_name(drivers, "mouse")?)
    };
    let target_dir = get_target_dir().unwrap();
*/

#[test]
fn test_find_directory_deeply_nested() {
    // Given a deeply nested directory
    let root = get_root_directory_from_image();
    let usr = root.subdirectories.get("usr").unwrap();
    let include = usr.subdirectories.get("include").unwrap();
    let drivers = include.subdirectories.get("drivers").unwrap();
    let mouse = drivers.subdirectories.get("mouse").unwrap();

    let entry_name = "mouse";
    let path = format!("/usr/include/drivers/{}", entry_name);
    assert_eq!(mouse.name, entry_name);

    // When I try to find the path
    let found_entry = fs_entry_find(&root, &path).unwrap();

    // Then the correct directory is returned
    assert!(found_entry.is_dir);
    let found_dir = found_entry.dir_image.unwrap();
    assert_eq!(
        found_dir.name, entry_name,
        "Expected to find entry named \"{}\"",
        entry_name
    );
    assert_eq!(found_dir as *const _, mouse as *const _);
}

#[test]
fn test_find_directory_extra_forward_slashes() {
    // Given a deeply nested directory
    let root = get_root_directory_from_image();
    let usr = root.subdirectories.get("usr").unwrap();
    let include = usr.subdirectories.get("include").unwrap();
    let drivers = include.subdirectories.get("drivers").unwrap();
    let mouse = drivers.subdirectories.get("mouse").unwrap();

    let entry_name = "mouse";
    assert_eq!(mouse.name, entry_name);

    // When I try to find the path
    // And the path I'm using contains erroneous forward slashes
    let found_entry = fs_entry_find(&root, "///usr/include////drivers//mouse/////").unwrap();

    // Then the correct directory is returned
    assert!(found_entry.is_dir);
    let found_dir = found_entry.dir_image.unwrap();
    assert_eq!(
        found_dir.name, entry_name,
        "Expected to find entry named \"{}\"",
        entry_name
    );
    assert_eq!(found_dir as *const _, mouse as *const _);
}

#[test]
fn test_find_file() {
    // Given a file in the FS
    let root = get_root_directory_from_image();
    let usr = root.subdirectories.get("usr").unwrap();
    let applications = usr.subdirectories.get("applications").unwrap();
    let initrd_fs = applications.files.get("initrd_fs").unwrap();

    let entry_name = "initrd_fs";
    let path = format!("/usr/applications/{}", entry_name);

    // When I try to find the file by its path
    let found_entry = fs_entry_find(&root, &path).unwrap();

    // Then the correct directory is returned
    // Then the file is returned
    assert!(!found_entry.is_dir);
    let found_file_data = found_entry.file_data.unwrap();
    assert_eq!(found_file_data, initrd_fs);
}

#[test]
fn test_find_file_with_extension() {
    // Given a file in the FS
    // And the file has an extension
    let root = get_root_directory_from_image();
    let usr = root.subdirectories.get("usr").unwrap();
    let include = usr.subdirectories.get("include").unwrap();
    let sys = include.subdirectories.get("sys").unwrap();
    let dirent_h = sys.files.get("dirent.h").unwrap();

    let entry_name = "dirent.h";

    // When I try to find the file by its path
    let found_entry = fs_entry_find(&root, "/usr/include/sys/dirent.h").unwrap();

    // Then the correct directory is returned
    // Then the file is returned
    assert!(!found_entry.is_dir);
    let found_file_data = found_entry.file_data.unwrap();
    assert_eq!(found_file_data, dirent_h);
}

//#[test]
fn test_traverse_past_file() {
    // Should not match abc.txt
    "/usr/include/abc.txt/more";
    todo!();
}
