use core::{
    cell::RefCell,
    cmp::{max, min},
    fmt::Display,
};

use crate::{
    bordered::Bordered,
    font::{draw_char, CHAR_HEIGHT, CHAR_WIDTH, FONT8X8},
    ui_elements::UIElement,
    view::View,
    window::KeyCode,
};
use agx_definitions::{
    Color, Drawable, Layer, LikeLayerSlice, NestedLayerSlice, Point, Rect, RectInsets,
    SingleFramebufferLayer, Size, StrokeThickness,
};
use alloc::boxed::Box;
use alloc::fmt::Debug;
use alloc::vec;
use alloc::{
    rc::{Rc, Weak},
    string::String,
    vec::Vec,
};
use axle_rt::println;
use libgui_derive::{Bordered, Drawable, NestedLayerSlice, UIElement};

struct ExpandingLayerSlice {
    parent: Weak<ExpandingLayer>,
    frame: Rect,
}

impl ExpandingLayerSlice {
    fn new(parent: &Rc<ExpandingLayer>, frame: Rect) -> Self {
        Self {
            parent: Rc::downgrade(parent),
            frame,
        }
    }
}

impl Display for ExpandingLayerSlice {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "<ExpandingLayerSlice {:?}>", self.frame)
    }
}

impl LikeLayerSlice for ExpandingLayerSlice {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn fill_rect(&self, raw_rect: Rect, color: Color, thickness: StrokeThickness) {
        //println!("Fill_rect in expandinglayerslice {raw_rect:?} {color:?}");
        self.parent
            .upgrade()
            .unwrap()
            .fill_rect(raw_rect, color, thickness)
    }

    fn fill(&self, color: Color) {
        panic!("fill")
    }

    fn putpixel(&self, loc: Point, color: Color) {
        self.parent.upgrade().unwrap().putpixel(loc, color)
    }

    fn getpixel(&self, loc: Point) -> Color {
        todo!()
    }

    fn get_slice(&self, rect: Rect) -> Box<dyn LikeLayerSlice> {
        let frame = Rect::from_parts(self.frame.origin + rect.origin, rect.size);
        self.parent.upgrade().unwrap().get_slice_with_frame(frame)
    }

    fn blit(&self, source_layer: &Box<dyn LikeLayerSlice>, source_frame: Rect, dest_origin: Point) {
        println!("Blit to {dest_origin:?}");
        //todo!()
    }

    fn pixel_data(&self) -> Vec<u8> {
        let frame = Rect::from_parts(self.frame.origin, self.frame.size);
        let slice = self.parent.upgrade().unwrap().get_slice_with_frame(frame);
        slice.pixel_data()
    }

    fn blit2(&self, source_layer: &Box<dyn LikeLayerSlice>) {
        todo!()
    }

    fn draw_char(&self, ch: char, draw_loc: Point, draw_color: Color, font_size: Size) {
        /*
        println!(
            "Drawing char! {ch} draw_loc {draw_loc:?} self {}",
            self.frame
        );
        */
        let frame = Rect::from_parts(self.frame.origin + draw_loc, font_size);
        self.parent
            .upgrade()
            .unwrap()
            .draw_char(frame, ch, draw_color);
    }
}

#[derive(PartialEq)]
struct TileLayer {
    frame: Rect,
    inner: RefCell<SingleFramebufferLayer>,
}

impl TileLayer {
    fn new(frame: Rect) -> Self {
        println!("TileLayer {frame:?}");
        Self {
            frame,
            inner: RefCell::new(SingleFramebufferLayer::new(frame.size)),
        }
    }

    fn fill_rect(&self, rect: Rect, color: Color, thickness: StrokeThickness) {
        //fn get_slice(&mut self, rect: Rect) -> Box<dyn LikeLayerSlice> {
        let mut inner = self.inner.borrow_mut();
        //let slice = self.inner.get_slice(rect);
        let slice = inner.get_slice(rect);
        /*
        println!(
            "fill_rect {rect:?} self {:?} inner {:?} slice {:?}",
            self.frame,
            inner.size(),
            slice.frame()
        );
        let mut fb = inner.framebuffer.borrow_mut();
        println!(
            "Framebuffer size {}, inner size {:?}",
            fb.len(),
            inner.size()
        );
        let bpp = 4;
        let parent_bytes_per_row = inner.size().width * bpp;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row);
        let slice_origin_offset = self.frame.origin * bpp_multiple;
        let rect_origin_offset = slice_origin_offset + (rect.origin * bpp_multiple);
        println!("parent_bytes_per_row {parent_bytes_per_row} bpp_multiple {bpp_multiple:?} slice_origin_offset {slice_origin_offset:?} rect_origin_offset {rect_origin_offset:?}");
        for y in 0..rect.height() {
            let row_start = rect_origin_offset.y + (y * parent_bytes_per_row);
            println!("y {y}, row_start {row_start}");
            for x in 0..rect.width() {
                let offset = (rect_origin_offset.x + row_start + (x * bpp)) as usize;
                fb[offset + 0] = color.b;
                fb[offset + 1] = color.g;
                fb[offset + 2] = color.r;
            }
        }
        */
        //println!("slice {:?}", slice.frame());
        slice.fill_rect(rect, color, thickness)
        //slice.fill(color)
    }

