extern crate alloc;
use crate::{Color, Point, Rect, Size};
use alloc::vec;
use alloc::{boxed::Box, rc::Rc, vec::Vec};
use core::cell::RefCell;

pub enum StrokeThickness {
    Filled,
    // TODO(PT): Support per-side thickness?
    Width(isize),
}

#[derive(Debug)]
pub struct LayerSlice {
    parent_framebuffer: Rc<RefCell<Box<[u8]>>>,
    parent_framebuffer_size: Size,
    bytes_per_pixel: isize,
    pub frame: Rect,
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

    fn record_damaged_rect(&self, rect: Rect) {
        let mut damaged_rects = self.damaged_rects.borrow_mut();
        damaged_rects.push(rect);
    }

    pub fn fill_rect(&self, raw_rect: Rect, color: Color, thickness: StrokeThickness) {
        let rect = self.frame.constrain(raw_rect);

        // Note that this rect has been damaged
        self.record_damaged_rect(rect);

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
            let mut fb = (*self.parent_framebuffer).borrow_mut();
            for y in 0..rect.height() {
                let row_start = rect_origin_offset.y + (y * parent_bytes_per_row);
                for x in 0..rect.width() {
                    let offset = (rect_origin_offset.x + row_start + (x * bpp)) as usize;
                    fb[offset + 0] = color.b;
                    fb[offset + 1] = color.g;
                    fb[offset + 2] = color.r;
                }
            }
        }
    }

    pub fn fill(&self, color: Color) {
        self.fill_rect(
            Rect::from_parts(Point::zero(), self.frame.size),
            color,
            StrokeThickness::Filled,
        )
    }

    pub fn putpixel(&self, loc: Point, color: Color) {
        if !self.frame.contains(loc + self.frame.origin) {
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
    }

    // TODO(PT): Implement Into for Layer and LayerSlice -> LayerSlice?
    pub fn get_slice(&self, rect: Rect) -> LayerSlice {
        // The provided rect is in our coordinate system
        // Translate it to the global coordinate system, then translate
        let constrained = Rect::from_parts(Point::zero(), self.frame.size).constrain(rect);
        let to_current_coordinate_system =
            Rect::from_parts(self.frame.origin + rect.origin, constrained.size);
        //let constrained = Rect::from_parts(Point::zero(), self.frame.size)
        //    .constrain(to_current_coordinate_system);

        LayerSlice::new(
            Rc::clone(&self.parent_framebuffer),
            self.parent_framebuffer_size,
            to_current_coordinate_system,
            self.bytes_per_pixel,
        )
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
    //fn getpixel(&self, loc: &Point) -> &'a [u8];
    fn get_slice(&mut self, rect: Rect) -> LayerSlice;
}

pub struct SingleFramebufferLayer {
    pub framebuffer: Rc<RefCell<Box<[u8]>>>,
    bytes_per_pixel: isize,
    size: Size,
}

impl SingleFramebufferLayer {
    pub fn new(size: Size) -> Self {
        let bytes_per_pixel = 4;
        let max_size = Size::new(600, 480);
        let framebuf_size = max_size.width * max_size.height * bytes_per_pixel;
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
            }
        }
    }

    fn putpixel(&self, loc: &Point, color: Color) {
        let mut framebuffer = (*self.framebuffer).borrow_mut();
        let off = ((loc.y * self.width() * self.bytes_per_pixel())
            + (loc.x * self.bytes_per_pixel())) as usize;
        framebuffer[off + 0] = color.r;
        framebuffer[off + 1] = color.g;
        framebuffer[off + 2] = color.b;
    }

    fn get_slice(&mut self, rect: Rect) -> LayerSlice {
        let constrained = Rect::from_parts(Point::zero(), self.size).constrain(rect);
        LayerSlice::new(
            Rc::clone(&self.framebuffer),
            self.size(),
            constrained,
            self.bytes_per_pixel,
        )
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
            } else {
                assert_eq!(fb[off + 0], 0);
                assert_eq!(fb[off + 1], 0);
                assert_eq!(fb[off + 2], 0);
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
    assert_eq!(slice.frame, Rect::new(0, 0, 100, 100));

    // When I request a slice
    let slice2 = layer.get_slice(Rect::new(99, 99, 100, 100));
    // Then the returned slice is constrained to the available size in the layer
    assert_eq!(slice2.frame, Rect::new(99, 99, 1, 1));

    // When I request a slice
    let slice3 = layer.get_slice(Rect::new(50, 50, 150, 150));
    // Then the returned slice is constrained to the available size in the layer
    assert_eq!(slice3.frame, Rect::new(50, 50, 50, 50));

    // When I request a slice
    let slice3 = layer.get_slice(Rect::new(500, 500, 500, 500));
    // Then the returned slice is constrained to the available size in the layer
    assert_eq!(slice3.frame, Rect::new(0, 0, 0, 0));
}
