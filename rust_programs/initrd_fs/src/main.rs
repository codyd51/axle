#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate libc;
use serde::Serialize;

use axle_rt::amc_message_send;
use axle_rt::amc_register_service;
use axle_rt::printf;
use axle_rt::AmcMessage;
use axle_rt::{amc_message_await, ContainsEventField, ExpectsEventField};

use file_manager_messages::str_from_u8_nul_utf8_unchecked;
use file_manager_messages::FileManagerDirectoryContents;
use file_manager_messages::FileManagerDirectoryEntry;
use file_manager_messages::FileManagerReadDirectory;

use libfs::{fs_entry_find, DirectoryImage};

trait FromDirectoryImage {
    fn from_dir_image(dir: &DirectoryImage) -> Self;
}

impl FromDirectoryImage for FileManagerDirectoryContents {
    fn from_dir_image(dir: &DirectoryImage) -> Self {
        let mut contents = FileManagerDirectoryContents {
            event: FileManagerReadDirectory::EXPECTED_EVENT,
            entries: [None; 64],
        };
        let files = &dir.files;
        printf!("Found {:?} files\n", files.len());
        let mut count = 0;
        for entry in files
            .into_iter()
            .map(|kv| FileManagerDirectoryEntry::new(&kv.0, false))
        {
            printf!("Transformed dir entry: {:?}\n", entry);
            contents.entries[count] = Some(entry.clone());
            count += 1;
        }
        let subdirectories = &dir.subdirectories;
        for entry in subdirectories
            .into_iter()
            .map(|kv| FileManagerDirectoryEntry::new(&kv.0, true))
        {
            printf!("Transformed dir entry: {:?}\n", entry);
            contents.entries[count] = Some(entry.clone());
            count += 1;
        }

        contents
    }
}

#[repr(C)]
#[derive(Debug)]
struct AmcInitrdRequest {
    event: u32,
}

impl AmcInitrdRequest {
    fn new() -> Self {
        AmcInitrdRequest {
            event: Self::EXPECTED_EVENT,
        }
    }
}

impl ExpectsEventField for AmcInitrdRequest {
    const EXPECTED_EVENT: u32 = 203;
}

impl ContainsEventField for AmcInitrdRequest {
    fn event(&self) -> u32 {
        self.event
    }
}

// Defineed in core_commands.h
// TODO(PT): Define this in libc
#[repr(C)]
#[derive(Debug)]
struct AmcInitrdInfo {
    event: u32,
    // actually uptr
    initrd_start: u64,
    initrd_end: u64,
    initrd_size: u64,
}

impl ExpectsEventField for AmcInitrdInfo {
    const EXPECTED_EVENT: u32 = AmcInitrdRequest::EXPECTED_EVENT;
}

impl ContainsEventField for AmcInitrdInfo {
    fn event(&self) -> u32 {
        self.event
    }
}

fn traverse_dir(depth: usize, dir: &DirectoryImage) {
    let tabs = "\t".repeat(depth);
    printf!("{}{}\n", tabs, dir.name);
    for file in &dir.files {
        printf!("\t{}{}\n", tabs, file.0);
    }
    for subdir in &dir.subdirectories {
        traverse_dir(depth + 1, subdir.1);
    }
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service("com.axle.file_manager2");

    amc_message_send("com.axle.core", AmcInitrdRequest::new());

    let initrd_info_msg: AmcMessage<AmcInitrdInfo> = amc_message_await(Some("com.axle.core"));
    let initrd_info = initrd_info_msg.body();

    printf!(
        "Event: {}, initrd2 {:#16x} - {:#16x} ({:16x} bytes)\n",
        initrd_info.event,
        initrd_info.initrd_start,
        initrd_info.initrd_end,
        initrd_info.initrd_size
    );

    let rust_reference: &[u8] = unsafe {
        core::slice::from_raw_parts(
            initrd_info.initrd_start as *const u8,
            initrd_info.initrd_size as usize,
        )
    };
    let root_dir: DirectoryImage = postcard::from_bytes(rust_reference).expect("Dealloc failed");
    //traverse_dir(0, &root_dir);

    loop {
        printf!("Awaiting next message...\n");
        let msg: AmcMessage<FileManagerReadDirectory> = amc_message_await(None);

        printf!("Received msg from {:?}: {:?}\n", msg.source(), msg);
        let requested_dir = str_from_u8_nul_utf8_unchecked(&msg.body().dir);
        printf!("Dir: {:?}\n", requested_dir);

        // Find the directory within
        if let Some(entry) = fs_entry_find(&root_dir, &requested_dir) {
            printf!("Found FS entry: {}\n", entry.path);
            if entry.is_dir {
                let response =
                    FileManagerDirectoryContents::from_dir_image(&entry.dir_image.unwrap());
                printf!("Sending response...\n");
                amc_message_send(msg.source(), response);
            }
        } else {
            printf!("Failed to find directory {:?}\n", requested_dir);
        }
    }
    0
}
