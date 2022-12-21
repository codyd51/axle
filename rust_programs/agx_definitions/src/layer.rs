extern crate alloc;
use crate::{Color, Point, Rect, Size, CHAR_HEIGHT, CHAR_WIDTH, FONT8X8};
use alloc::vec;
use alloc::{boxed::Box, rc::Rc, vec::Vec};
use core::{cell::RefCell, cmp::min, fmt::Display};

#[cfg(target_os = "axle")]
use axle_rt::println;

#[derive(Debug, Copy, Clone, PartialEq)]
pub enum StrokeThickness {
    Filled,
    // TODO(PT): Support per-side thickness?
    Width(isize),
}

/// The protocol expected of a LayerSlice
/// Pulled out into a trait to provide a 'virtual' LayerSlice that
/// doesn't map exactly onto a backing framebuffer, to support scrollable layers.
pub trait LikeLayerSlice: Display {
    fn frame(&self) -> Rect;
    fn fill_rect(&self, raw_rect: Rect, color: Color, thickness: StrokeThickness);
    fn fill(&self, color: Color);
    fn putpixel(&self, loc: Point, color: Color);
    fn getpixel(&self, loc: Point) -> Color;
    fn get_slice(&self, rect: Rect) -> Box<dyn LikeLayerSlice>;
    fn blit(&self, source_layer: &Box<dyn LikeLayerSlice>, source_frame: Rect, dest_origin: Point);
    fn blit2(&self, source_layer: &Box<dyn LikeLayerSlice>);
    fn pixel_data(&self) -> Vec<u8>;
    fn draw_char(&self, ch: char, draw_loc: Point, draw_color: Color, font_size: Size);
    fn get_pixel_row(&self, y: usize) -> Vec<u8>;
    fn get_pixel_row_slice(&self, y: usize) -> (*const u8, usize);
    // First usize is the inner slice's row size, second usize is increment to get to the next
    // row in the parent framebuf
    fn get_buf_ptr_and_row_size(&self) -> (*const u8, usize, usize);

    fn track_damage(&self, r: Rect);
    fn drain_damages(&self) -> Vec<Rect>;
}

#[derive(Debug)]
pub struct LayerSlice {
    parent_framebuffer: Rc<RefCell<Box<[u8]>>>,
    parent_framebuffer_size: Size,
    bytes_per_pixel: isize,
    frame: Rect,
    pub damaged_rects: RefCell<Vec<Rect>>,
}

impl LayerSlice {
    pub fn new(
        framebuffer: Rc<RefCell<Box<[u8]>>>,
        framebuffer_size: Size,
        frame: Rect,
        bytes_per_pixel: isize,
    ) -> Self {
        LayerSlice {
            parent_framebuffer: framebuffer,
            parent_framebuffer_size: framebuffer_size,
            bytes_per_pixel,
            frame,
            damaged_rects: RefCell::new(Vec::new()),
        }
    }
}

impl Display for LayerSlice {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "<LayerSlice {}>", self.frame)
    }
}

impl LikeLayerSlice for LayerSlice {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn fill_rect(&self, raw_rect: Rect, color: Color, thickness: StrokeThickness) {
        let rect = self.frame.constrain(raw_rect);

        // Note that this rect has been damaged
        //self.record_damaged_rect(rect);

        let bpp = self.bytes_per_pixel;
        let parent_bytes_per_row = self.parent_framebuffer_size.width * bpp;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row);
        let slice_origin_offset = self.frame.origin * bpp_multiple;
        let rect_origin_offset = slice_origin_offset + (rect.origin * bpp_multiple);

