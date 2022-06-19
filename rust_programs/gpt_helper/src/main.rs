#![feature(int_log)]
#![feature(exclusive_range_pattern)]
#![cfg_attr(not(feature = "use_std"), no_std)]
extern crate alloc;
#[cfg(test)]
extern crate std;
use core::mem::size_of;
#[cfg(test)]
use std::format;
use std::{
    fs::File,
    io::{self, BufReader, Read, Seek, SeekFrom},
    println,
};

use bitvec::prelude::*;

use alloc::borrow::ToOwned;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use binread::BinRead;
use binread::BinReaderExt;
use binread::{io::Cursor, NullString};

const _SECTOR_SIZE: usize = 512;

fn seek_read(mut reader: impl Read + Seek, offset: usize, buf: &mut [u8]) -> io::Result<()> {
    reader.seek(SeekFrom::Start(offset.try_into().unwrap()))?;
    reader.read_exact(buf)?;
    Ok(())
}

#[derive(Debug)]
pub struct MasterBootRecordPartitionRecordOld<'a>(
    //&'a BitSlice<[u8; MasterBootRecordPartitionRecord::SIZE_IN_BYTES], Lsb0>,
    &'a BitSlice<u8>,
);

impl<'a> MasterBootRecordPartitionRecordOld<'a> {
    const SIZE_IN_BYTES: usize = 16;
    const SIZE_IN_BITS: usize = Self::SIZE_IN_BYTES * 8;

    fn new(data: &'a BitSlice<u8>) -> Self {
        Self { 0: data }
    }

    fn os_type(&self) -> u8 {
        // Byte 4
        self.0[(4 * 8)..((4 + 1) * 8)].load::<u8>()
    }
}

#[derive(Debug)]
pub struct ProtectiveMasterBootRecordOld(
    BitArray<[u8; ProtectiveMasterBootRecordOld::SIZE_IN_BYTES], Lsb0>,
);

impl ProtectiveMasterBootRecordOld {
    const SIZE_IN_BYTES: usize = 512;
    const SIZE_IN_BITS: usize = Self::SIZE_IN_BYTES * 8;

    fn new(data: [u8; Self::SIZE_IN_BYTES]) -> Self {
        Self {
            0: BitArray::new(data),
        }
    }

    fn partition_records(&self) -> Vec<MasterBootRecordPartitionRecordOld> {
        let mut out = Vec::new();
        for i in 0..4 {
            let offset_in_bytes = 446 + (MasterBootRecordPartitionRecordOld::SIZE_IN_BYTES * i);
            out.push(MasterBootRecordPartitionRecordOld::new(
                &self.0[offset_in_bytes * 8
                    ..(offset_in_bytes + MasterBootRecordPartitionRecordOld::SIZE_IN_BYTES) * 8],
            ));
        }
        out
    }

    fn signature(&self) -> u16 {
        // Last 2 bytes
        self.0[Self::SIZE_IN_BITS - (8 * size_of::<u16>())..].load::<u16>()
    }
}

#[derive(Debug)]
#[repr(C)]
pub struct MasterBootRecordPartitionRecord {
    // UEFI Spec v2.9, §Table 5-2: Legacy MBR Partition Record
    boot_indicator: u8,
    start_head: u8,
    start_sector: u8,
    start_track: u8,
    os_type: u8,
    end_head: u8,
    end_sector: u8,
    end_track: u8,
    start_lba: u32,
    size_in_lba: u32,
}

impl<'a> MasterBootRecordPartitionRecord {
    const SIZE: usize = 16;

    fn from(data: &'a [u8]) -> Self {
        unsafe { core::ptr::read(data.as_ptr() as *const _) }
    }
}

#[derive(Debug)]
pub struct ProtectiveMasterBootRecord([u8; Self::SIZE]);

impl ProtectiveMasterBootRecord {
    const SIZE: usize = 512;

    fn new(data: [u8; Self::SIZE]) -> Self {
        Self { 0: data }
    }

    fn partition_records(&self) -> Vec<MasterBootRecordPartitionRecord> {
        let mut out = Vec::new();
        for i in 0..4 {
            let offset = 446 + (MasterBootRecordPartitionRecord::SIZE * i);
            out.push(MasterBootRecordPartitionRecord::from(
                &self.0[offset..offset + MasterBootRecordPartitionRecord::SIZE],
            ));
        }
        out
    }

