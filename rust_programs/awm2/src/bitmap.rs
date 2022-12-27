use crate::println;
use agx_definitions::{Color, LikeLayerSlice, Point, Size};
use alloc::boxed::Box;
use alloc::vec::Vec;
use axle_rt::{amc_message_await__u32_event, amc_message_send, AmcMessage};
use core::ptr;
use file_manager_messages::{ReadFile, ReadFileResponse, FILE_SERVER_SERVICE_NAME};

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct BmpHeader {
    signature: u16,
    size: u32,
    reserved: u32,
    data_off: u32,
}

impl BmpHeader {
    // 'BM' with flipped endianness
    pub const MAGIC: u16 = 0x4d42;
}

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
struct BmpInfoHeader {
    size: u32,
    width: u32,
    height: u32,
    planes: u16,
    bit_count: u16,
    compression: u32,
    compressed_size: u32,
    x_px_per_meter: u32,
    y_px_per_meter: u32,
    colors_used: u32,
    colors_important: u32,
}

#[derive(Debug, Clone)]
pub struct BitmapImage {
    size: Size,
    bits_per_pixel: usize,
    pixel_data: Vec<u8>,
}

impl BitmapImage {
    fn new(size: Size, bits_per_pixel: usize, pixel_data: Vec<u8>) -> Self {
        Self {
            size,
            bits_per_pixel,
            pixel_data,
        }
    }

    pub fn read_bmp_from_path(path: &str) -> Self {
        let file_read_request = ReadFile::new(path);
        amc_message_send(FILE_SERVER_SERVICE_NAME, file_read_request);
        let file_data_msg: AmcMessage<ReadFileResponse> =
            amc_message_await__u32_event(FILE_SERVER_SERVICE_NAME);
        let file_data = file_data_msg.body();

        let bmp_buf = unsafe {
            let bmp_data_slice =
                ptr::slice_from_raw_parts((&file_data.data) as *const u8, file_data.len);
            let bmp_data: &[u8] = &*(bmp_data_slice as *const [u8]);
            bmp_data.to_vec()
        };
        let bmp_base_ptr = bmp_buf.as_ptr();
        let bmp_header: BmpHeader = unsafe { core::ptr::read(bmp_base_ptr as *const _) };
        let bmp_signature = bmp_header.signature;
        assert_eq!(bmp_signature, BmpHeader::MAGIC);
        let bmp_info_header: BmpInfoHeader = unsafe {
            ptr::read(bmp_base_ptr.offset(core::mem::size_of::<BmpHeader>() as isize) as *const _)
        };
        let bmp_pixel_data = unsafe {
            let px_data_slice_base = bmp_base_ptr.offset(bmp_header.data_off as _);
            let px_data_slice = ptr::slice_from_raw_parts(px_data_slice_base, bmp_header.size as _);
            let px_data: &[u8] = &*(px_data_slice as *const [u8]);
            px_data.iter().map(|&e| e).collect()
        };
        Self::new(
            Size::new(
                bmp_info_header.width as isize,
                bmp_info_header.height as isize,
            ),
            bmp_info_header.bit_count as usize,
            bmp_pixel_data,
        )
    }

    pub fn render(&self, onto: &Box<dyn LikeLayerSlice>) {
        let mut scale_x = 1.0;
        let mut scale_y = 1.0;
        let onto_frame = onto.frame();
        if onto_frame.width() != self.size.width || onto_frame.height() != self.size.height {
            scale_x = self.size.width as f64 / onto_frame.size.width as f64;
            scale_y = self.size.height as f64 / onto_frame.size.height as f64;
        }
        let bits_per_pixel = self.bits_per_pixel;
        let bytes_per_pixel = bits_per_pixel / 8;
        for draw_row in 0..onto_frame.height() {
            let bmp_y = (self.size.height - 1) - ((draw_row as f64 * scale_y) as isize);
            for draw_col in 0..onto_frame.width() {
                let bmp_x = (draw_col as f64 * scale_x) as isize;
                let bmp_off = core::cmp::min(
                    ((bmp_y * self.size.width * bytes_per_pixel as isize)
                        + (bmp_x * bytes_per_pixel as isize)) as usize,
                    self.pixel_data.len() - bytes_per_pixel,
                );
                let b = self.pixel_data[bmp_off + 0];
                let g = self.pixel_data[bmp_off + 1];
                let r = self.pixel_data[bmp_off + 2];
                onto.putpixel(Point::new(draw_col, draw_row), Color::new(r, g, b));
                // Read as a u32 so we can get the whole pixel in one memory access
                //uint32_t pixel = *((uint32_t*)(&image->pixel_data[bmp_off]));
            }
        }
    }
}
