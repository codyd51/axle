use core::{
    cell::RefCell,
    cmp::{max, min},
    fmt::Display,
};

use crate::window_events::KeyCode;
use crate::{
    bordered::Bordered,
    font::{draw_char, CHAR_HEIGHT, CHAR_WIDTH, FONT8X8},
    println,
    ui_elements::UIElement,
    view::View,
};
use agx_definitions::{
    Color, Drawable, Layer, LikeLayerSlice, NestedLayerSlice, Point, Rect, RectInsets,
    SingleFramebufferLayer, Size, StrokeThickness,
};
use alloc::boxed::Box;
use alloc::collections::BTreeSet;
use alloc::fmt::Debug;
use alloc::vec;
use alloc::{
    rc::{Rc, Weak},
    string::String,
    vec::Vec,
};
use core::fmt::Formatter;
use libgui_derive::{Bordered, Drawable, NestedLayerSlice, UIElement};
use rand::Rng;

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

    fn putpixel_unchecked(&self, loc: Point, color: Color) {
        self.parent
            .upgrade()
            .unwrap()
            .putpixel_unchecked(loc, color)
    }
}

impl Display for ExpandingLayerSlice {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "<ExpandingLayerSlice {}>", self.frame)
    }
}

impl LikeLayerSlice for ExpandingLayerSlice {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn fill_rect(&self, raw_rect: Rect, color: Color, thickness: StrokeThickness) {
        //println!("Fill_rect in expandinglayerslice {raw_rect:?} {color:?}");
        self.parent.upgrade().unwrap().fill_rect(
            Rect::from_parts(self.frame.origin + raw_rect.origin, raw_rect.size),
            color,
            thickness,
        )
    }

    fn fill(&self, color: Color) {
        self.fill_rect(
            Rect::from_parts(Point::zero(), self.frame.size),
            color,
            StrokeThickness::Filled,
        )
    }

    fn putpixel(&self, loc: Point, color: Color) {
        self.parent.upgrade().unwrap().putpixel(loc, color)
    }

    fn getpixel(&self, loc: Point) -> Color {
        todo!()
    }

    fn get_slice(&self, rect: Rect) -> Box<dyn LikeLayerSlice> {
        //println!("(LikeLayerSlice for ExpandingLayerSlice({})::get_slice({rect})", self.frame);
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
        /*
        assert!(self.frame().size == source_layer.frame().size);
        //let pixel_data = source_layer.pixel_data();
        for y in 0..self.frame().height() {
            for x in 0..self.frame().width() {
                let p = Point::new(x, y);
                self.putpixel_unchecked(p, source_layer.getpixel(p));
            }
        }

         */
        self.parent.upgrade().unwrap().fill_rect_unchecked(
            Rect::from_parts(Point::zero(), Size::new(100, 100)),
            Color::blue(),
            StrokeThickness::Filled,
        );
    }

    fn draw_char(&self, ch: char, draw_loc: Point, draw_color: Color, font_size: Size) {
        //println!("ExpandingLayerSlice({}).draw_char({ch}, {draw_loc})", self.frame);
        let frame = Rect::from_parts(self.frame.origin + draw_loc, font_size);
        self.parent
            .upgrade()
            .unwrap()
            .draw_char(frame, ch, draw_color);
    }

    fn get_pixel_row(&self, y: usize) -> Vec<u8> {
        todo!()
    }
}

#[derive(PartialEq)]
struct TileLayer {
    frame: Rect,
    inner: RefCell<SingleFramebufferLayer>,
}

