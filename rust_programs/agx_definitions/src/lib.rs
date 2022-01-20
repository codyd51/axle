#![no_std]
#![feature(core_intrinsics)]

extern crate alloc;
use alloc::rc::Weak;
use core::{
    cmp::max,
    ops::{Add, Mul, Sub},
};

pub mod layer;
pub use layer::*;

pub trait NestedLayerSlice: Drawable {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>>;
    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>);

    fn get_slice(&self) -> LayerSlice {
        let parent = self.get_parent().unwrap().upgrade().unwrap();
        let parent_slice = parent.get_slice();

        let content_frame = parent.content_frame();
        let constrained_to_content_frame = content_frame.constrain(self.frame());
        parent_slice.get_slice(Rect::from_parts(
            content_frame.origin + self.frame().origin,
            constrained_to_content_frame.size,
        ))
    }
}

pub trait Drawable {
    fn frame(&self) -> Rect;

    fn content_frame(&self) -> Rect;

    fn draw(&self);
}

#[derive(Debug, Copy, Clone)]
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
    pub fn black() -> Self {
        Color::from([0, 0, 0])
    }
    pub fn white() -> Self {
        Color::from([255, 255, 255])
    }
    pub fn gray() -> Self {
        Color::from([127, 127, 127])
    }
    pub fn dark_gray() -> Self {
        Color::from([80, 80, 80])
    }
    pub fn light_gray() -> Self {
        Color::from([120, 120, 120])
    }
    pub fn red() -> Self {
        Color::from([255, 0, 0])
    }
    pub fn green() -> Self {
        Color::from([0, 255, 0])
    }
    pub fn blue() -> Self {
        Color::from([0, 0, 255])
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

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Size {
    pub width: isize,
    pub height: isize,
}

impl Size {
    pub fn new(width: isize, height: isize) -> Self {
        Size { width, height }
    }

    pub fn zero() -> Self {
        Size {
            width: 0,
            height: 0,
        }
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

    pub fn from(point: Point) -> Self {
        PointU32 {
            x: point.x as u32,
            y: point.y as u32,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Point {
    pub x: isize,
    pub y: isize,
}

impl Point {
    pub fn new(x: isize, y: isize) -> Self {
        Point { x, y }
    }
    pub fn zero() -> Self {
        Point { x: 0, y: 0 }
    }
    pub fn from(point: PointU32) -> Self {
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

impl Mul<Point> for Point {
    type Output = Point;
    fn mul(self, rhs: Self) -> Self::Output {
        Point {
            x: self.x * rhs.x,
            y: self.y * rhs.y,
        }
    }
}

impl Mul<isize> for Point {
    type Output = Point;
    fn mul(self, rhs: isize) -> Self::Output {
        Point {
            x: self.x * rhs,
            y: self.y * rhs,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct RectInsets {
    pub left: isize,
    pub top: isize,
    pub right: isize,
    pub bottom: isize,
}

impl RectInsets {
    pub fn new(left: isize, top: isize, right: isize, bottom: isize) -> Self {
        RectInsets {
            left,
            top,
            right,
            bottom,
        }
    }

    pub fn zero() -> Self {
        RectInsets {
            left: 0,
            top: 0,
            right: 0,
            bottom: 0,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Rect {
    pub origin: Point,
    pub size: Size,
}

impl Rect {
    pub fn new(x: isize, y: isize, width: isize, height: isize) -> Self {
        Rect {
            origin: Point::new(x, y),
            size: Size::new(width, height),
        }
    }

    pub fn from_parts(origin: Point, size: Size) -> Self {
        Rect { origin, size }
    }

    pub fn zero() -> Self {
        Rect::from_parts(Point::zero(), Size::zero())
    }

    pub fn inset_by(&self, bottom: isize, left: isize, right: isize, top: isize) -> Self {
        Rect::from_parts(
            self.origin + Point::new(left, top),
            self.size - Size::new(right * 2, bottom * 2),
        )
    }

    pub fn min_x(&self) -> isize {
        self.origin.x
    }

    pub fn min_y(&self) -> isize {
        self.origin.y
    }

    pub fn max_x(&self) -> isize {
        self.min_x() + self.size.width
    }

    pub fn max_y(&self) -> isize {
        self.min_y() + self.size.height
    }

    pub fn mid_x(&self) -> isize {
        self.min_x() + ((self.size.width as f64 / 2f64) as isize)
    }

    pub fn mid_y(&self) -> isize {
        self.min_y() + ((self.size.height as f64 / 2f64) as isize)
    }

    pub fn width(&self) -> isize {
        self.size.width
    }

    pub fn height(&self) -> isize {
        self.size.height
    }

    pub fn center(&self) -> Point {
        Point::new(self.mid_x(), self.mid_y())
    }

    pub fn contains(&self, p: Point) -> bool {
        p.x >= self.min_x() && p.y >= self.min_y() && p.x < self.max_x() && p.y < self.max_y()
    }

    pub fn constrain(&self, rhs: Self) -> Self {
        if rhs.min_x() >= self.max_x() || rhs.min_y() >= self.max_y() {
            return Rect::zero();
        }

        let mut width = rhs.width();
        if rhs.max_x() > self.width() {
            width -= rhs.max_x() - self.width();
        }

        let mut height = rhs.height();
        if rhs.max_y() > self.height() {
            height -= rhs.max_y() - self.height();
        }

        Rect::from_parts(rhs.origin, Size::new(width, height))
    }

    pub fn apply_insets(&self, insets: RectInsets) -> Self {
        Rect::new(
            self.origin.x + insets.left,
            self.origin.y + insets.top,
            self.size.width - (insets.left + insets.right),
            self.size.height - (insets.top + insets.bottom),
        )
    }
}

/*
impl Add for Rect {
    type Output = Rect;
    fn add(self, rhs: Self) -> Self::Output {
        Rect::from_parts(self.origin + rhs.origin, self.size + rhs.size)
    }
}
*/

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Line {
    p1: Point,
    p2: Point,
}

impl Line {
    pub fn new(p1: Point, p2: Point) -> Self {
        Line { p1, p2 }
    }

    fn draw_strip(&self, onto: &mut LayerSlice, color: Color) {
        // Relative distances in both directions
        let mut delta_x = self.p2.x - self.p1.x;
        let mut delta_y = self.p2.y - self.p1.y;

        // Increment of 0 would imply either vertical or horizontal line
        let inc_x = match delta_x {
            _ if delta_x > 0 => 1,
            _ if delta_x == 0 => 0,
            _ => -1,
        };
        let inc_y = match delta_y {
            _ if delta_y > 0 => 1,
            _ if delta_y == 0 => 0,
            _ => -1,
        };

        //let distance = max(delta_x.abs(), delta_y.abs());
        delta_x = delta_x.abs();
        delta_y = delta_y.abs();
        let distance = max(delta_x, delta_y);

        let mut cursor = Point::new(self.p1.x, self.p1.y);
        let mut x_err = 0;
        let mut y_err = 0;
        for _ in 0..distance {
            onto.putpixel(cursor, color);

            x_err += delta_x;
            y_err += delta_y;

            if x_err > distance {
                x_err -= distance;
                cursor.x += inc_x;
            }
            if y_err > distance {
                y_err -= distance;
                cursor.y += inc_y;
            }
        }
    }

    pub fn draw(&self, onto: &mut LayerSlice, color: Color, thickness: StrokeThickness) {
        if let StrokeThickness::Width(thickness) = thickness {
            let off = (thickness / 2) as isize;
            for i in 0..thickness {
                let mut subline = self.clone();
                subline.p1.x += i - off;
                subline.p2.x += i - off;
                subline.draw_strip(onto, color);
            }
        } else {
            self.draw_strip(onto, color);
        }
    }
}
