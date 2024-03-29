#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate libc;

use alloc::str;

use axle_rt::printf;
use axle_rt::AmcMessage;
use axle_rt::{amc_message_await, ContainsEventField, ExpectsEventField};
use axle_rt::{amc_message_await_untyped, amc_message_send};
use axle_rt::{amc_register_service, core_commands::AmcExecBuffer};
use axle_rt_derive::ContainsEventField;

use cstr_core::CString;
use file_manager_messages::{
    str_from_u8_nul_utf8_unchecked, CheckFileExists, LaunchProgram, ReadFilePart,
    ReadFilePartResponse,
};
use file_manager_messages::{CheckFileExistsResponse, ReadDirectory};
use file_manager_messages::{DirectoryContents, ReadFile, ReadFileResponse};
use file_manager_messages::{DirectoryEntry, FILE_SERVER_SERVICE_NAME};

use libfs::{fs_entry_find, DirectoryImage, FsEntry};

trait FromDirectoryImage {
    fn from_dir_image(dir: &DirectoryImage) -> Self;
}

impl FromDirectoryImage for DirectoryContents {
    fn from_dir_image(dir: &DirectoryImage) -> Self {
        let mut contents = DirectoryContents {
            event: ReadDirectory::EXPECTED_EVENT,
            entries: [None; 128],
        };
        let files = &dir.files;
        printf!("Found {:?} files\n", files.len());
        let mut count = 0;

        let subdirectories = &dir.subdirectories;
        for entry in subdirectories
            .into_iter()
            .map(|kv| DirectoryEntry::new(&kv.0, true))
        {
            //printf!("Transformed dir entry: {:?}\n", entry);
            contents.entries[count] = Some(entry.clone());
            count += 1;
        }

        for entry in files
            .into_iter()
            .map(|kv| DirectoryEntry::new(&kv.0, false))
        {
            //printf!("Transformed dir entry: {:?}\n", entry);
            contents.entries[count] = Some(entry.clone());
            count += 1;
        }

        contents
    }
}

