#![cfg_attr(target_os = "axle", no_std)]
#![feature(core_intrinsics)]
#![feature(slice_ptr_get)]
#![feature(default_alloc_error_handler)]
#![feature(format_args_nl)]

extern crate alloc;
extern crate core;
#[cfg(target_os = "axle")]
extern crate libc;

use alloc::fmt::Debug;
use alloc::vec;
use alloc::{format, rc::Rc, string::String, vec::Vec};

use alloc::boxed::Box;
use alloc::rc::Weak;
use core::fmt::Formatter;
use core::{
    cmp::{max, min},
    fmt::Display,
    ops::{Add, Mul, Sub},
};
use num_traits::Float;

#[cfg(target_os = "axle")]
use axle_rt::println;
#[cfg(not(target_os = "axle"))]
use std::println;

pub mod font;
pub mod layer;
pub use font::*;
pub use layer::*;

pub trait NestedLayerSlice: Drawable {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>>;
    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>);

    fn get_content_slice_frame(&self) -> Rect {
        let parent = self.get_parent().unwrap().upgrade().unwrap();
        let content_frame = parent.content_frame();
        let constrained_to_content_frame = content_frame.constrain(self.frame());

        /*
        parent_slice.get_slice(Rect::from_parts(
            content_frame.origin + self.frame().origin,
            constrained_to_content_frame.size,
        ))
        */

        let mut origin = content_frame.origin + self.frame().origin;
        /*
        origin.x = max(origin.x, 0);
        origin.y = max(origin.y, 0);
        */
        let mut size = constrained_to_content_frame.size;
        if origin.x < 0 {
            let overhang = -origin.x;
            size.width -= overhang;
            origin.x = 0;
        }
        if origin.y < 0 {
            let overhang = -origin.y;
            size.height -= overhang;
            origin.y = 0;
        }
        Rect::from_parts(origin, size)
    }

    fn get_slice(&self) -> Box<dyn LikeLayerSlice> {
        let parent = self.get_parent().unwrap().upgrade().unwrap();
        let parent_slice = parent.get_slice();
        let content_slice_frame = self.get_content_slice_frame();
        parent_slice.get_slice(content_slice_frame)
    }

    fn get_slice_for_render(&self) -> Box<dyn LikeLayerSlice>;
}

pub trait Drawable {
    fn frame(&self) -> Rect;

    fn content_frame(&self) -> Rect;

    /// Returns the rects damaged while drawing
    fn draw(&self) -> Vec<Rect>;
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
    pub fn yellow() -> Self {
        Color::from([0, 234, 255])
    }

    /// Swag RGB to BGR and vice versa
    pub fn swap_order(&self) -> Self {
        Color::new(self.b, self.g, self.r)
    }
}

impl Display for Color {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "Color({}, {}, {})", self.r, self.g, self.b)
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

    pub fn from(size: Size) -> Self {
        SizeU32 {
            width: size.width as u32,
            height: size.height as u32,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Ord, PartialOrd, Eq)]
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

    pub fn area(&self) -> isize {
        self.width * self.height
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

impl Display for Size {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "({}, {})", self.width, self.height)
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

#[derive(Debug, Clone, Copy, PartialEq, Ord, PartialOrd, Eq)]
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

    pub fn distance(&self, p2: Point) -> f64 {
        let p1 = self;
        let x_dist = p2.x - p1.x;
        let y_dist = p2.y - p1.y;
        let hypotenuse_squared = x_dist.pow(2) + y_dist.pow(2);
        (hypotenuse_squared as f64).sqrt()
    }
}

