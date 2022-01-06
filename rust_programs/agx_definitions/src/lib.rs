#![no_std]

extern crate alloc;
use alloc::vec::Vec;
use core::ops::{Add, Sub};

pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl Color {
    pub fn new(r: u8, g: u8, b: u8) -> Self {
        Color { r, g, b }
    }
    pub fn from(vals: [u8; 3]) -> Self {
        Color::new(vals[0], vals[1], vals[2])
    }
    pub fn white() -> Self {
        Color::from([255, 255, 255])
    }
    pub fn gray() -> Self {
        Color::from([127, 127, 127])
    }
    pub fn black() -> Self {
        Color::from([0, 0, 0])
    }
}

// For FFI
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct SizeU32 {
    pub width: u32,
    pub height: u32,
}

impl SizeU32 {
    pub fn new(width: u32, height: u32) -> Self {
        SizeU32 { width, height }
    }

    pub fn from(size: &Size) -> Self {
        SizeU32 {
            width: size.width as u32,
            height: size.height as u32,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct Size {
    pub width: usize,
    pub height: usize,
}

impl Size {
    pub fn new(width: usize, height: usize) -> Self {
        Size { width, height }
    }
}

impl Size {
    pub fn from(size: &SizeU32) -> Self {
        Size {
            width: size.width.try_into().unwrap(),
            height: size.height.try_into().unwrap(),
        }
    }
}

impl Add for Size {
    type Output = Size;
    fn add(self, rhs: Self) -> Self::Output {
        Size {
            width: self.width + rhs.width,
            height: self.height + rhs.height,
        }
    }
}

impl Sub for Size {
    type Output = Size;
    fn sub(self, rhs: Self) -> Self::Output {
        Size {
            width: self.width - rhs.width,
            height: self.height - rhs.height,
        }
    }
}

// For FFI
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct PointU32 {
    pub x: u32,
    pub y: u32,
}

impl PointU32 {
    pub fn new(x: u32, y: u32) -> Self {
        PointU32 { x, y }
    }

    pub fn from(point: &Point) -> Self {
        PointU32 {
            x: point.x as u32,
            y: point.y as u32,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct Point {
    pub x: usize,
    pub y: usize,
}

impl Point {
    pub fn new(x: usize, y: usize) -> Self {
        Point { x, y }
    }
    pub fn zero() -> Self {
        Point { x: 0, y: 0 }
    }
    pub fn from(point: &PointU32) -> Self {
        Point {
            x: point.x.try_into().unwrap(),
            y: point.y.try_into().unwrap(),
        }
    }
}

impl Add for Point {
    type Output = Point;
    fn add(self, rhs: Self) -> Self::Output {
        Point {
            x: self.x + rhs.x,
            y: self.y + rhs.y,
        }
    }
}

impl Sub for Point {
    type Output = Point;
    fn sub(self, rhs: Self) -> Self::Output {
        Point {
            x: self.x - rhs.x,
            y: self.y - rhs.y,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct Rect {
    pub origin: Point,
    pub size: Size,
}

impl Rect {
    pub fn new(origin: Point, size: Size) -> Self {
        Rect { origin, size }
    }

    pub fn inset_by(&self, bottom: usize, left: usize, right: usize, top: usize) -> Self {
        // TODO(PT): Handle underflow by making Size signed? or handle explicitly here?
        Rect::new(
            self.origin + Point::new(left, top),
            self.size - Size::new(right * 2, bottom * 2),
        )
    }

    pub fn min_x(&self) -> usize {
        self.origin.x
    }

    pub fn min_y(&self) -> usize {
        self.origin.y
    }

    pub fn max_x(&self) -> usize {
        self.min_x() + self.size.width
    }

    pub fn max_y(&self) -> usize {
        self.min_y() + self.size.height
    }

    pub fn mid_x(&self) -> usize {
        self.min_x() + ((self.size.width as f64 / 2f64) as usize)
    }

    pub fn mid_y(&self) -> usize {
        self.min_y() + ((self.size.height as f64 / 2f64) as usize)
    }

    pub fn center(&self) -> Point {
        Point::new(self.mid_x(), self.mid_y())
    }

    pub fn contains(&self, p: Point) -> bool {
        p.x >= self.min_x() && p.y >= self.min_y() && p.x < self.max_x() && p.y < self.max_y()
    }
}

pub trait Layer {
    fn size(&self) -> Size;
    fn bytes_per_pixel(&self) -> usize;
    fn framebuffer(&mut self) -> &mut [u8];

    fn width(&self) -> usize {
        (self.size().width as usize).try_into().unwrap()
    }

    fn height(&self) -> usize {
        (self.size().height as usize).try_into().unwrap()
    }

    fn fill_rect(&mut self, rect: &Rect, color: &Color) {
        if ![3usize, 4].contains(&self.bytes_per_pixel()) {
            unimplemented!();
        }
        let start = (rect.min_y() * self.width() * self.bytes_per_pixel())
            + (rect.min_x() * self.bytes_per_pixel());
        for y in 0..rect.size.height {
            let row_off = y * self.width() * self.bytes_per_pixel();
            for x in 0..rect.size.width {
                let px_off = start + row_off + (x * self.bytes_per_pixel());
                self.framebuffer()[px_off + 0] = color.r;
                self.framebuffer()[px_off + 1] = color.g;
                self.framebuffer()[px_off + 2] = color.b;
            }
        }
    }
}

#[derive(Debug)]
pub struct UnownedLayer<'a> {
    framebuffer: &'a mut [u8],
    bytes_per_pixel: usize,
    size: Size,
}

impl<'a> UnownedLayer<'a> {
    pub fn new(framebuffer: &'a mut [u8], bytes_per_pixel: usize, size: Size) -> Self {
        UnownedLayer {
            framebuffer,
            bytes_per_pixel,
            size,
        }
    }
}

impl Layer for UnownedLayer<'_> {
    fn bytes_per_pixel(&self) -> usize {
        self.bytes_per_pixel
    }
    fn size(&self) -> Size {
        self.size
    }
    fn framebuffer(&mut self) -> &mut [u8] {
        &mut self.framebuffer
    }
}

#[derive(Debug)]
pub struct OwnedLayer {
    framebuffer: Vec<u8>,
    bytes_per_pixel: usize,
    size: Size,
}

impl OwnedLayer {
    pub fn new(framebuffer: Vec<u8>, bytes_per_pixel: usize, size: Size) -> Self {
        OwnedLayer {
            framebuffer,
            bytes_per_pixel,
            size,
        }
    }
}

impl Layer for OwnedLayer {
    fn bytes_per_pixel(&self) -> usize {
        self.bytes_per_pixel
    }
    fn size(&self) -> Size {
        self.size
    }
    fn framebuffer(&mut self) -> &mut [u8] {
        &mut self.framebuffer
    }
}

pub fn putpixel(layer: &mut dyn Layer, loc: &Point, color: &Color) {
    let off = (loc.y * layer.width() * layer.bytes_per_pixel()) + (loc.x * layer.bytes_per_pixel());
    layer.framebuffer()[off + 0] = color.r;
    layer.framebuffer()[off + 1] = color.g;
    layer.framebuffer()[off + 2] = color.b;
}