    fn putpixel(&self, point: Point, color: Color) {
        let mut inner = self.inner.borrow_mut();
        inner.putpixel(&point, color)
    }
}

#[derive(PartialEq)]
struct TileSegment<'a> {
    viewport_frame: Rect,
    tile_frame: Rect,
    tile: &'a TileLayer,
}

impl<'a> TileSegment<'a> {
    fn new(viewport_frame: Rect, tile_frame: Rect, tile: &'a TileLayer) -> Self {
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
            "TileSegment(VP {} / Tile {} within tile {})",
            self.viewport_frame.origin, self.tile_frame.origin, self.tile.frame,
        )
    }
}

#[derive(Debug, PartialEq)]
struct TileSegments<'a>(Vec<TileSegment<'a>>);

#[derive(PartialEq)]
struct Tile {
    frame: Rect,
}

pub struct ExpandingLayer {
    visible_frame: RefCell<Rect>,
    scroll_offset: RefCell<Point>,
    layers: RefCell<Vec<TileLayer>>,
}

impl ExpandingLayer {
    pub fn new() -> Rc<Self> {
        Rc::new(Self {
            visible_frame: RefCell::new(Rect::zero()),
            scroll_offset: RefCell::new(Point::zero()),
            layers: RefCell::new(Vec::new()),
        })
    }

    pub fn set_visible_frame(&self, visible_frame: Rect) {
        //println!("Setting visible frame to {visible_frame:?}");
        *self.visible_frame.borrow_mut() = visible_frame
    }

    pub fn scroll_offset(&self) -> Point {
        *self.scroll_offset.borrow()
    }

    pub fn set_scroll_offset(&self, scroll_offset: Point) {
        *self.scroll_offset.borrow_mut() = scroll_offset
    }