impl Display for Point {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "({}, {})", self.x, self.y)
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

    pub fn uniform(inset: isize) -> Self {
        RectInsets {
            left: inset,
            top: inset,
            right: inset,
            bottom: inset,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Ord, PartialOrd, Eq)]
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

    pub fn with_size(size: Size) -> Self {
        Self {
            origin: Point::zero(),
            size,
        }
    }

    pub fn replace_origin(&self, new_origin: Point) -> Self {
        Self::from_parts(new_origin, self.size)
    }

    pub fn replace_size(&self, new_size: Size) -> Self {
        Self::from_parts(self.origin, new_size)
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
            self.size - Size::new(left + right, top + bottom),
        )
    }

    pub fn inset_by_insets(&self, insets: RectInsets) -> Self {
        self.inset_by(insets.bottom, insets.left, insets.right, insets.top)
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

    pub fn midpoint(&self) -> Point {
        Point::new(self.mid_x(), self.mid_y())
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

    pub fn encloses(&self, rhs: Self) -> bool {
        rhs.min_x() >= self.min_x()
            && rhs.min_y() >= self.min_y()
            && rhs.max_x() <= self.max_x()
            && rhs.max_y() <= self.max_y()
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

        let mut origin = rhs.origin;
        /*
        if rhs.min_x() < self.min_x() {
            origin.x = 0;
        }
        if rhs.min_y() < self.min_y() {
            origin.y = 0;
        }
        */
        /*
        if rhs.min_x() < self.min_x() {
            width -= self.min_x() - rhs.min_x();
            origin.x = 0;
        }
        if rhs.min_y() < self.min_y() {
            height -= self.min_y() - rhs.min_y();
            origin.y = 0;
        }
        */

        Rect::from_parts(origin, Size::new(width, height))
    }

    pub fn apply_insets(&self, insets: RectInsets) -> Self {
        Rect::new(
            self.origin.x + insets.left,
            self.origin.y + insets.top,
            self.size.width - (insets.left + insets.right),
            self.size.height - (insets.top + insets.bottom),
        )
    }

    pub fn intersects_with(&self, other: Rect) -> bool {
        self.max_x() > other.min_x()
            && self.min_x() < other.max_x()
            && self.max_y() > other.min_y()
            && self.min_y() < other.max_y()
    }

    pub fn area_excluding_rect(&self, exclude_rect: Rect) -> Vec<Self> {
        let mut trimmed_area = *self;
        //println!("{trimmed_area}.exclude({exclude_rect})");
        let mut out = Vec::new();

        if !trimmed_area.intersects_with(exclude_rect) {
            //println!("no intersection, not doing anything");
            return out;
        }

        // Exclude the left edge, resulting in an excluded left area
        let left_overlap = exclude_rect.min_x() - trimmed_area.min_x();
        if left_overlap > 0 {
            //println!("left edge {trimmed_area} overlap {left_overlap}");
            out.push(Rect::from_parts(
                trimmed_area.origin,
                Size::new(left_overlap, trimmed_area.height()),
            ));
            trimmed_area.origin.x += left_overlap;
            trimmed_area.size.width -= left_overlap;
        }

        if !trimmed_area.intersects_with(exclude_rect) {
            return out;
        }

        // Exclude the right edge
        let right_overlap = trimmed_area.max_x() - exclude_rect.max_x();
        if right_overlap > 0 {
            //println!("right edge {trimmed_area} overlap {right_overlap}");
            out.push(Rect::from_parts(
                Point::new(exclude_rect.max_x(), trimmed_area.min_y()),
                Size::new(right_overlap, trimmed_area.height()),
            ));
            trimmed_area.size.width -= right_overlap;
        }

        if !trimmed_area.intersects_with(exclude_rect) {
            return out;
        }

        // Exclude the top, resulting in an excluded bottom area
        //println!("top edge {trimmed_area}");
        let top_overlap = trimmed_area.max_y() - exclude_rect.max_y();
        if top_overlap > 0 {
            //println!("top edge {trimmed_area} overlap {top_overlap}");
            let top_rect = Rect::from_parts(
                //Point::new(trimmed_area.min_x(), trimmed_area.min_y() + top_overlap),
                Point::new(trimmed_area.min_x(), exclude_rect.max_y()),
                //Size::new(trimmed_area.width(), trimmed_area.height() - top_overlap),
                Size::new(trimmed_area.width(), top_overlap),
            );
            //println!("\tGot top rect {top_rect}");
            out.push(top_rect);
            //trimmed_area.origin.y += top_overlap;
            trimmed_area.size.height -= top_overlap;
        }

        if !trimmed_area.intersects_with(exclude_rect) {
            return out;
        }

        // Exclude the bottom, resulting in an included top area
        let bottom_overlap = exclude_rect.min_y() - trimmed_area.min_y();
        //println!("bottom overlap {bottom_overlap}, rect {trimmed_area}");
        if bottom_overlap > 0 {
            //println!("bottom edge {trimmed_area} overlap {bottom_overlap}");
            out.push(Rect::from_parts(
                trimmed_area.origin,
                Size::new(trimmed_area.width(), bottom_overlap),
            ));
            trimmed_area.size.height -= bottom_overlap;
        }

        out
    }

    pub fn area_overlapping_with(&self, rect_to_intersect_with: Rect) -> Option<Self> {
        if !self.intersects_with(rect_to_intersect_with) {
            return None;
        }
        let r1 = *self;
        let r2 = rect_to_intersect_with;

        // Handle when the rectangles are identical, otherwise our assertion below will trigger
        if r1 == r2 {
            return Some(r1);
        }

        let origin = Point::new(max(r1.min_x(), r2.min_x()), max(r1.min_y(), r2.min_y()));
        let bottom_right = Point::new(min(r1.max_x(), r2.max_x()), min(r1.max_y(), r2.max_y()));

        /*
        println!(
            "area_overlapping_with {r1} {r2}, intersects? {:?} intersects2 {:?}",
            r1.intersects_with(r2),
            self.intersects_with(rect_to_intersect_with),
        );
        */
        if !(origin.x < bottom_right.x && origin.y < bottom_right.y) {
            //println!("Rects didn't intersect even though we checked above: {r1} {r2}");
            return None;
        }

        /*
        assert!(
            origin.x < bottom_right.x && origin.y < bottom_right.y,
            "Rects didn't intersect even though we checked above"
        );
        */
        let size = Size::new(bottom_right.x - origin.x, bottom_right.y - origin.y);
        Some(Rect::from_parts(origin, size))
    }

    pub fn union(&self, other: Rect) -> Rect {
        let origin = Point::new(
            min(self.min_x(), other.min_x()),
            min(self.min_y(), other.min_y()),
        );
        Rect::from_parts(
            origin,
            Size::new(
                max(self.max_x(), other.max_x()) - origin.x,
                max(self.max_y(), other.max_y()) - origin.y,
            ),
        )
    }

    pub fn translate_point(&self, p: Point) -> Point {
        p - self.origin
    }
}

impl Display for Rect {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "(({}, {}), ({}, {}))",
            self.min_x(),
            self.min_y(),
            self.width(),
            self.height()
        )
    }
}

