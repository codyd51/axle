#![no_std]

use core::ops::Add;

pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl Color {
    pub fn new(r: u8, g: u8, b: u8) -> Self {
        Color { r, g, b }
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

#[derive(Debug, Clone, Copy)]
pub struct Point {
    pub x: usize,
    pub y: usize,
}

impl Point {
    pub fn new(x: usize, y: usize) -> Self {
        Point { x, y }
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

#[derive(Debug)]
pub struct Layer<'a> {
    framebuffer: &'a mut [u8],
    bytes_per_pixel: usize,
    size: Size,
}

impl<'a> Layer<'a> {
    pub fn new(framebuffer: &'a mut [u8], bytes_per_pixel: usize, size: Size) -> Self {
        Layer {
            framebuffer,
            bytes_per_pixel,
            size,
        }
    }

    pub fn width(&self) -> usize {
        (self.size.width as usize).try_into().unwrap()
    }

    pub fn height(&self) -> usize {
        (self.size.height as usize).try_into().unwrap()
    }
}

pub fn putpixel(layer: &mut Layer, loc: &Point, color: &Color) {
    let off = (loc.y * layer.width() * layer.bytes_per_pixel) + (loc.x * layer.bytes_per_pixel);
    layer.framebuffer[off + 0] = color.r;
    layer.framebuffer[off + 1] = color.g;
    layer.framebuffer[off + 2] = color.b;
}