impl TileLayer {
    fn new(frame: Rect) -> Self {
        println!("TileLayer.new({frame})");
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
            "\tTileLayer.fill_rect {rect} self_frame {} inner {} slice {} color {color} thickness {thickness:?}",
            self.frame,
            inner.size(),
            slice.frame()
        );
        */
        /*
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
        slice.fill_rect(
            Rect::from_parts(Point::zero(), slice.frame().size),
            color,
            thickness,
        );
        //slice.fill_rect(rect, color, thickness)
        //slice.fill(color)
    }

    fn putpixel(&self, point: Point, color: Color) {
        let mut inner = self.inner.borrow_mut();
        inner.putpixel(&point, color)
    }
}

impl Display for TileLayer {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "<TileLayer {}>", self.frame)
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
    pub scroll_offset: RefCell<Point>,
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
        //let viewport = viewport.inset_by_insets(RectInsets::new(11, 11, 11, 11));
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
        //println!("ExpandingLayer.get_slice_with_frame {frame}");
        Box::new(ExpandingLayerSlice::new(self, frame))
    }

    fn tile_size() -> Size {
        Size::new(300, 300)
    }

    fn round_to_tile_boundary(val: isize, tile_length: isize) -> isize {
        let remainder = val % tile_length;
        if remainder == 0 {
            return val;
        }
        if val < 0 {
            return val - (tile_length - remainder.abs());
        }
        val - remainder
    }

    fn container_tile_origin_for_point(point: Point, tile_size: Size) -> Point {
        Point::new(
            Self::round_to_tile_boundary(point.x, tile_size.width),
            Self::round_to_tile_boundary(point.y, tile_size.height),
        )
    }

    fn rects_not_contained_in_current_tiles(&self, rect: Rect) -> Vec<Rect> {
        let mut tiles = self.layers.borrow_mut();
        let mut rects_not_contained = vec![rect];
        for tile in tiles.iter() {
            //println!("\tExisting tile: {}", tile.frame);
            let mut new_rects_not_contained = vec![];
            for uncovered_rect in rects_not_contained.iter() {
                //println!("\tChecking diff of {uncovered_rect} and {}", tile.frame);
                if !uncovered_rect.intersects_with(tile.frame) {
                    new_rects_not_contained.push(*uncovered_rect);
                } else {
                    let mut uncovered_portions_of_rect =
                        uncovered_rect.area_excluding_rect(tile.frame);
                    new_rects_not_contained.append(&mut uncovered_portions_of_rect);
                }
            }
            rects_not_contained = new_rects_not_contained;
        }
        rects_not_contained
    }

    fn tiles_needed_to_cover_rect(rect: Rect, tile_size: Size) -> Vec<Rect> {
        let mut new_tiles = BTreeSet::new();

        let mut rects_not_contained = vec![rect];
        loop {
            if rects_not_contained.len() == 0 {
                break;
            }
            let mut new_rects_not_contained = vec![];
            for uncontained_rect in rects_not_contained.iter() {
                let tile_origin =
                    Self::container_tile_origin_for_point(uncontained_rect.origin, tile_size);
                let tile_frame = Rect::from_parts(tile_origin, tile_size);
                new_tiles.insert(tile_frame);

                let mut uncovered_portion_of_rect =
                    uncontained_rect.area_excluding_rect(tile_frame);
                new_rects_not_contained.append(&mut uncovered_portion_of_rect);
            }
            rects_not_contained = new_rects_not_contained;
        }

        Vec::from_iter(new_tiles.into_iter())
    }

    fn expand_to_contain_rect(&self, rect: Rect) {
        let tile_size = Self::tile_size();
        // Cull down to the rects that are not covered by our existing tiles
        let rects_not_contained = self.rects_not_contained_in_current_tiles(rect);
        /*
        if rects_not_contained.len() > 0 {
            println!("ExpandingLayer.expand_to_contain_rect({rect})");
            let layers = self.layers.borrow();
            for layer in layers.iter() {
                println!("\tExisting tile @ {}", layer.frame);
            }
            println!("\tRects not already contained in existing tiles:");
        }
        for r in rects_not_contained.iter() {
            println!("\t\t{r}");
        }
        */
        // Find the tiles we'll need to create to cover each outstanding rect
        let existing_tile_frames = {
            let tile_layers = self.layers.borrow();
            BTreeSet::from_iter(tile_layers.iter().map(|t| t.frame))
        };
        // Start off with the existing tile frames so we don't add a tile that we've already created
        let mut total_needed_tiles = existing_tile_frames.clone();