impl From<RectU32> for Rect {
    fn from(rect: RectU32) -> Self {
        Self {
            origin: Point::from(rect.origin),
            size: Size::from(&rect.size),
        }
    }
}

#[derive(PartialEq)]
struct TileSegment<'a> {
    viewport_frame: Rect,
    tile_frame: Rect,
    tile: &'a Tile,
}

impl<'a> TileSegment<'a> {
    fn new(viewport_frame: Rect, tile_frame: Rect, tile: &'a Tile) -> Self {
        Self {
            viewport_frame,
            tile_frame,
            tile,
        }
    }
}

impl<'a> Debug for TileSegment<'a> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "TileSegment({} / {} within tile {:?})",
            self.viewport_frame, self.tile_frame, self.tile
        )
    }
}

#[derive(Debug, PartialEq)]
struct TileSegments<'a>(Vec<TileSegment<'a>>);

#[derive(PartialEq)]
struct Tile {
    frame: Rect,
}

impl Tile {
    fn new(frame: Rect) -> Self {
        Self { frame }
    }
    fn tiles_visible_in_viewport(tiles: &Vec<Tile>, viewport_rect: Rect) -> TileSegments {
        TileSegments(
            tiles
                .iter()
                .filter_map(|tile| {
                    println!(
                        "\tChecking for intersection with {viewport_rect} and {}",
                        tile.frame
                    );
                    if let Some(intersection) = viewport_rect.area_overlapping_with(tile.frame) {
                        //println!("\t\t area overlapping {intersection}");
                        let tile_viewport_origin = intersection.origin - viewport_rect.origin;
                        Some(TileSegment::new(
                            Rect::from_parts(tile_viewport_origin, intersection.size),
                            Rect::from_parts(
                                intersection.origin - tile.frame.origin,
                                intersection.size,
                            ),
                            tile,
                        ))
                    } else {
                        None
                    }
                })
                .collect(),
        )
    }
}

impl Debug for Tile {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "Tile({})", self.frame)
    }
}

#[cfg(test)]
mod test {
    use alloc::vec::Vec;

    use crate::{Rect, Tile, TileSegment, TileSegments};
    use std::println;