        if let StrokeThickness::Width(px_count) = thickness {
            let top = Rect::from_parts(rect.origin, Size::new(rect.width(), px_count));
            self.fill_rect(top, color, StrokeThickness::Filled);

            let left = Rect::from_parts(rect.origin, Size::new(px_count, rect.height()));
            self.fill_rect(left, color, StrokeThickness::Filled);

            // The leftmost `px_count` pixels of the bottom rect are drawn by the left rect
            let bottom = Rect::from_parts(
                Point::new(rect.origin.x + px_count, rect.max_y() - px_count),
                Size::new(rect.width() - px_count, px_count),
            );
            self.fill_rect(bottom, color, StrokeThickness::Filled);

            // The topmost `px_count` pixels of the right rect are drawn by the top rect
            // The bottommost `px_count` pixels of the right rect are drawn by the bottom rect
            let right = Rect::from_parts(
                Point::new(rect.max_x() - px_count, rect.origin.y + px_count),
                Size::new(px_count, rect.height() - (px_count * 2)),
            );
            self.fill_rect(right, color, StrokeThickness::Filled);
        } else {
            self.track_damage(rect);

            let color_as_u32 = match cfg!(target_os = "axle") {
                true => {
                    (0xff_u32 << 24
                        | (color.r as u32) << 16
                        | (color.g as u32) << 8
                        | (color.b as u32))
                }
                false => {
                    // Swap channels when targeting macOS
                    (0xff_u32 << 24
                        | (color.b as u32) << 16
                        | (color.g as u32) << 8
                        | (color.r as u32))
                }
            };
            let mut fb = (*self.parent_framebuffer).borrow_mut();
            for y in 0..rect.height() {
                let row_start =
                    rect_origin_offset.y + (y * parent_bytes_per_row) + (rect_origin_offset.x);
                let mut row_slice = &mut fb
                    [(row_start as usize)..(row_start + (rect.width() * bpp as isize)) as usize];
                let (prefix, row_as_u32_slice, suffix) = unsafe { row_slice.align_to_mut::<u32>() };
                // Ensure the slice was exactly u32-aligned
                assert_eq!(prefix.len(), 0);
                assert_eq!(suffix.len(), 0);
                row_as_u32_slice.fill(color_as_u32);
            }
        }
    }

    fn fill(&self, color: Color) {
        self.fill_rect(
            Rect::from_parts(Point::zero(), self.frame.size),
            color,
            StrokeThickness::Filled,
        )
    }

    fn putpixel(&self, loc: Point, color: Color) {
        if !self.frame.contains(loc + self.frame.origin) {
            return;
            /*
            println!(
                "{:?} is uncontained in {}",
                loc + self.frame.origin,
                self.frame
            );
            */
            //assert!(false, "uncontained putpixel");
            return;
        }

        let bpp = self.bytes_per_pixel;
        let parent_bytes_per_row = self.parent_framebuffer_size.width * bpp;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row);
        let mut fb = (*self.parent_framebuffer).borrow_mut();
        let slice_origin_offset = self.frame.origin * bpp_multiple;
        //let off = slice_origin_offset + (loc.y * parent_bytes_per_row) + (loc.x * bpp);
        let point_offset = slice_origin_offset + (loc * bpp_multiple);
        let off = (point_offset.y + point_offset.x) as usize;
        fb[off + 0] = color.b;
        fb[off + 1] = color.g;
        fb[off + 2] = color.r;
        fb[off + 3] = 0xff;
    }

    fn getpixel(&self, loc: Point) -> Color {
        let bpp = self.bytes_per_pixel;
        let parent_bytes_per_row = self.parent_framebuffer_size.width * bpp;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row);
        let fb = (*self.parent_framebuffer).borrow_mut();
        let slice_origin_offset = self.frame.origin * bpp_multiple;
        //let off = slice_origin_offset + (loc.y * parent_bytes_per_row) + (loc.x * bpp);
        let point_offset = slice_origin_offset + (loc * bpp_multiple);
        let off = (point_offset.y + point_offset.x) as usize;
        Color::new(fb[off + 2], fb[off + 1], fb[off + 0])
    }

    // TODO(PT): Implement Into for Layer and LayerSlice -> LayerSlice?
    fn get_slice(&self, rect: Rect) -> Box<dyn LikeLayerSlice> {
        // The provided rect is in our coordinate system
        // Translate it to the global coordinate system, then translate
        let constrained = Rect::from_parts(Point::zero(), self.frame.size).constrain(rect);
        let to_current_coordinate_system =
            Rect::from_parts(self.frame.origin + rect.origin, constrained.size);
        //let constrained = Rect::from_parts(Point::zero(), self.frame.size)
        //    .constrain(to_current_coordinate_system);

        Box::new(LayerSlice::new(
            Rc::clone(&self.parent_framebuffer),
            self.parent_framebuffer_size,
            to_current_coordinate_system,
            self.bytes_per_pixel,
        ))
    }

    fn blit(&self, source_layer: &Box<dyn LikeLayerSlice>, src_frame: Rect, dest_origin: Point) {
        let mut fb = (*self.parent_framebuffer).borrow_mut();
        let bpp = self.bytes_per_pixel;

        let dest_frame = Rect::from_parts(dest_origin, src_frame.size);
        let dest_backing_layer_size = self.frame().size;
        // Offset into dest that we start writing
        let mut dest_row_start =
            (dest_frame.min_y() * dest_backing_layer_size.width * bpp) + (dest_frame.min_x() * bpp);
        // Offset to data from source to write to dest
        let src_backing_layer_size = source_layer.frame().size;
        let mut src_row_start =
            (src_frame.min_y() * src_backing_layer_size.width * bpp) + (src_frame.min_x() * bpp);

        let mut transferrable_rows = src_frame.height();
        let mut overhang = src_frame.max_y() - src_backing_layer_size.height;
        if overhang > 0 {
            transferrable_rows -= overhang;
        }

        // Dst doesn't necessarily start at the origin in its parent
        let parent_bytes_per_row = self.parent_framebuffer_size.width * bpp;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row);
        let dst_slice_origin_offset = self.frame.origin * bpp_multiple;
        let dst_slice_origin_offset =
            (dst_slice_origin_offset.y + dst_slice_origin_offset.x) as usize;
        let dst_row_start_to_parent = dst_slice_origin_offset + (dest_row_start as usize);

        // Copy height - y origin rows
        let total_px_in_layer = dest_backing_layer_size.area() * bpp;
        let dest_max_y = dest_frame.max_y();
        let src_pixels = source_layer.pixel_data();
        for i in 0..transferrable_rows {
            if i >= dest_max_y {
                break;
            }

            // Figure out how many pixels can actually be copied, in case the
            // source frame exceeds the dest frame
            let offset = dest_row_start;
            if offset >= total_px_in_layer {
                break;
            }

            let transferrable_px = min(src_frame.width(), dest_frame.width()) * bpp;
            //let dest_slice = self.
            //let src_pixels_in_row = source_layer.get_pixel_data(src_row_start, transferrable_px);
            let src_pixels_in_row =
                &src_pixels[(src_row_start as usize)..(src_row_start + transferrable_px) as usize];
            let mut dst_pixels_in_row = &mut fb
                [dst_row_start_to_parent..dst_row_start_to_parent + (transferrable_px as usize)];
            dst_pixels_in_row.copy_from_slice(src_pixels_in_row);

            dest_row_start += dest_backing_layer_size.width * bpp;
            src_row_start += src_backing_layer_size.width * bpp;
            //let dst_pixels_in_row =
        }
    }

    fn blit2(&self, source_layer: &Box<dyn LikeLayerSlice>) {
        assert_eq!(
            self.frame().size,
            source_layer.frame().size,
            "{} != {}",
            self.frame().size,
            source_layer.frame().size
        );
        let bpp = 4;
        let parent_size = self.parent_framebuffer_size;
        let parent_bytes_per_row = parent_size.width * bpp;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row);
        let mut fb = self.parent_framebuffer.borrow_mut();
        let slice_origin_offset = self.frame.origin * bpp_multiple;

        let (src_base, src_slice_row_size, src_parent_framebuf_row_size) =
            source_layer.get_buf_ptr_and_row_size();

        for y in 0..self.frame().height() {
            // Blit an entire row at once
            let point_offset = slice_origin_offset + (Point::new(0, y) * bpp_multiple);
            let off = (point_offset.y + point_offset.x) as usize;
            let mut dst_row_slice = &mut fb[off..off + ((self.frame.width() * bpp) as usize)];
            let src_row_slice = unsafe {
                let src_row_start = src_base.offset(y * (src_parent_framebuf_row_size as isize));
                core::slice::from_raw_parts(src_row_start, src_slice_row_size)
            };
            if dst_row_slice != src_row_slice {
                dst_row_slice.copy_from_slice(src_row_slice);
            }
        }
    }

    fn pixel_data(&self) -> Vec<u8> {
        let mut out = vec![];
        let fb = (*self.parent_framebuffer).borrow();
        let bpp = self.bytes_per_pixel;
        let parent_bytes_per_row = self.parent_framebuffer_size.width * bpp;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row);
        let slice_origin_offset = self.frame.origin * bpp_multiple;
        for y in self.frame.min_y()..self.frame.max_y() {
            let row_start = slice_origin_offset.y + (y * parent_bytes_per_row);
            for x in self.frame.min_x()..self.frame.max_x() {
                let offset = (slice_origin_offset.x + row_start + (x * bpp)) as usize;
                for byte in 0..(bpp as usize) {
                    out.push(fb[offset + byte]);
                }
            }
        }
        out
    }

    fn draw_char(&self, ch: char, draw_loc: Point, draw_color: Color, font_size: Size) {
        // Scale font to the requested size
        let scale_x: f64 = (font_size.width as f64) / (CHAR_WIDTH as f64);
        let scale_y: f64 = (font_size.height as f64) / (CHAR_HEIGHT as f64);

        let bitmap = FONT8X8[ch as usize];

        for draw_y in 0..font_size.height {
            // Go from scaled pixel back to 8x8 font
            let font_y = (draw_y as f64 / scale_y) as usize;
            let row = bitmap[font_y];
            for draw_x in 0..font_size.width {
                let font_x = (draw_x as f64 / scale_x) as usize;
                if row >> font_x & 0b1 != 0 {
                    self.putpixel(draw_loc + Point::new(draw_x, draw_y), draw_color);
                }
            }
        }
    }

    fn get_pixel_row(&self, y: usize) -> Vec<u8> {
        let bpp = 4;
        let parent_size = self.parent_framebuffer_size;
        let parent_bytes_per_row = (parent_size.width * bpp) as usize;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row as isize);
        let pixels = self.parent_framebuffer.borrow();
        let slice_origin_offset = self.frame.origin * bpp_multiple;
        // The offset where the pixel data for this slice begins in the parent pixel buffer
        let slice_pixel_data_start = (slice_origin_offset.y + slice_origin_offset.x) as usize;
        // The offset where the provided `y` row begins in the parent pixel buffer
        let row_pixel_data_start = slice_pixel_data_start + (y * parent_bytes_per_row);
        let slice_bytes_per_row = (self.frame.width() * bpp) as usize;
        pixels[row_pixel_data_start..row_pixel_data_start + slice_bytes_per_row].to_vec()
    }

    fn get_pixel_row_slice(&self, y: usize) -> (*const u8, usize) {
        let bpp = 4;
        let parent_size = self.parent_framebuffer_size;
        let parent_bytes_per_row = (parent_size.width * bpp) as usize;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row as isize);
        let pixels = self.parent_framebuffer.borrow();
        let slice_origin_offset = self.frame.origin * bpp_multiple;
        // The offset where the pixel data for this slice begins in the parent pixel buffer
        let slice_pixel_data_start = (slice_origin_offset.y + slice_origin_offset.x) as usize;
        // The offset where the provided `y` row begins in the parent pixel buffer
        let row_pixel_data_start = slice_pixel_data_start + (y * parent_bytes_per_row);
        let slice_bytes_per_row = (self.frame.width() * bpp) as usize;
        (
            pixels[row_pixel_data_start..row_pixel_data_start + slice_bytes_per_row].as_ptr(),
            slice_bytes_per_row,
        )
    }

    fn get_buf_ptr_and_row_size(&self) -> (*const u8, usize, usize) {
        let bpp = 4;
        let parent_size = self.parent_framebuffer_size;
        let parent_bytes_per_row = (parent_size.width * bpp) as usize;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row as isize);
        let pixels = self.parent_framebuffer.borrow();
        let slice_origin_offset = self.frame.origin * bpp_multiple;
        // The offset where the pixel data for this slice begins in the parent pixel buffer
        let slice_pixel_data_start = (slice_origin_offset.y + slice_origin_offset.x) as usize;
        let slice_bytes_per_row = (self.frame.width() * bpp) as usize;
        (
            pixels[slice_pixel_data_start..].as_ptr(),
            slice_bytes_per_row,
            parent_bytes_per_row,
        )
    }

    fn track_damage(&self, r: Rect) {
        self.damaged_rects.borrow_mut().push(r)
    }

    fn drain_damages(&self) -> Vec<Rect> {
        self.damaged_rects.borrow_mut().drain(..).collect()
    }
}