        for r in rects_not_contained.iter() {
            let tiles_needed_to_cover_rect = Self::tiles_needed_to_cover_rect(*r, tile_size);
            total_needed_tiles.append(&mut BTreeSet::from_iter(
                tiles_needed_to_cover_rect.into_iter(),
            ));
        }

        // Subtract our existing tiles to find the new tiles we need to create
        let new_tile_frames: Vec<Rect> = total_needed_tiles
            .difference(&existing_tile_frames)
            .cloned()
            .collect();

        let mut tiles = self.layers.borrow_mut();
        for new_tile_frame in new_tile_frames.iter() {
            //println!("\tCreating tile {new_tile_frame}");
            let new_tile = TileLayer::new(*new_tile_frame);
            new_tile.fill_rect(
                Rect::from_parts(Point::zero(), tile_size),
                Color::white(),
                StrokeThickness::Filled,
            );
            /*
            new_tile.fill_rect(
                Rect::from_parts(Point::zero(), tile_size),
                Color::green(),
                StrokeThickness::Width(1),
            );
            */
            //println!("\tFinished drawing background of tile");
            tiles.push(new_tile);
        }
    }

    fn fill_rect_unchecked(&self, rect: Rect, color: Color, thickness: StrokeThickness) {
        //println!("ExpandingLayer.fill_rect_unchecked({rect} color {color} thickness {thickness:?})");
        //let layers = self.layers_covering_rect(rect);
        let layers = self.layers.borrow();
        for layer in layers.iter() {
            if let Some(visible_portion) = layer.frame.area_overlapping_with(rect) {
                //println!("\tFound overlap of {visible_portion} in tile {layer}");
                // Also need to translate the rect to the tile's coordinate space!
                let origin_in_tile_coordinate_space = visible_portion.origin - layer.frame.origin;
                let visible_portion_in_tile_coordinate_space =
                    Rect::from_parts(origin_in_tile_coordinate_space, visible_portion.size);
                //println!("\torigin_in_tile_coordinate_space {origin_in_tile_coordinate_space}, visible_portion {visible_portion_in_tile_coordinate_space}");
                layer.fill_rect(visible_portion_in_tile_coordinate_space, color, thickness);
            }
        }
    }

    pub fn fill_rect(&self, rect: Rect, color: Color, thickness: StrokeThickness) {
        //println!("ExpandingLayer.fill_rect({rect} color {color} thickness {thickness:?})");
        self.expand_to_contain_rect(rect);
        self.fill_rect_unchecked(rect, color, thickness)
    }

    fn putpixel_unchecked(&self, point: Point, color: Color) {
        let layers = self.layers.borrow();
        for layer in layers.iter() {
            if layer.frame.contains(point) {
                let translated_point = point - layer.frame.origin;
                layer.putpixel(translated_point, color);
                return;
            }
        }
    }

    fn putpixel(&self, point: Point, color: Color) {
        self.expand_to_contain_rect(Rect::from_parts(point, Size::new(800, 800)));
        self.putpixel_unchecked(point, color);
    }

    pub fn draw_char(&self, frame: Rect, ch: char, draw_color: Color) {
        let global_frame = frame;
        let visible_frame = self.visible_frame.borrow();
        //let frame = Rect::from_parts(frame.origin - visible_frame.origin, frame.size);
        //println!("ExpandingLayer.draw_char({frame})");
        /*
        println!(
            "ExpandingLayer global_frame {global_frame} draw_char({frame}, {ch}, {draw_color:?}"
        );
        */
        self.expand_to_contain_rect(frame);
        //println!("Finished expanding to contain {frame}");
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

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        //println!("ScrollView.draw_inner_content({outer_frame}, {onto}");
        // We have a different layer from our parent, so need to exclude the border insets here
        /*
        let border_insets = self.border_insets();
        let onto_frame = onto.frame();
        let onto = &mut onto.get_slice(
            Rect::from_parts(
                Point::new(border_insets.left, border_insets.top),
                Size::new(
                    onto_frame.width() - (border_insets.left + border_insets.right),
                    onto_frame.height() - (border_insets.top + border_insets.bottom
                    )
                )
            )
        );
        */
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

    fn get_content_slice_frame(&self) -> Rect {
        //println!("(NestedLayerSlice for ScrollView).get_content_slice_frame()");
        Rect::with_size(self.view.frame().size)
    }

    fn get_slice(&self) -> Box<dyn LikeLayerSlice> {
        //println!("!!! Calling get_slice() for ScrollView");
        let content_frame = self.get_content_slice_frame();
        //println!("(NestedLayerSlice for ScrollView)::get_slice(), content_frame {content_frame})");
        let ret = self.layer.get_slice_with_frame(content_frame);
        //println!("ScrollView.get_slice() got slice {ret}");
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
        //println!("ScrollView.handle_mouse_scrolled({delta_z})");
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

#[derive(Debug, Copy, Clone)]
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

impl Display for DrawnCharacter {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "<DrawnChar '{}' @ {}>",
            self.value,
            Rect::from_parts(self.pos, self.font_size)
        )
    }
}