    fn tiles_visible_in_viewport(tiles: &Vec<TileLayer>, viewport_rect: Rect) -> TileSegments {
        TileSegments(
            tiles
                .iter()
                .filter_map(|tile| {
                    /*
                    println!(
                        "\tChecking for intersection with {viewport_rect} and {}",
                        tile.frame
                    );
                    */
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

    pub fn draw_visible_content_onto(&self, onto: &mut Box<dyn LikeLayerSlice>) {
        let tile_layers = self.layers.borrow();
        // Target slice in local coordinate space
        let onto_frame = Rect::from_parts(Point::zero(), onto.frame().size);
        onto.fill_rect(onto_frame, Color::white(), StrokeThickness::Filled);
        let scroll_offset = *self.scroll_offset.borrow();
        let viewport = Rect::from_parts(scroll_offset, onto.frame().size);
        //println!("draw_visible_content_onto {onto}, viewport {viewport}");

        let visible_tile_segments = Self::tiles_visible_in_viewport(&tile_layers, viewport);
        for visible_segment in visible_tile_segments.0.iter() {
            //println!("Visible segment {visible_segment:?}");
            let dst_viewport_slice = onto.get_slice(visible_segment.viewport_frame);
            let mut inner_tile_layer = visible_segment.tile.inner.borrow_mut();
            let src_tile_slice = inner_tile_layer.get_slice(visible_segment.tile_frame);
            dst_viewport_slice.blit2(&src_tile_slice)
        }
    }

    pub fn get_slice_with_frame(self: &Rc<Self>, frame: Rect) -> Box<dyn LikeLayerSlice> {
        //println!("ExpandingLayer get_slice_with_frame {frame}");
        Box::new(ExpandingLayerSlice::new(self, frame))
    }

    fn rect_contains(&self, a: Rect, b: Rect) -> bool {
        b.max_x() < a.max_x()
            && b.min_x() >= a.min_x()
            && b.max_y() < a.max_y()
            && b.min_y() >= a.min_y()
    }

    fn tile_size() -> Size {
        Size::new(300, 300)
    }

    fn expand_to_contain_rect(&self, rect: Rect) {
        //println!("expand_to_contain_rect {rect}");
        let mut tiles = self.layers.borrow_mut();
        let mut rects_not_contained = vec![rect];
        for tile in tiles.iter() {
            let mut new_rects_not_contained = vec![];
            for uncovered_rect in rects_not_contained.iter() {
                //println!("Checking diff of {uncovered_rect} and {}", tile.frame);
                let mut uncovered_portions_of_rect = uncovered_rect.area_excluding_rect(tile.frame);
                for x in uncovered_portions_of_rect.iter() {
                    //println!("\tRect diff {x}");
                }
                new_rects_not_contained.append(&mut uncovered_portions_of_rect);
            }
            rects_not_contained = new_rects_not_contained;
        }
        //println!("Found region which is not covered:");
        let tile_size = Self::tile_size();
        let mut tiles_to_create = vec![];
        loop {
            if rects_not_contained.len() == 0 {
                break;
            }
            let mut new_rects_not_contained = vec![];
            for uncontained_rect in rects_not_contained.iter() {
                //println!("\tUncontained rect: {uncontained_rect}");
                // Identify the tile(s) covering the rect we need to cover
                //let new_tile_origin = Point::new(uncontained_rect.origin.x );
                let new_tile_origin = Point::new(
                    uncontained_rect.min_x() - (uncontained_rect.min_x() % tile_size.width),
                    uncontained_rect.min_y() - (uncontained_rect.min_y() % tile_size.height),
                );
                let new_tile_frame = Rect::from_parts(new_tile_origin, tile_size);
                //println!("\tCreating tile at {new_tile_origin:?}");
                tiles_to_create.push(new_tile_frame);

                let mut uncovered_portion_of_rect =
                    uncontained_rect.area_excluding_rect(new_tile_frame);
                //println!("\t{uncontained_rect}.area_excluding_rect({new_tile_frame}) = ");
                if uncovered_portion_of_rect.len() == 0 {
                    //println!("\t\t(No area excluding rect)");
                } else {
                    for r in uncovered_portion_of_rect.iter() {
                        //println!("\t\t{r}");
                    }
                }
                //new_rects_not_contained.append(&mut uncovered_portions_of_rect);
                new_rects_not_contained.append(&mut uncovered_portion_of_rect);
            }
            rects_not_contained = new_rects_not_contained;
        }
        // Deduplicate the tiles we need to create
        //HashSet::from_iter(data.iter().cloned())
        //println!("Broke out of loop! Will create tiles:");
        for tile_frame in tiles_to_create.iter() {
            println!("\tCreating tile {tile_frame}");
            let new_tile = TileLayer::new(*tile_frame);
            new_tile.fill_rect(
                Rect::from_parts(Point::zero(), tile_size),
                Color::white(),
                StrokeThickness::Filled,
            );
            new_tile.fill_rect(
                Rect::from_parts(Point::zero(), tile_size),
                Color::green(),
                StrokeThickness::Width(1),
            );
            tiles.push(new_tile);
        }
        //println!("Finished expanding! Tile count: {}", tiles.len());
    }

    pub fn fill_rect(&self, rect: Rect, color: Color, thickness: StrokeThickness) {
        //println!("fill_rect {rect} color {color:?} thickness {thickness:?}");
        self.expand_to_contain_rect(rect);
        //let layers = self.layers_covering_rect(rect);
        let layers = self.layers.borrow();
        for layer in layers.iter() {
            if let Some(visible_portion) = layer.frame.area_overlapping_with(rect) {
                // Also need to translate the rect to the tile's coordinate space!
                let origin_in_tile_coordinate_space = visible_portion.origin - rect.origin;
                let visible_portion_in_tile_coordinate_space =
                    Rect::from_parts(origin_in_tile_coordinate_space, rect.size);
                layer.fill_rect(visible_portion_in_tile_coordinate_space, color, thickness);
            }
        }
    }

    fn putpixel_unchecked(&self, point: Point, color: Color) {
        let layers = self.layers.borrow();
        for layer in layers.iter() {
            if layer.frame.contains(point) {
                layer.putpixel(point, color);
                return;
            }
        }
    }

    fn putpixel(&self, point: Point, color: Color) {
        self.expand_to_contain_rect(Rect::from_parts(point, Size::new(800, 800)));
        self.putpixel_unchecked(point, color);
    }

    fn draw_char(&self, frame: Rect, ch: char, draw_color: Color) {
        let global_frame = frame;
        let visible_frame = self.visible_frame.borrow();
        let frame = Rect::from_parts(frame.origin - visible_frame.origin, frame.size);
        /*
        println!(
            "ExpandingLayer global_frame {global_frame} draw_char({frame}, {ch}, {draw_color:?}"
        );
        */
        self.expand_to_contain_rect(frame);
        // Scale font to the requested size
        let font_size = frame.size;
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
                    //layer.putpixel(*draw_loc + Point::new(draw_x, draw_y), draw_color);
                    self.putpixel_unchecked(frame.origin + Point::new(draw_x, draw_y), draw_color);
                }
            }
        }
    }
}

impl NestedLayerSlice for ExpandingLayer {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        println!("ExpandingLayer ignoring getParent");
        None
    }

    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
        println!("ExpandingLayer ignoring setParent");
        //todo!()
    }

    fn get_slice(&self) -> Box<dyn LikeLayerSlice> {
        panic!("Cannot be used directly?");
        //let content_frame = self.get_content_slice_frame();
        //Box::new(ExpandingLayerSlice::new(self, content_frame))
    }

    fn get_slice_for_render(&self) -> Box<dyn LikeLayerSlice> {
        panic!("Cannot be used here?")
    }
}