pub trait Layer {
    fn size(&self) -> Size;
    fn bytes_per_pixel(&self) -> isize;

    fn width(&self) -> isize {
        self.size().width
    }

    fn height(&self) -> isize {
        self.size().height
    }

    fn fill_rect(&self, rect: &Rect, color: Color);
    fn putpixel(&self, loc: &Point, color: Color);
    fn get_slice(&mut self, rect: Rect) -> Box<dyn LikeLayerSlice>;
}

#[derive(PartialEq)]
pub struct SingleFramebufferLayer {
    pub framebuffer: Rc<RefCell<Box<[u8]>>>,
    bytes_per_pixel: isize,
    size: Size,
}

impl SingleFramebufferLayer {
    pub fn new(size: Size) -> Self {
        let bytes_per_pixel = 4;
        //let max_size = Size::new(600, 480);
        //let framebuf_size = max_size.width * max_size.height * bytes_per_pixel;
        let framebuf_size = size.width * size.height * bytes_per_pixel;
        let framebuffer = vec![0; framebuf_size.try_into().unwrap()];

        SingleFramebufferLayer {
            framebuffer: Rc::new(RefCell::new(framebuffer.into_boxed_slice())),
            bytes_per_pixel: bytes_per_pixel,
            size,
        }
    }

    pub fn from_framebuffer(framebuffer: Box<[u8]>, bytes_per_pixel: isize, size: Size) -> Self {
        SingleFramebufferLayer {
            framebuffer: Rc::new(RefCell::new(framebuffer)),
            bytes_per_pixel,
            size,
        }
    }