#[derive(Drawable, NestedLayerSlice, UIElement)]
pub struct TextView {
    pub view: Rc<ScrollView>,
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
        let view = ScrollView::new(sizer);
        //let view = Rc::new(View::new(background_color, sizer));

        Rc::new(Self {
            view,
            font_size,
            text_insets,
            text: RefCell::new(vec![]),
            cursor_pos: RefCell::new(CursorPos(0, Point::zero())),
        })
    }

    pub fn font_size(&self) -> Size {
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
        let content_frame = Rect::with_size(Bordered::content_frame(self).size);
        content_frame.apply_insets(self.text_insets)
    }

    pub fn draw_char_and_update_cursor3(&self, ch: char, color: Color) {
        let content_slice_frame = self.view.get_content_slice_frame();
        println!("Content slice frame {content_slice_frame}");
    }

    fn next_cursor_pos_for_char(
        cursor_pos: Point,
        ch: char,
        font_size: Size,
        onto: &Box<dyn LikeLayerSlice>,
    ) -> Point {
        let mut cursor_pos = cursor_pos;
        if ch == '\n' || cursor_pos.x + (font_size.width * 2) >= onto.frame().width() {
            cursor_pos.x = 0;
            cursor_pos.y += font_size.height + 2;
        } else {
            cursor_pos.x += font_size.width;
        }
        cursor_pos
    }

    pub fn draw_char_and_update_cursor(&self, ch: char, color: Color) {
        let mut cursor_pos = self.cursor_pos.borrow_mut();
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        let font_size = self.font_size();

        let mut text = self.text.borrow_mut();
        let is_inserting_at_end = cursor_pos.0 == text.len();
        //println!("draw_char_and_update({ch}) is_insert_at_end? {is_inserting_at_end}, {cursor_pos:?} {}", text.len());

        if !is_inserting_at_end {
            // If we just inserted a newline, we need to adjust our cursor position
            let mut cursor_point =
                Self::next_cursor_pos_for_char(cursor_pos.1, ch, font_size, onto);
            //println!("Base cursor for later chars: {cursor_point}");
            for drawn_ch in text[cursor_pos.0..].iter_mut() {
                //println!("\tFound later {}, originally placed at {}", drawn_ch.value, drawn_ch.pos);
                // Cover up this as we'll redraw it somewhere else
                onto.fill_rect(
                    Rect::from_parts(drawn_ch.pos, font_size),
                    Color::white(),
                    StrokeThickness::Filled,
                );
                drawn_ch.pos = cursor_point;
                //println!("\tShifted to {}", drawn_ch.pos);
                cursor_point =
                    Self::next_cursor_pos_for_char(cursor_point, drawn_ch.value, font_size, onto);
            }
        }

        onto.draw_char(ch, cursor_pos.1, color, font_size);

        // TODO(PT): This is not correct if we're not inserting at the end
        // We'll need to adjust the positions of every character that comes after this one
        let insertion_point = cursor_pos.0;
        cursor_pos.0 += 1;
        let mut cursor_point = &mut cursor_pos.1;

        // TODO(PT): If not inserting at the end, we need to move everything along and insert at an index
        let draw_desc = DrawnCharacter::new(*cursor_point, color, ch, font_size);
        if is_inserting_at_end {
            text.push(draw_desc);
        } else {
            println!("Inserting at {insertion_point}: {draw_desc:?}");
            text.insert(insertion_point, draw_desc);
        }
        *cursor_point = Self::next_cursor_pos_for_char(*cursor_point, ch, font_size, onto);
        //Bordered::draw(self)
    }

    pub fn erase_char_and_update_cursor(&self) {
        let mut cursor_pos = self.cursor_pos.borrow_mut();
        if cursor_pos.0 == 0 {
            //println!("Cannot delete with no text!");
            return;
        }

        let mut text = self.text.borrow_mut();

        let is_deleting_from_end = cursor_pos.0 == text.len();
        //println!("Erasing! From end? {is_deleting_from_end}");

        let mut chars_after_delete = vec![];
        for drawn_ch in text[cursor_pos.0 - 1..].iter() {
            chars_after_delete.push(*drawn_ch);
        }

        let removed_char = text.remove(cursor_pos.0 - 1);

        // Cover up the deleted character
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        let font_size = self.font_size;
        onto.fill_rect(
            Rect::from_parts(removed_char.pos, font_size),
            Color::white(),
            StrokeThickness::Filled,
        );

        cursor_pos.0 -= 1;

        if cursor_pos.0 == 0 {
            cursor_pos.1 = Point::zero();
        } else {
            let prev = text[cursor_pos.0 - 1];
            cursor_pos.1 =
                Self::next_cursor_pos_for_char(prev.pos, prev.value, prev.font_size, onto);
        }

        if !is_deleting_from_end {
            // Shift back the characters in front of this one
            //println!("{chars_after_delete:?}");
            for (i, drawn_ch) in text[cursor_pos.0..].iter_mut().enumerate() {
                let prev = chars_after_delete[i];
                // Cover up its previous position
                onto.fill_rect(
                    Rect::from_parts(drawn_ch.pos, drawn_ch.font_size),
                    Color::white(),
                    StrokeThickness::Filled,
                );
                //println!("Shifting back {} from {} to {} ({})", drawn_ch.value, drawn_ch.pos, prev.pos, prev);
                //cursor_pos.1 = Self::next_cursor_pos_for_char(prev.pos, prev.value, prev.font_size, onto);
                drawn_ch.pos = prev.pos;
                onto.draw_char(
                    drawn_ch.value,
                    drawn_ch.pos,
                    drawn_ch.color,
                    drawn_ch.font_size,
                );

                if prev.value == '\n' {
                    let mut next_cursor_pos = drawn_ch.pos;
                    for next_ch in chars_after_delete[i + 1..].iter_mut() {
                        //print!("Shifting forward {next_ch} to ");
                        next_ch.pos = Self::next_cursor_pos_for_char(
                            next_cursor_pos,
                            next_ch.value,
                            next_ch.font_size,
                            onto,
                        );
                        next_cursor_pos = next_ch.pos;
                        //println!("{}", next_ch.pos);
                    }
                }
            }
        }
        //println!("Removing char {removed_char:?}");
    }

    pub fn set_cursor_pos(&self, cursor: CursorPos) {
        let mut cursor_pos = self.cursor_pos.borrow_mut();
        *cursor_pos = cursor;
    }

    pub fn is_inserting_at_end(&self) -> bool {
        let mut cursor_pos = self.cursor_pos.borrow();
        let ret = cursor_pos.0 == self.text.borrow().len();
        //println!("{} {}", cursor_pos.0, self.text.borrow().len());
        ret
    }

    pub fn draw_char_with_description(&self, char_description: DrawnCharacter) {
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        onto.draw_char(
            char_description.value,
            char_description.pos,
            char_description.color,
            self.font_size,
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
        //println!("draw_inner_content({outer_frame}, {onto})");
        self.view.draw_inner_content(outer_frame, onto);
        /*
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
        */
        //println!("Finished call to draw_inner_content()");
    }
}