    #[test]
    fn test_tiles_visible_in_layer() {
        let tiles = vec![
            Tile::new(Rect::new(0, 0, 300, 300)),
            Tile::new(Rect::new(0, 300, 300, 300)),
        ];
        assert_eq!(
            Tile::tiles_visible_in_viewport(&tiles, Rect::new(0, 0, 300, 300)),
            TileSegments(vec![TileSegment::new(
                Rect::new(0, 0, 300, 300),
                Rect::new(0, 0, 300, 300),
                &tiles[0]
            )])
        );
        assert_eq!(
            Tile::tiles_visible_in_viewport(&tiles, Rect::new(0, 10, 300, 300)),
            TileSegments(vec![
                TileSegment::new(
                    Rect::new(0, 0, 300, 290),
                    Rect::new(0, 10, 300, 290),
                    &tiles[0]
                ),
                TileSegment::new(
                    Rect::new(0, 290, 300, 10),
                    Rect::new(0, 0, 300, 10),
                    &tiles[1]
                ),
            ])
        );
    }

    fn test_intersects_with() {
        assert!(!Rect::new(0, 0, 300, 300).intersects_with(Rect::new(0, 300, 300, 300)));
        assert!(!Rect::new(0, 0, 300, 300).intersects_with(Rect::new(0, 300, 300, 300)));
        assert!(Rect::new(0, 0, 300, 300).intersects_with(Rect::new(0, 0, 300, 300)));
    }

    fn test_intersection_with_flipped_ordering(
        r1: Rect,
        r2: Rect,
        expected_intersection: Option<Rect>,
    ) {
        assert_eq!(r1.area_overlapping_with(r2), expected_intersection);
        assert_eq!(r2.area_overlapping_with(r1), expected_intersection);
    }

    #[test]
    fn test_find_intersection() {
        /*
        *----*---------*
        |    |    .    |
        |    |    .    |
        *----*---------*
        */
        test_intersection_with_flipped_ordering(
            Rect::new(0, 0, 100, 100),
            Rect::new(50, 0, 100, 100),
            Some(Rect::new(50, 0, 50, 100)),
        );

        /*
        *----------*
        |          |
        *----------*
        |          |
        | . . . .  |
        |          |
        *----------*
        */
        test_intersection_with_flipped_ordering(
            Rect::new(0, 0, 300, 300),
            Rect::new(0, 150, 300, 300),
            Some(Rect::new(0, 150, 300, 150)),
        );
    }