impl Drawable for ExpandingLayer {
    fn frame(&self) -> Rect {
        *self.visible_frame.borrow()
    }

    fn content_frame(&self) -> Rect {
        *self.visible_frame.borrow()
    }

    fn draw(&self) {
        println!("ExpandingLayer ignoring draw()")
    }
}

#[derive(Drawable)]
pub struct ScrollView {
    view: Rc<View>,
    pub layer: Rc<ExpandingLayer>,
}

impl ScrollView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(sizer: F) -> Rc<Self> {
        let layer = ExpandingLayer::new();
        let layer_clone = Rc::clone(&layer);
        let sizer = move |v: &View, superview_size: Size| -> Rect {
            let view_frame = sizer(v, superview_size);
            // Update the inner layer
            layer_clone.set_visible_frame(view_frame);
            view_frame
        };
        let view = Rc::new(View::new(Color::new(100, 100, 100), sizer));
        //view.set_border_enabled(false);

        Rc::new(Self { view, layer: layer })
    }
}

impl Bordered for ScrollView {
    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }

    fn draw_inner_content(&self, _outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        self.layer.draw_visible_content_onto(onto)
    }
}

impl NestedLayerSlice for ScrollView {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        self.view.get_parent()
    }

    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
        self.view.set_parent(parent)
    }

    fn get_slice(&self) -> Box<dyn LikeLayerSlice> {
        //println!("!!! Calling get_slice() for ScrollView");
        let content_frame = self.get_content_slice_frame();
        let ret = self.layer.get_slice_with_frame(content_frame);
        //println!("get_slice() got slice {ret}");
        ret
        //self.view.get_slice()
    }

    fn get_slice_for_render(&self) -> Box<dyn LikeLayerSlice> {
        //println!("!!! Calling get_slice_for_render() for ScrollView");
        let ret = self.view.get_slice();
        //println!("get_slice_for_render() got slice {ret}");
        ret
    }
}

impl UIElement for ScrollView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered()
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited()
    }

    fn handle_mouse_moved(&self, mouse_point: Point) {
        self.view.handle_mouse_moved(mouse_point)
    }

    fn handle_left_click(&self, mouse_point: Point) {
        self.view.handle_left_click(mouse_point)
    }

    fn handle_key_pressed(&self, key: KeyCode) {
        self.view.handle_key_pressed(key);
        Bordered::draw(self)
    }

    fn handle_mouse_scrolled(&self, _mouse_point: Point, delta_z: isize) {
        let mut scroll_offset = self.layer.scroll_offset();
        scroll_offset.y += delta_z * 20;
        self.layer.set_scroll_offset(scroll_offset);
        //self.draw()
        Bordered::draw(self)
    }

    fn handle_key_released(&self, key: KeyCode) {
        self.view.handle_key_released(key);
        Bordered::draw(self)
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size)
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}

#[derive(Debug, Copy, Clone)]
pub struct CursorPos(pub usize, pub Point);

#[derive(Debug)]
pub struct DrawnCharacter {
    pub value: char,
    pub pos: Point,
    pub color: Color,
    pub font_size: Size,
}

impl DrawnCharacter {
    fn new(pos: Point, color: Color, ch: char, font_size: Size) -> Self {
        Self {
            value: ch,
            pos,
            color,
            font_size,
        }
    }
}