#[cfg(test)]
mod test {
    use alloc::vec::Vec;

    //use crate::{Rect, Tile, TileSegment, TileSegments};
    use crate::text_view::{ExpandingLayer, TileLayer};
    use agx_definitions::{Point, Rect, Size};
    use std::println;

    fn expect_insert_results_in_tile_frames_with_existing_tiles(
        rect: Rect,
        expected_tile_frames: Vec<Rect>,
        existing_tile_frames: Vec<Rect>,
    ) {
        let layer = ExpandingLayer::new();
        {
            // Set up the tiles that should already be present
            let mut existing_tiles = layer.layers.borrow_mut();
            for existing_tile_frame in existing_tile_frames.iter() {
                existing_tiles.push(TileLayer::new(*existing_tile_frame));
            }
        }
        layer.expand_to_contain_rect(rect);
        let tile_frames: Vec<Rect> = layer
            .layers
            .borrow()
            .iter()
            .map(|tile| tile.frame)
            .collect();
        assert_eq!(tile_frames, expected_tile_frames);
    }

    fn expect_insert_results_in_tile_frames(rect: Rect, expected_tile_frames: Vec<Rect>) {
        expect_insert_results_in_tile_frames_with_existing_tiles(rect, expected_tile_frames, vec![])
    }

    #[test]
    fn test_container_tile_for_point() {
        let tile_size = Size::new(100, 100);
    }