    #[test]
    fn test_rect_diff() {
        let main = Rect::new(0, 150, 300, 300);
        let exclude = Rect::new(0, 0, 300, 300);
        /*
        ------------------------------
        |                            |
        |                            |
        |                            |
        |                            |
        ------------------------------
        |                            |
        |                            |
        |                            |
        |  -   -   -   -   -   -   - |
        |                            |
        |                            |
        |                            |
        |                            |
        ------------------------------
        */
        assert_eq!(
            main.area_excluding_rect(exclude),
            vec![Rect::new(0, 300, 300, 150)]
        );

        let main = Rect::new(0, 100, 400, 50);
        let exclude = Rect::new(50, 0, 300, 300);
        assert_eq!(
            main.area_excluding_rect(exclude),
            vec![
                // Left edge
                Rect::new(0, 100, 50, 50),
                // Right edge
                Rect::new(350, 100, 50, 50)
            ]
        );

        let main = Rect::new(0, 100, 400, 50);
        let exclude = Rect::new(0, 0, 300, 300);
        assert_eq!(
            main.area_excluding_rect(exclude),
            vec![
                // Right edge
                Rect::new(300, 100, 100, 50)
            ]
        );

        let main = Rect::new(300, 200, 300, 100);
        let exclude = Rect::new(400, 100, 100, 150);
        /*
                           -----------
                           |         |
                     ------|---------|------
                     |     |---------|     |
                     |                     |
                     -----------------------
        */

        assert_eq!(
            main.area_excluding_rect(exclude),
            vec![
                // Left portion
                Rect::new(300, 200, 100, 100),
                // Right portion
                Rect::new(500, 200, 100, 100),
                // Botton portion
                Rect::new(400, 250, 100, 50),
            ]
        );

        let exclude = Rect::new(50, 0, 100, 200);
        let main = Rect::new(0, 50, 200, 50);
        /*
             ----------
             |        |
             |        |
             |        |
             |        |
        --------------------
        |    |        |    |
        |    |        |    |
        |    |        |    |
        --------------------
             |        |
             |        |
             |        |
             |        |
             |        |
             |        |
             |        |
             |        |
             |        |
             ----------
        */
        for a in main.area_excluding_rect(exclude) {
            println!("{a}");
        }
        assert_eq!(
            main.area_excluding_rect(exclude),
            vec![
                // Left portion
                Rect::new(0, 50, 50, 50),
                // Right portion
                Rect::new(150, 50, 50, 50),
            ]
        );

        // Same rectangle as above with the regions flipped
        let main = Rect::new(50, 0, 100, 200);
        let exclude = Rect::new(0, 50, 200, 50);
        /*
             ----------
             | (main) |
             |        |
             |        |
             |        |
        --------------------
        |     (exclude)    |
        |                  |
        |                  |
        --------------------
             |        |
             |        |
             |        |
             |        |
             |        |
             |        |
             |        |
             |        |
             |        |
             ----------
        */
        for a in main.area_excluding_rect(exclude) {
            println!("{a}");
        }
        assert_eq!(
            main.area_excluding_rect(exclude),
            vec![
                // Bottom portion
                Rect::new(50, 100, 100, 100),
                // Top portion
                Rect::new(50, 0, 100, 50),
            ]
        );

        let main = Rect::new(0, 0, 200, 200);
        let exclude = Rect::new(50, 50, 100, 100);
        /*
        ----------------------------
        |       *   (main) *       |
        |       ------------       |
        |       | (exclude)|       |
        |       |          |       |
        |       ------------       |
        |       *          *       |
        ----------------------------
        */
        for a in main.area_excluding_rect(exclude) {
            println!("{a}");
        }
        assert_eq!(
            main.area_excluding_rect(exclude),
            vec![
                // Left portion
                Rect::new(0, 0, 50, 200),
                // Right portion
                Rect::new(150, 0, 50, 200),
                // Bottom portion
                Rect::new(50, 150, 100, 50),
                // Top portion
                Rect::new(50, 0, 100, 50),
            ]
        );

        /*
        ----------
        | (main) |
        |        |
        |        |
        |    ----------
        |    | (excl) |
        |    |        |
        |    |        |
        |    |        |
        |    |        |
        |    |        |
        |    |        |
        -----|        |
             |        |
             |        |
             |        |
             ----------
        */
        let main = Rect::new(200, 200, 100, 130);
        let exclude = Rect::new(250, 250, 100, 130);
        /*
        for a in main.area_excluding_rect(exclude) {
            println!("{a}");
        }
        */
        assert_eq!(
            main.area_excluding_rect(exclude),
            vec![Rect::new(200, 200, 50, 130), Rect::new(250, 200, 50, 50),],
        );

        /*
        ------------------------------
        |                            |
        |                            |
        |              ------------------------------|
        |              |                             |
        ---------------|                             |
                       |                             |
                       |                             |
        |              ------------------------------|
        */
    }
}

// For FFI

#[derive(Debug, Clone, Copy)]
pub struct RectU32 {
    pub origin: PointU32,
    pub size: SizeU32,
}

impl RectU32 {
    pub fn from(rect: Rect) -> Self {
        Self {
            origin: PointU32::from(rect.origin),
            size: SizeU32::from(rect.size),
        }
    }

    pub fn zero() -> Self {
        Self::from(Rect::zero())
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

    fn draw_strip(&self, onto: &mut Box<dyn LikeLayerSlice>, color: Color) {
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

    pub fn draw(
        &self,
        onto: &mut Box<dyn LikeLayerSlice>,
        color: Color,
        thickness: StrokeThickness,
    ) {
        if let StrokeThickness::Width(thickness) = thickness {
            // Special casing for straight lines
            // Horizontal line?
            if self.p1.x == self.p2.x {
                for i in 0..thickness {
                    let mut subline = self.clone();
                    subline.p1.x += i;
                    subline.p2.x += i;
                    subline.draw_strip(onto, color);
                }
            }
            // Vertical line?
            else if self.p1.y == self.p2.y {
                for i in 0..thickness {
                    let mut subline = self.clone();
                    subline.p1.y += i;
                    subline.p2.y += i;
                    subline.draw_strip(onto, color);
                }
            } else {
                let off = (thickness / 2) as isize;
                for i in 0..thickness {
                    let mut subline = self.clone();
                    subline.p1.x += off - i;
                    subline.p2.x += off - i;
                    // PT: This would be more intuitive behavior, but I've disabled it to keep
                    // compatibility with the view-border-inset drawing code.
                    //subline.p1.y += off - i;
                    //subline.p2.y += off - i;
                    subline.draw_strip(onto, color);
                }
            }
        } else {
            self.draw_strip(onto, color);
        }
    }
}