#[derive(Drawable, NestedLayerSlice, UIElement)]

pub struct TextView {
    pub view: Rc<View>,
    //pub view: Rc<ScrollView>,
    font_size: Size,
    text_insets: RectInsets,
    pub text: RefCell<Vec<DrawnCharacter>>,
    pub cursor_pos: RefCell<CursorPos>,
}

impl TextView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(
        background_color: Color,
        font_size: Size,
        text_insets: RectInsets,
        sizer: F,
    ) -> Rc<Self> {
        //let view = ScrollView::new(sizer);
        //view.view.set_border_enabled(false);
        let view = Rc::new(View::new(background_color, sizer));

        Rc::new(Self {
            view,
            font_size,
            text_insets,
            text: RefCell::new(vec![]),
            cursor_pos: RefCell::new(CursorPos(0, Point::zero())),
        })
    }

    fn font_size(&self) -> Size {
        self.font_size
    }

    pub fn get_text(&self) -> String {
        let mut out = vec![];
        let text = self.text.borrow();
        for ch in text.iter() {
            out.push(ch.value);
        }
        out.iter().collect()
    }

    pub fn text_entry_frame(&self) -> Rect {
        let content_frame = Bordered::content_frame(self);
        content_frame.apply_insets(self.text_insets)
    }

    pub fn draw_char_and_update_cursor(&self, ch: char, color: Color) {
        let mut cursor_pos = self.cursor_pos.borrow_mut();
        //println!("CursorPos {:?}", cursor_pos.1);
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        /*
        println!(
            "Onto {}, Text entry frame {} content frame {} cursor_pos {:?}",
            onto.frame(),
            self.text_entry_frame(),
            Bordered::content_frame(self),
            cursor_pos
        );
        */
        let font_size = self.font_size();
        onto.draw_char(ch, cursor_pos.1, color, font_size);
        //draw_char(onto, ch, &cursor_pos.1, color, &font_size);
        //draw_char(onto, ch, &cursor_pos.1, Color::black(), &font_size);
        /*
        onto.fill_rect(
            Rect::new(
                cursor_pos.1.x,
                cursor_pos.1.y,
                font_size.width,
                font_size.height,
            ),
            Color::green(),
            StrokeThickness::Filled,
        );
        */

        // TODO(PT): This is not correct if we're not inserting at the end
        // We'll need to adjust the positions of every character that comes after this one
        cursor_pos.0 += 1;
        let mut cursor_pos = &mut cursor_pos.1;

        self.text
            .borrow_mut()
            .push(DrawnCharacter::new(*cursor_pos, color, ch, font_size));

        if ch == '\n' || cursor_pos.x + (font_size.width * 2) >= onto.frame().width() {
            cursor_pos.x = 0;
            cursor_pos.y += font_size.height + 2;
        } else {
            cursor_pos.x += font_size.width;
        }
        Bordered::draw(self)
    }

    pub fn erase_char_and_update_cursor(&self) {
        let mut cursor_pos = self.cursor_pos.borrow_mut();
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        let font_size = self.font_size();

        if cursor_pos.0 == 0 {
            println!("Cannot delete with no text!");
            return;
        }

        let mut text = self.text.borrow_mut();
        let removed_char = text.remove(cursor_pos.0 - 1);

        cursor_pos.0 -= 1;
        let cursor_pos = &mut cursor_pos.1;
        *cursor_pos = removed_char.pos;

        //println!("Removing char {removed_char:?}");

        onto.fill_rect(
            Rect::from_parts(removed_char.pos, font_size),
            Color::white(),
            StrokeThickness::Filled,
        );
    }

    pub fn clear(&self) {
        *self.text.borrow_mut() = vec![];
        *self.cursor_pos.borrow_mut() = CursorPos(0, Point::zero());
        Bordered::draw(&*self.view);
    }
}

impl Bordered for TextView {
    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        self.view.draw_inner_content(outer_frame, onto);
        // PT: No need to use text_entry_frame() as `onto` is already in the content frame coordinate space
        let text_entry_slice = onto.get_slice(
            Rect::from_parts(Point::zero(), onto.frame().size).apply_insets(self.text_insets),
        );
        // TODO(PT): Only draw what's visible?
        let rendered_text = self.text.borrow();
        for drawn_char in rendered_text.iter() {
            text_entry_slice.draw_char(
                drawn_char.value,
                drawn_char.pos,
                drawn_char.color,
                drawn_char.font_size,
            );
        }
        //println!("Finished call to draw_inner_content()");
    }
}