    fn signature(&self) -> u16 {
        // Last 2 bytes
        self.0[510..512].view_bits::<Lsb0>().load::<u16>()
    }
}

#[derive(Debug)]
#[repr(C)]
pub struct GptHeader {
    // UEFI Spec v2.9, §Table 5-5
    signature: [u8; 8],
    revision: u32,
    header_size: u32,
    header_crc32: u32,
    reserved: u32,
    my_lba: u64,
    alternate_lba: u64,
    first_usable_lba: u64,
    last_usable_lba: u64,
    disk_guid: [u8; 16],
    partition_entry_lba: u64,
    partition_entry_count: u32,
    partition_entry_sizeof: u32,
    partition_entry_array_crc32: u32,
    // 92 is the size of the above fields
    reserved2: [u8; _SECTOR_SIZE - 92],
}

impl<'a> GptHeader {
    fn from(data: &'a [u8]) -> Self {
        unsafe { core::ptr::read(data.as_ptr() as *const _) }
    }
}

#[derive(Debug)]
#[repr(C)]
pub struct GptPartitionEntry {
    // UEFI Spec v2.9, §Table 5-6
    partition_type_guid: [u8; 16],
    unique_partition_guid: [u8; 16],
    starting_lba: u64,
    ending_lba: u64,
    attributes: u64,
    partition_name: [u8; 72],
}

impl<'a> GptPartitionEntry {
    fn from(data: &'a [u8]) -> Self {
        unsafe { core::ptr::read(data.as_ptr() as *const _) }
    }
}