    #[test]
    fn test_round_to_tile_boundary() {
        assert_eq!(ExpandingLayer::round_to_tile_boundary(0, 100), 0);
        assert_eq!(ExpandingLayer::round_to_tile_boundary(50, 100), 0);
        assert_eq!(ExpandingLayer::round_to_tile_boundary(100, 100), 100);
        assert_eq!(ExpandingLayer::round_to_tile_boundary(150, 100), 100);
        assert_eq!(ExpandingLayer::round_to_tile_boundary(-5, 100), -100);
        assert_eq!(ExpandingLayer::round_to_tile_boundary(-150, 100), -200);
        assert_eq!(ExpandingLayer::round_to_tile_boundary(-200, 100), -200);
    }

    #[test]
    fn test_container_tile_origin_for_point() {
        let tile_size = Size::new(100, 100);
        assert_eq!(
            ExpandingLayer::container_tile_origin_for_point(Point::new(0, 0), tile_size),
            Point::new(0, 0)
        );
        assert_eq!(
            ExpandingLayer::container_tile_origin_for_point(Point::new(50, 0), tile_size),
            Point::new(0, 0)
        );
        assert_eq!(
            ExpandingLayer::container_tile_origin_for_point(Point::new(50, 50), tile_size),
            Point::new(0, 0)
        );
        assert_eq!(
            ExpandingLayer::container_tile_origin_for_point(Point::new(150, 0), tile_size),
            Point::new(100, 0)
        );
        assert_eq!(
            ExpandingLayer::container_tile_origin_for_point(Point::new(150, 150), tile_size),
            Point::new(100, 100)
        );
        assert_eq!(
            ExpandingLayer::container_tile_origin_for_point(Point::new(-50, -50), tile_size),
            Point::new(-100, -100)
        );
        assert_eq!(
            ExpandingLayer::container_tile_origin_for_point(Point::new(-120, -70), tile_size),
            Point::new(-200, -100)
        );
        assert_eq!(
            ExpandingLayer::container_tile_origin_for_point(Point::new(-205, -205), tile_size),
            Point::new(-300, -300)
        );
    }