unsafe fn body_as_type_unchecked<T>(body: &[u8]) -> &T {
    &*(body.as_ptr() as *const T)
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
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

// Defineed in core_commands.h
// TODO(PT): Define this in libc
#[repr(C)]
#[derive(Debug, ContainsEventField)]
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

/*
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
*/

fn read_directory(root_dir: &DirectoryImage, sender: &str, request: &ReadDirectory) {
    let requested_dir = str_from_u8_nul_utf8_unchecked(&request.dir);
    printf!("Dir: {:?}\n", requested_dir);

    // Find the directory within
    if let Some(entry) = fs_entry_find(&root_dir, &requested_dir) {
        printf!("Found FS entry: {}\n", entry.path);
        if entry.is_dir {
            let response = DirectoryContents::from_dir_image(&entry.dir_image.unwrap());
            printf!("Sending response...\n");
            amc_message_send(sender, response);
        }
    } else {
        printf!("Failed to find directory {:?}\n", requested_dir);
    }
}

fn launch_program_by_path(root_dir: &DirectoryImage, path: &str) {
    if let Some(entry) = fs_entry_find(&root_dir, &path) {
        if entry.is_dir {
            printf!("Can't launch directories\n");
        } else {
            // TODO(PT): Verify that this is indeed a file?
            // TODO(PT): Replace the Vec<u8> with a structured entry, describing if executable
            let file_name = entry.path.split("/").last().unwrap();
            let c_str = CString::new(file_name).unwrap();
            // TODO(PT): Change the C API to accept a char array instead of char pointer
            let program_name_ptr = c_str.as_ptr() as *const u8;
            amc_message_send(
                "com.axle.core",
                AmcExecBuffer::from(file_name, entry.file_data.as_ref().unwrap(), false),
            );
        }
    } else {
        printf!("Couldn't find path {}\n", path);
    }
}

fn launch_program(root_dir: &DirectoryImage, _sender: &str, request: &LaunchProgram) {
    let requested_path = str_from_u8_nul_utf8_unchecked(&request.path);
    if requested_path == "/magic/exploit" {
        launch_program_by_path(root_dir, "/usr/applications/memory_walker");
        launch_program_by_path(root_dir, "/usr/applications/memory_scan_viewer");
    } else {
        launch_program_by_path(root_dir, requested_path)
    }
}

fn launch_startup_programs(root_dir: &DirectoryImage) {
    let run_on_startup_path = "/config/run_on_startup.txt";
    let run_on_startup_config =
        fs_entry_find(root_dir, run_on_startup_path).expect("{run_on_startup_path} is missing!");
    let run_on_startup_contents = match str::from_utf8(run_on_startup_config.file_data.unwrap()) {
        Ok(v) => v,
        Err(e) => panic!("Failed to read {run_on_startup_path}, invalid UTF-8: {e}"),
    };

    for line in run_on_startup_contents.split("\n") {
        if line.starts_with("#") {
            // Skip comments
            continue;
        }
        launch_program_by_path(root_dir, line);
    }
}

fn read_file(root_dir: &DirectoryImage, sender: &str, request: &ReadFile) {
    let requested_path = str_from_u8_nul_utf8_unchecked(&request.path);
    printf!("Reading {} for {}\n", requested_path, sender);
    if let Some(entry) = fs_entry_find(&root_dir, &requested_path) {
        if entry.is_dir {
            printf!("Can't read directories\n");
        } else {
            /*
            amc_message_send(
                sender,
                ReadFileResponse::new(&entry.path, entry.file_data.unwrap()),
            );
            */
            ReadFileResponse::send(sender, &entry.path, entry.file_data.unwrap());
        }
    } else {
        printf!("Couldn't find path {}\n", requested_path);
    }
}

fn read_file_part(root_dir: &DirectoryImage, sender: &str, request: &ReadFilePart) {
    let requested_path = str_from_u8_nul_utf8_unchecked(&request.path);
    //printf!("Reading part of {} for {}\n", requested_path, sender);
    if let Some(entry) = fs_entry_find(&root_dir, &requested_path) {
        if entry.is_dir {
            printf!("Can't read directories\n");
        } else {
            let file_data = entry.file_data.unwrap();
            let mut end_idx = request.offset + request.len;
            end_idx = core::cmp::min(end_idx, file_data.len());
            ReadFilePartResponse::send(sender, &entry.path, &file_data[request.offset..end_idx]);
        }
    } else {
        printf!("Couldn't find path {}\n", requested_path);
    }
}

fn check_file_exists(root_dir: &DirectoryImage, sender: &str, request: &ReadFile) {
    let requested_path = str_from_u8_nul_utf8_unchecked(&request.path);
    let maybe_fs_entry = fs_entry_find(&root_dir, &requested_path);
    let exists = maybe_fs_entry.is_some();
    let file_size = match maybe_fs_entry {
        None => 0,
        Some(fs_entry) => fs_entry.file_data.unwrap().len(),
    };
    printf!("Checking if file exists: {requested_path}, {exists}, len {file_size}\n");
    CheckFileExistsResponse::send(sender, requested_path, exists, file_size);
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service(FILE_SERVER_SERVICE_NAME);

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
    printf!("Parsing DirectoryImage...\n");
    let root_dir: DirectoryImage = postcard::from_bytes(rust_reference).expect("Dealloc failed");
    printf!("Parsed!\n");
    //traverse_dir(0, &root_dir);

    launch_startup_programs(&root_dir);

    loop {
        //printf!("Awaiting next message...\n");
        // TODO(PT): This pattern is copied from the AwmWindow event loop
        let msg_unparsed: AmcMessage<[u8]> = unsafe { amc_message_await_untyped(None).unwrap() };

        // Parse the first bytes of the message as a u32 event field
        let raw_body = msg_unparsed.body();
        let event = u32::from_ne_bytes(
            // We must slice the array to the exact size of a u32 for the conversion to succeed
            raw_body[..core::mem::size_of::<u32>()]
                .try_into()
                .expect("Failed to get 4-length array from message body"),
        );

        // Each inner call to body_as_type_unchecked is unsafe because we must be
        // sure we're casting to the right type.
        // Since we verify the type on the LHS, each usage is safe.
        //
        // Wrap the whole thing in an unsafe block to reduce
        // boilerplate in each match arm.
        unsafe {
            match event {
                ReadDirectory::EXPECTED_EVENT => read_directory(
                    &root_dir,
                    msg_unparsed.source(),
                    body_as_type_unchecked(raw_body),
                ),
                LaunchProgram::EXPECTED_EVENT => launch_program(
                    &root_dir,
                    msg_unparsed.source(),
                    body_as_type_unchecked(raw_body),
                ),
                ReadFile::EXPECTED_EVENT => read_file(
                    &root_dir,
                    msg_unparsed.source(),
                    body_as_type_unchecked(raw_body),
                ),
                ReadFilePart::EXPECTED_EVENT => read_file_part(
                    &root_dir,
                    msg_unparsed.source(),
                    body_as_type_unchecked(raw_body),
                ),
                CheckFileExists::EXPECTED_EVENT => check_file_exists(
                    &root_dir,
                    msg_unparsed.source(),
                    body_as_type_unchecked(raw_body),
                ),
                _ => printf!("Unknown event: {}\n", event),
            }
        }
    }
    0
}