fn main() -> io::Result<()> {
    let mut file =
        File::open("/Users/philliptennen/Documents/develop/axle.nosync/ubuntu_image_raw.iso")
            .expect("Unable to open file");

    let mut sector_contents = [0; 512];
    for sector_idx in 0..8 {
        println!("--- Sector {sector_idx} ---");

        let sector_size = 512;
        let chunk_size = 64;
        let base_address = sector_size * sector_idx;
        seek_read(&mut file, base_address, &mut sector_contents).unwrap();
        for (offset, line) in sector_contents.chunks(chunk_size).enumerate() {
            print!("{:08x}: ", base_address + (offset * chunk_size));
            // TODO(PT): Fixup endianness?
            for word in line.chunks(4) {
                for byte in word {
                    print!("{:02x}", *byte);
                }
                print!(" ");
            }
            println!();
        }

        println!("\n\n\n");
    }

    let mut protective_mbr_bytes = [0; 512];
    seek_read(&mut file, 0, &mut protective_mbr_bytes).unwrap();
    let mbr = ProtectiveMasterBootRecord::new(protective_mbr_bytes);
    println!("Protective MBR sig: {:04x}", mbr.signature());
    let partition_records = mbr.partition_records();
    for record in &partition_records {
        //println!("Partition record {i}:");
        println!("Partition record:");
        //println!("\t{record:?}");
        println!("\tOS type: {:02x}", record.os_type);
        println!("\tBoot indicator: {:02x}", record.boot_indicator);
        let start_lba = record.start_lba;
        println!(
            "\tLBA range: {:08x} - {:08x}",
            start_lba,
            start_lba + record.size_in_lba
        );
        println!(
            "\tAddress range: {:016x} - {:016x}",
            start_lba * 512,
            (start_lba as usize + record.size_in_lba as usize) * 512
        );

        if record.os_type == 0xee {
            println!("Found GPT protective partition");
        }
    }

    // UEFI Spec v2.9, §5.2.2 - OS Types
    let gpt_protective_partition_record = partition_records
        .iter()
        .find(|record| record.os_type == 0xee)
        .expect("Failed to find GPT protective partition");
    // GPT protective partition is defined as covering the sector range (1..disk_size_in_sectors - 1)
    // > Set to 0xFFFFFFFF if the size of the disk is too large to be represented
    assert_eq!(gpt_protective_partition_record.start_lba, 1);
    let disk_size_in_sectors = (gpt_protective_partition_record.size_in_lba + 1) as usize;
    let readable_disk_size = {
        let disk_size_in_bytes = disk_size_in_sectors * _SECTOR_SIZE;
        match disk_size_in_bytes.log2() {
            0..10 => format!("{disk_size_in_bytes} bytes"),
            10..20 => format!("{:02} kb", disk_size_in_bytes as f64 / 1024.0),
            20..30 => format!("{:02} mb", disk_size_in_bytes as f64 / 1024.0 / 1024.0),
            _ => format!(
                "{:02} gb",
                disk_size_in_bytes as f64 / 1024.0 / 1024.0 / 1024.0
            ),
        }
    };
    println!("Disk size: {}", disk_size_in_sectors * 512);

    // The primary GPT header will be at LBA 1 (UEFI v2.9, §5.3.1)
    // TODO(PT): Extract this pattern into a FromDiskSeek?
    let mut primary_gpt_header_bytes = [0; 512];
    seek_read(&mut file, 512, &mut primary_gpt_header_bytes).unwrap();
    let primary_gpt_header = GptHeader::from(&primary_gpt_header_bytes);
    println!("GPT header: {:?}", primary_gpt_header);

    let mut alternate_gpt_header_bytes = [0; 512];
    seek_read(
        &mut file,
        (primary_gpt_header.alternate_lba as usize) * _SECTOR_SIZE,
        &mut alternate_gpt_header_bytes,
    )
    .unwrap();
    let alternate_gpt_header = GptHeader::from(&alternate_gpt_header_bytes);

    for gpt_header in [&primary_gpt_header, &alternate_gpt_header] {
        let gpt_header_signature = core::str::from_utf8(&primary_gpt_header.signature)
            .expect("Failed to decode GPT header signature");
        // Must match EFI PART
        assert_eq!(gpt_header_signature, "EFI PART", "Unexpected GPT signature");
        // Must be 0x00010000
        assert_eq!(gpt_header.revision, 0x00010000, "Unexpected GPT revision");

        println!("GPT header at sector #{}:", gpt_header.my_lba);
        println!("\tFirst usable LBA: #{}", gpt_header.first_usable_lba);
        println!("\tLast usable LBA: #{}", gpt_header.last_usable_lba);
        println!("\tDisk GUID: {:?}", gpt_header.disk_guid);
        println!("\tPartitions: #{}", gpt_header.partition_entry_lba);
        println!("\tNum partitions: {}", gpt_header.partition_entry_count);
        println!(
            "\tPartition entry size: {}",
            gpt_header.partition_entry_sizeof
        );

        println!("\tPartitions:");
        for partition_idx in 0..gpt_header.partition_entry_count as usize {
            let partition_entry_address = ((gpt_header.partition_entry_lba as usize)
                * _SECTOR_SIZE)
                + (partition_idx * (gpt_header.partition_entry_sizeof as usize));
            let mut partition_entry_bytes =
                vec![Default::default(); gpt_header.partition_entry_sizeof as usize];
            seek_read(
                &mut file,
                partition_entry_address,
                &mut partition_entry_bytes,
            )
            .unwrap();
            let partition_entry = GptPartitionEntry::from(&partition_entry_bytes);
            if partition_entry.partition_type_guid == [0; 16] {
                // Partition not in use
                continue;
            }
            //println!("\t\t{partition_entry:?}");
            println!("\t\tPartition #{partition_idx}:");
            //28732ac1-1ff8-d211-ba4b-0a0c93ec93b

            println!(
                "\t\t\tType GUID: {:x?}",
                partition_entry.partition_type_guid
            );
            println!(
                "\t\t\tPartition GUID: {:x?}",
                partition_entry.unique_partition_guid
            );
            println!(
                "\t\t\tPartition range: #[{} - {}]",
                partition_entry.starting_lba, partition_entry.ending_lba
            );
            println!(
                "\t\t\tPartition name: {}",
                core::str::from_utf8(&partition_entry.partition_name).unwrap()
            );

            /*
            partition_type_guid: [u8; 16],
            unique_partition_guid: [u8; 16],
            starting_lba: u64,
            ending_lba: u64,
            attributes: u64,
            partition_name: [u8; 72],
            */
        }
    }

    Ok(())
}

#[test]
fn test_find_file_with_extension() {
    #[derive(BinRead)]
    #[br(magic = b"DOG", assert(name.len() != 0))]
    struct Dog {
        bone_pile_count: u8,

        #[br(big, count = bone_pile_count)]
        bone_piles: Vec<u16>,

        #[br(align_before = 0xA)]
        name: NullString,
    }

    let mut reader = Cursor::new(b"DOG\x02\x00\x01\x00\x12\0\0Rudy\0");
    let dog: Dog = reader.read_ne().unwrap();
    assert_eq!(dog.bone_piles, &[0x1, 0x12]);
    assert_eq!(dog.name.into_string(), "Rudy")
}