    pub fn copy_from(&self, other: &SingleFramebufferLayer) {
        assert!(self.size == other.size);
        let mut dst_fb = self.framebuffer.borrow_mut();
        let src_fb = other.framebuffer.borrow();
        dst_fb.copy_from_slice(&src_fb);
    }

    pub fn get_full_slice(&mut self) -> Box<dyn LikeLayerSlice> {
        self.get_slice(Rect::with_size(self.size))
    }
}

impl Layer for SingleFramebufferLayer {
    fn bytes_per_pixel(&self) -> isize {
        self.bytes_per_pixel
    }
    fn size(&self) -> Size {
        self.size
    }

    fn fill_rect(&self, rect: &Rect, color: Color) {
        if ![3, 4].contains(&self.bytes_per_pixel()) {
            unimplemented!();
        }
        let mut framebuffer = (*self.framebuffer).borrow_mut();
        let start = (rect.min_y() * self.width() * self.bytes_per_pixel())
            + (rect.min_x() * self.bytes_per_pixel());
        for y in 0..rect.size.height {
            let row_off = y * self.width() * self.bytes_per_pixel();
            for x in 0..rect.size.width {
                let px_off = (start + row_off + (x * self.bytes_per_pixel())) as usize;
                framebuffer[px_off + 0] = color.r;
                framebuffer[px_off + 1] = color.g;
                framebuffer[px_off + 2] = color.b;
                framebuffer[px_off + 3] = 0xff;
            }
        }
    }