    #[test]
    fn test_tiles_needed_to_cover_rect() {
        let tile_size = Size::new(100, 100);
        assert_eq!(
            ExpandingLayer::tiles_needed_to_cover_rect(Rect::new(75, 0, 50, 0), tile_size),
            vec![Rect::new(0, 0, 100, 100), Rect::new(100, 0, 100, 100)]
        );
        assert_eq!(
            ExpandingLayer::tiles_needed_to_cover_rect(Rect::new(0, 0, 100, 100), tile_size),
            vec![Rect::new(0, 0, 100, 100)]
        );
        assert_eq!(
            ExpandingLayer::tiles_needed_to_cover_rect(Rect::new(0, 0, 101, 101), tile_size),
            vec![
                Rect::new(0, 0, 100, 100),
                Rect::new(0, 100, 100, 100),
                Rect::new(100, 0, 100, 100),
                Rect::new(100, 100, 100, 100),
            ]
        );
        assert_eq!(
            ExpandingLayer::tiles_needed_to_cover_rect(Rect::new(150, 150, 1, 1), tile_size),
            vec![Rect::new(100, 100, 100, 100)]
        );
        assert_eq!(
            ExpandingLayer::tiles_needed_to_cover_rect(Rect::new(-1, -1, 2, 2), tile_size),
            vec![
                Rect::new(-100, -100, 100, 100),
                Rect::new(-100, 0, 100, 100),
                Rect::new(0, -100, 100, 100),
                Rect::new(0, 0, 100, 100),
            ]
        );
    }

    #[test]
    fn test_expand_to_contain_rect() {
        /*
        expect_insert_results_in_tile_frames(
            Rect::new(0, 0, 50, 50),
            vec![Rect::new(0, 0, 100, 100)]
        );
        expect_insert_results_in_tile_frames(
            Rect::new(0, 0, 100, 100),
            vec![Rect::new(0, 0, 100, 100)]
        );
        expect_insert_results_in_tile_frames(
            Rect::new(0, 0, 101, 100),
            vec![
                Rect::new(0, 0, 100, 100),
                Rect::new(100, 0, 100, 100),
            ]
        );
        expect_insert_results_in_tile_frames(
            Rect::new(0, 0, 101, 101),
            vec![
                Rect::new(0, 0, 100, 100),
                Rect::new(0, 100, 100, 100),
                Rect::new(100, 0, 100, 100),
                Rect::new(100, 100, 100, 100),
            ]
        );
        expect_insert_results_in_tile_frames(
            Rect::new(0,-5, 10, 10),
            vec![
                Rect::new(0, -100, 100, 100),
                Rect::new(0, 0, 100, 100),
            ]
        );
        expect_insert_results_in_tile_frames_with_existing_tiles(
            Rect::new(96, 0, 16, 16),
            vec![
                Rect::new(0, -100, 100, 100),
                Rect::new(0, 0, 100, 100),
                Rect::new(100, 0, 100, 100),
            ],
            vec![
                Rect::new(0, -100, 100, 100),
                Rect::new(0, 0, 100, 100),
            ],
        );
        */
        expect_insert_results_in_tile_frames_with_existing_tiles(
            Rect::new(113, -1, 1, 14),
            vec![
                Rect::new(0, -100, 100, 100),
                Rect::new(0, 0, 100, 100),
                Rect::new(100, 0, 100, 100),
                Rect::new(100, -100, 100, 100),
            ],
            vec![
                Rect::new(0, -100, 100, 100),
                Rect::new(0, 0, 100, 100),
                Rect::new(100, 0, 100, 100),
            ],
        );
    }
}
