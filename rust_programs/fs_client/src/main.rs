#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate libc;

use axle_rt::amc_message_await;
use axle_rt::amc_message_send;
use axle_rt::amc_register_service;
use axle_rt::printf;
use axle_rt::AmcMessage;

use file_manager_messages::FileManagerReadDirectory;
use file_manager_messages::{str_from_u8_nul_utf8_unchecked, FileManagerDirectoryContents};

use awm_messages::{AwmCreateWindow, AwmCreateWindowResponse};
fn print_tree(dir_name: &str) {
    let fs_server = "com.axle.file_manager2";
    amc_message_send(fs_server, FileManagerReadDirectory::new(dir_name));

    let dir_contents: AmcMessage<FileManagerDirectoryContents> = amc_message_await(Some(fs_server));

    for entry in dir_contents
        .body()
        .entries
        .iter()
        .filter_map(|e| e.as_ref())
    {
        let entry_name = str_from_u8_nul_utf8_unchecked(&entry.name);
        printf!(
            "{:?}: {:?} (dir? {:?})\n",
            dir_name,
            entry_name,
            entry.is_directory
        );
    }
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service("com.axle.fs2_client");

    printf!("Sending message to file_manager2...\n");

    //print_tree(&dir_contents.body());
    print_tree("/");
    print_tree("usr");
    print_tree("usr/lib");
    print_tree("/usr///applications/");

    loop {}
    0
}