    fn putpixel(&self, loc: &Point, color: Color) {
        let mut framebuffer = (*self.framebuffer).borrow_mut();
        let off = ((loc.y * self.width() * self.bytes_per_pixel())
            + (loc.x * self.bytes_per_pixel())) as usize;
        framebuffer[off + 0] = color.b;
        framebuffer[off + 1] = color.g;
        framebuffer[off + 2] = color.r;
        framebuffer[off + 3] = 0xff;
    }

    fn get_slice(&mut self, rect: Rect) -> Box<dyn LikeLayerSlice> {
        let constrained = Rect::from_parts(Point::zero(), self.size).constrain(rect);
        Box::new(LayerSlice::new(
            Rc::clone(&self.framebuffer),
            self.size(),
            constrained,
            self.bytes_per_pixel,
        ))
    }
}

/*
pub struct ScrollableLayer {
    // TODO(PT): Maybe these should be stored with positions?
    backing_layers: Vec<SingleFramebufferLayer>,
    bytes_per_pixel: usize,
    // Represents total size
    visible_size: Size,
}

impl ScrollableLayer {
    pub fn new(visible_size: Size) -> Self {
        ScrollableLayer {
            // Defer creating the first layer until we receive a draw request
            backing_layers: Vec::new(),
            bytes_per_pixel: 4,
            visible_size,
        }
    }
}

impl Layer for ScrollableLayer {
    fn bytes_per_pixel(&self) -> usize {
        self.bytes_per_pixel
    }
    fn size(&self) -> Size {
        self.visible_size
    }

    // TODO(PT): Need to remove framebuffer from the trait as this type has multiple framebuffers
    fn fill_rect(&self, rect: &Rect, color: Color) {
        SingleFramebufferLayer::fill_rect(self, rect, color)
    }

    fn putpixel(&self, loc: &Point, color: Color) {
        SingleFramebufferLayer::putpixel(self, loc, color)
    }

    fn get_slice(&self, rect: Rect) -> LayerSlice {
        SingleFramebufferLayer::get_slice(self, rect)
    }
}

//impl SingleFramebufferLayer for ScrollableLayer {}

*/

#[cfg(test)]
extern crate std;
#[cfg(test)]
use std::println;

#[test]
fn fill_rect() {
    // Given a layer
    let mut layer = SingleFramebufferLayer::new(Size::new(100, 100));
    // And a slice
    // And the slice encompasses the entire framebuffer
    let slice = layer.get_slice(Rect::new(0, 0, 100, 100));
    // When I fill the slice with a given color
    let color = Color::new(0, 100, 123);
    slice.fill_rect(Rect::new(0, 0, 100, 100), color, StrokeThickness::Filled);
    // Then then entire framebuffer is filled with this sequence
    let fb = (*layer.framebuffer).borrow();
    let bytes_per_row = layer.bytes_per_pixel * layer.size.width;
    for y in 0..100 {
        for x in 0..100 {
            let off = ((y * bytes_per_row) + (x * layer.bytes_per_pixel)) as usize;
            assert_eq!(fb[off + 0], color.b);
            assert_eq!(fb[off + 1], color.g);
            assert_eq!(fb[off + 2], color.r);
            assert_eq!(fb[off + 3], 0xff);
        }
    }
}

#[test]
fn fill_rect_constrains_rect() {
    // Given a layer
    let mut layer = SingleFramebufferLayer::new(Size::new(100, 100));
    // And a slice
    let slice = layer.get_slice(Rect::new(50, 50, 30, 30));

    // When I fill the slice with a given color
    let color = Color::new(50, 100, 123);
    slice.fill_rect(Rect::new(10, 20, 400, 800), color, StrokeThickness::Filled);

    let expected_rect = Rect::new(60, 70, 20, 10);

    let fb = (*layer.framebuffer).borrow();
    let bytes_per_row = layer.bytes_per_pixel * layer.size.width;
    for y in 0..100 {
        for x in 0..100 {
            let p = Point::new(x, y);
            let off = ((y * bytes_per_row) + (x * layer.bytes_per_pixel)) as usize;
            if expected_rect.contains(p) {
                assert_eq!(fb[off + 0], color.b);
                assert_eq!(fb[off + 1], color.g);
                assert_eq!(fb[off + 2], color.r);
                assert_eq!(fb[off + 3], 0xff);
            } else {
                assert_eq!(fb[off + 0], 0);
                assert_eq!(fb[off + 1], 0);
                assert_eq!(fb[off + 2], 0);
                assert_eq!(fb[off + 3], 0);
            }
        }
    }
}

#[test]
fn slice_constrains_rect() {
    // Given a layer
    let mut layer = SingleFramebufferLayer::new(Size::new(100, 100));

    // When I request a slice
    // And the requested slice frame is outside the bounds of the layer
    let slice = layer.get_slice(Rect::new(0, 0, 200, 200));
    // Then the returned slice is constrained to the available size in the layer
    assert_eq!(slice.frame(), Rect::new(0, 0, 100, 100));

    // When I request a slice
    let slice2 = layer.get_slice(Rect::new(99, 99, 100, 100));
    // Then the returned slice is constrained to the available size in the layer
    assert_eq!(slice2.frame(), Rect::new(99, 99, 1, 1));

    // When I request a slice
    let slice3 = layer.get_slice(Rect::new(50, 50, 150, 150));
    // Then the returned slice is constrained to the available size in the layer
    assert_eq!(slice3.frame(), Rect::new(50, 50, 50, 50));

    // When I request a slice
    let slice3 = layer.get_slice(Rect::new(500, 500, 500, 500));
    // Then the returned slice is constrained to the available size in the layer
    assert_eq!(slice3.frame(), Rect::new(0, 0, 0, 0));
}
