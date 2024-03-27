use core::{cell::RefCell, fmt::Display};

use crate::bordered::{
    compute_inner_margin_and_content_frames, draw_border_with_insets, draw_outer_mouse_highlight,
};
use crate::window_events::KeyCode;
use crate::{
    bordered::Bordered,
    font::{CHAR_HEIGHT, CHAR_WIDTH, FONT8X8},
    ui_elements::UIElement,
    view::View,
};
use agx_definitions::{
    scanline_compute_fill_lines_from_edges, Color, Drawable, FillMode, Layer, LikeLayerSlice, Line,
    LineF64, NestedLayerSlice, PixelByteLayout, Point, PointF64, Polygon, PolygonStack, Rect,
    RectInsets, SingleFramebufferLayer, Size, StrokeThickness,
};
use alloc::boxed::Box;
use alloc::collections::BTreeSet;
use alloc::fmt::Debug;
use alloc::vec;
use alloc::{
    rc::{Rc, Weak},
    vec::Vec,
};
use core::cmp::{max, min};
use core::fmt::Formatter;
use libgui_derive::Drawable;
use num_traits::Float;

struct ExpandingLayerSlice {
    parent: Weak<ExpandingLayer>,
    frame: Rect,
    global_origin: Point,
}

impl ExpandingLayerSlice {
    fn new(parent: &Rc<ExpandingLayer>, frame: Rect, global_origin: Point) -> Self {
        Self {
            parent: Rc::downgrade(parent),
            frame,
            global_origin,
        }
    }
}

impl Display for ExpandingLayerSlice {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
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
            Rect::from_parts(self.global_origin + raw_rect.origin, raw_rect.size),
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

    fn fill_polygon_stack(&self, polygon_stack: &PolygonStack, color: Color, fill_mode: FillMode) {
        // TODO(PT): It appears there's a rendering bug while rendering a polygon that crosses between two tiles?
        //println!("Fill_polygon_stack!!!!");
        let mut slice = self.parent.upgrade().unwrap();
        let mut bounding_box = polygon_stack.bounding_box();
        //println!("Polygon stack bounding box {bounding_box:?}");
        // Adjust for this slice's offset
        bounding_box.origin.x += self.global_origin.x as f64;
        bounding_box.origin.y += self.global_origin.y as f64;

        slice.expand_to_contain_rect(Rect::from(bounding_box));

        match fill_mode {
            FillMode::Filled => {
                for line in
                scanline_compute_fill_lines_from_edges(&polygon_stack.lines()).into_iter()
                {
                    // Horizontal line?
                    if line.p1.y.round() == line.p2.y.round() {
                        let line = Line::from(line);
                        slice.fill_rect_unchecked(
                            Rect::from_parts(
                                self.frame.origin + line.p1,
                                Size::new(line.p2.x - line.p1.x, 1),
                            ),
                            color,
                            StrokeThickness::Filled,
                        )
                    } else {
                        for (x, y) in line.as_inclusive_bresenham_iterator() {
                            // We've guaranteed that we'll have tiles to cover all the lines in the polygon above
                            slice.putpixel_unchecked(
                                Point::new(x + self.frame.origin.x, y + self.frame.origin.y),
                                //Point::new(x, y),
                                color,
                            );
                        }
                    }
                }
            }
            FillMode::Outline => {
                let lines = polygon_stack.lines();
                for line in lines.iter() {
                    //line.draw(&mut slice, color);
                    let line = LineF64::new(
                        line.p1 + PointF64::from(self.frame.origin),
                        line.p2 + PointF64::from(self.frame.origin),
                    );
                    for (x, y) in line.as_inclusive_bresenham_iterator() {
                        slice.putpixel_unchecked(Point::new(x, y), color);
                    }
                    //line.draw(self, color);
                }
            }
        }
        // 12s with 8 alphabet repeats, putpixel_unchecked
        // 69s with 8 alphabet repeats, putpixel
    }

    fn putpixel(&self, loc: Point, color: Color) {
        self.parent
            .upgrade()
            .unwrap()
            .putpixel(self.global_origin + loc, color)
    }

    fn getpixel(&self, _loc: Point) -> Color {
        todo!()
    }

    fn get_slice(&self, rect: Rect) -> Box<dyn LikeLayerSlice> {
        /*
        println!(
            "(LikeLayerSlice for ExpandingLayerSlice({})::get_slice({rect}), global_origin = {}",
            self.frame, self.global_origin,
        );
        */
        let frame = Rect::from_parts(self.global_origin + rect.origin, rect.size);
        self.parent.upgrade().unwrap().get_slice_with_frame(frame)
    }

    fn blit(
        &self,
        _source_layer: &Box<dyn LikeLayerSlice>,
        _source_frame: Rect,
        dest_origin: Point,
    ) {
        //println!("Blit to {dest_origin:?}");
        //todo!()
    }

    fn pixel_data(&self) -> Vec<u8> {
        let frame = Rect::from_parts(self.global_origin, self.frame.size);
        let slice = self.parent.upgrade().unwrap().get_slice_with_frame(frame);
        slice.pixel_data()
    }

    fn blit2(&self, _source_layer: &Box<dyn LikeLayerSlice>) {
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
        let frame = Rect::from_parts(self.global_origin + draw_loc, font_size);
        self.parent
            .upgrade()
            .unwrap()
            .draw_char(frame, ch, draw_color);
    }

    fn get_pixel_row(&self, _y: usize) -> Vec<u8> {
        todo!()
    }

    fn get_pixel_row_slice(&self, _y: usize) -> (*const u8, usize) {
        todo!()
    }

    fn get_buf_ptr_and_row_size(&self) -> (*const u8, usize, usize) {
        todo!()
    }

    fn track_damage(&self, _r: Rect) {
        todo!()
    }

    fn drain_damages(&self) -> Vec<Rect> {
        todo!()
    }
}

#[derive(PartialEq)]
struct TileLayer {
    frame: Rect,
    inner: RefCell<SingleFramebufferLayer>,
}

impl TileLayer {
    fn new(frame: Rect, pixel_byte_layout: PixelByteLayout) -> Self {
        //println!("TileLayer.new({frame})");
        Self {
            frame,
            inner: RefCell::new(SingleFramebufferLayer::new_ext(
                frame.size,
                pixel_byte_layout,
            )),
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
        let inner = self.inner.borrow_mut();
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
    pixel_byte_layout: PixelByteLayout,
}

impl ExpandingLayer {
    pub fn new(pixel_byte_layout: PixelByteLayout) -> Rc<Self> {
        Rc::new(Self {
            visible_frame: RefCell::new(Rect::zero()),
            scroll_offset: RefCell::new(Point::zero()),
            layers: RefCell::new(Vec::new()),
            pixel_byte_layout,
        })
    }

    fn total_content_frame(&self) -> Rect {
        let mut total = Rect::zero();
        for tile in self.layers.borrow().iter() {
            total = total.union(tile.frame);
        }
        total
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
        Box::new(ExpandingLayerSlice::new(self, frame, frame.origin))
    }

    fn tile_size() -> Size {
        Size::new(100, 100)
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
        //println!("rects_not_contained_in_current_tiles {rect}");
        let mut rects_not_contained = vec![rect];
        for tile in self.layers.borrow().iter() {
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
            let new_tile = TileLayer::new(*new_tile_frame, self.pixel_byte_layout);
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
        // TODO(PT): Some kind of data structure that makes it easy to look up a tile by its contained coords
        for layer in layers.iter() {
            if layer.frame.contains(point) {
                let translated_point = point - layer.frame.origin;
                layer.putpixel(translated_point, color);
                return;
            }
        }
    }

    fn putpixel(&self, point: Point, color: Color) {
        // WTH?!
        self.expand_to_contain_rect(Rect::from_parts(point, Size::new(800, 800)));
        self.putpixel_unchecked(point, color);
    }

    pub fn draw_char(&self, frame: Rect, ch: char, draw_color: Color) {
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
        //println!("ExpandingLayer ignoring getParent");
        None
    }

    fn set_parent(&self, _parent: Weak<dyn NestedLayerSlice>) {
        //println!("ExpandingLayer ignoring setParent");
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

    fn draw(&self) -> Vec<Rect> {
        //println!("ExpandingLayer ignoring draw()");
        vec![]
    }
}

#[derive(Drawable)]
pub struct ScrollView {
    pub view: Rc<View>,
    pub layer: Rc<ExpandingLayer>,
    cached_mouse_position: RefCell<Point>,
}

impl ScrollView {
    pub fn new_ext<F: 'static + Fn(&View, Size) -> Rect>(
        sizer: F,
        pixel_byte_layout: PixelByteLayout,
    ) -> Rc<Self> {
        let layer = ExpandingLayer::new(pixel_byte_layout);
        let layer_clone = Rc::clone(&layer);
        let sizer = move |v: &View, superview_size: Size| -> Rect {
            let view_frame = sizer(v, superview_size);
            // Update the inner layer
            layer_clone.set_visible_frame(view_frame);
            view_frame
        };
        let view = Rc::new(View::new(Color::new(100, 100, 100), sizer));
        view.set_border_enabled(false);
        //view.set_border_enabled(false);

        Rc::new(Self {
            view,
            layer,
            cached_mouse_position: RefCell::new(Point::zero()),
        })
    }

    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(sizer: F) -> Rc<Self> {
        Self::new_ext(sizer, PixelByteLayout::RGBA)
    }

    pub fn add_component(self: Rc<Self>, elem: Rc<dyn UIElement>) {
        Rc::clone(&self.view).add_component(elem)
    }

    fn scroll_bar_width() -> isize {
        42
    }

    fn record_mouse_position(&self, mouse_point: Point) {
        *self.cached_mouse_position.borrow_mut() = mouse_point;
    }

    fn scroll_bar_region_contains_mouse(&self) -> bool {
        let mouse_position = *self.cached_mouse_position.borrow();
        // TODO(PT): Check whether we need to relocate this point to our local coordinate space
        self.scroll_bar_content_frame().contains(mouse_position)
    }

    fn scroll_bar_content_frame(&self) -> Rect {
        let (frame_of_inner_margin, frame_of_content) = compute_inner_margin_and_content_frames(
            self.outer_border_insets(),
            self.inner_border_insets(),
            self.frame().size,
            true,
        );

        // Start from y=0 so that the top border lines up with the content's border
        let scroll_bar_content_frame = Rect::from_parts(
            Point::new(frame_of_inner_margin.max_x(), 0),
            Size::new(
                Self::scroll_bar_width(),
                frame_of_inner_margin.height()
                    + self.outer_border_insets().top
                    + self.outer_border_insets().bottom,
            ),
        );
        scroll_bar_content_frame
    }
}

impl Bordered for ScrollView {
    fn outer_border_insets(&self) -> RectInsets {
        RectInsets::new(5, 5, Self::scroll_bar_width(), 5)
    }

    fn inner_border_insets(&self) -> RectInsets {
        RectInsets::new(6, 6, 6, 6)
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        self.layer.draw_visible_content_onto(onto)
    }

    fn draw_border_with_insets(&self, onto: &mut Box<dyn LikeLayerSlice>) -> Rect {
        let (_, frame_of_content) = draw_border_with_insets(
            onto,
            self.outer_border_insets(),
            self.inner_border_insets(),
            self.frame().size,
            true,
            self.currently_contains_mouse(),
        );
        let scroll_bar_content_frame = self.scroll_bar_content_frame();

        // Fill the scroll bar area with the same gray as the rest of the outer margin
        // (Currently, our rect filling only supports a constant thickness around all edges, so the border
        // drawn on the right hand side will be too thin unless we color it manually)
        onto.fill_rect(
            scroll_bar_content_frame,
            Color::light_gray(),
            StrokeThickness::Filled,
        );

        let mut scroll_bar_onto = onto.get_slice(scroll_bar_content_frame);
        let (_, frame_of_scrollbar_content) = draw_border_with_insets(
            &mut scroll_bar_onto,
            RectInsets::new(5, 5, 5, 5),
            RectInsets::new(5, 5, 5, 5),
            scroll_bar_content_frame.size,
            false,
            false,
        );

        // Draw the 'background' of the scroll bar
        scroll_bar_onto.fill_rect(
            frame_of_scrollbar_content,
            Color::black(),
            StrokeThickness::Filled,
        );

        let scrollbar_attrs = compute_scrollbar_attributes(
            scroll_bar_onto.frame().size,
            self.layer.frame().size,
            self.layer.total_content_frame().size,
            self.layer.scroll_offset(),
        );
        scroll_bar_onto.fill_rect(
            Rect::from_parts(scrollbar_attrs.origin, scrollbar_attrs.size),
            Color::new(180, 180, 180),
            StrokeThickness::Filled,
        );

        // Mouse interaction highlight on top of the scroll bar
        let scroll_bar_highlight_color = if self.scroll_bar_region_contains_mouse() {
            Color::new(200, 200, 200)
        } else {
            Color::new(160, 160, 160)
        };
        scroll_bar_onto.fill_rect(
            Rect::from_parts(scrollbar_attrs.origin, scrollbar_attrs.size),
            scroll_bar_highlight_color,
            StrokeThickness::Width(1),
        );

        // PT: Horrible hack to make the highlight border show perfectly on top of the scroll bar
        draw_outer_mouse_highlight(onto, self.frame().size, self.currently_contains_mouse());

        frame_of_content
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

fn scrollable_region_size(viewport_size: Size, content_size: Size) -> Size {
    Size::new(
        content_size.width,
        content_size.height - (viewport_size.height as f64 / 2.0) as isize,
    )
}

impl UIElement for ScrollView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered()
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited()
    }

    fn handle_mouse_moved(&self, mouse_point: Point) {
        // Keep track of the mouse position, so we can detect whether the mouse is
        // hovering on the scroll bar
        self.record_mouse_position(mouse_point);
        self.view.handle_mouse_moved(mouse_point)
    }

    fn handle_left_click(&self, mouse_point: Point) {
        self.view.handle_left_click(mouse_point)
    }

    fn handle_key_pressed(&self, key: KeyCode) {
        self.view.handle_key_pressed(key);
        Bordered::draw(self);
    }

    fn handle_mouse_scrolled(&self, _mouse_point: Point, delta_z: isize) {
        let mut scroll_offset = self.layer.scroll_offset();
        let previous_scroll_offset = scroll_offset;
        let content_frame = self.layer.total_content_frame();

        // Scale how much the mouse scrolls by based on how large the content view is
        // TODO(PT): Scale by how fast the user is scrolling
        let scroll_step = {
            if content_frame.height() > 10000 {
                10
            } else if content_frame.height() > 1000 {
                5
            } else {
                1
            }
        };
        scroll_offset.y += delta_z * scroll_step;

        // Don't allow the user to scroll such that the entire content view is above the viewport.
        //
        // To give a good sense of interactivity, allow the user to scroll such that *half* of the
        // bottommost content is above the viewport.
        let scrollable_region = Rect::from_parts(
            content_frame.origin,
            scrollable_region_size(self.layer.frame().size, content_frame.size),
        );

        // Would this exceed the visible content frame?
        // If so, don't allow the user to scroll there.
        // TODO(PT): Eventually, we could have some kind of attribute to control whether the user is allowed to scroll
        // past the visible content bounds.
        // For now, it's blocked because it made scroll bars a tiny bit tricky.
        if !scrollable_region.contains(scroll_offset) {
            // Without further work, the user can get 'stuck' where the step size would push them past the
            // content bounds, so they can't scroll to the very top or very bottom.
            if delta_z < 0 {
                scroll_offset = Point::zero();
            } else {
                scroll_offset = Point::new(0, scrollable_region.height());
            }
        }
        // TODO(PT): Update this to handle horizontal scrolls as well?

        // Only redraw if we've changed state
        if scroll_offset != previous_scroll_offset {
            self.layer.set_scroll_offset(scroll_offset);
            Bordered::draw(self);
        }
    }

    fn handle_key_released(&self, key: KeyCode) {
        self.view.handle_key_released(key);
        Bordered::draw(self);
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size)
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
struct ScrollbarAttributes {
    origin: Point,
    size: Size,
}

impl ScrollbarAttributes {
    fn new(origin: Point, size: Size) -> Self {
        Self { origin, size }
    }
}

fn lerp(a: f64, b: f64, t: f64) -> f64 {
    a + (b - a) * t
}

fn compute_scrollbar_attributes(
    scroll_bar_onto_size: Size,
    viewport_size: Size,
    content_size: Size,
    scroll_position: Point,
) -> ScrollbarAttributes {
    // TODO(PT): Handle when the scroll offset is 'negative' i.e. when there's a bunch of content above the origin?
    let scrollbar_vertical_padding = 12;
    let scrollbar_canvas_height = (viewport_size.height - (scrollbar_vertical_padding * 2)) as f64;
    let min_scrollbar_height = (scrollbar_canvas_height * 0.08) as isize;
    let max_scrollbar_height = (scrollbar_canvas_height * 0.35) as isize;

    // If the viewport is larger than the current content size, cap out at 100%
    let nominal_scrollbar_height = if viewport_size.height >= content_size.height {
        scrollbar_canvas_height
    } else {
        lerp(
            min_scrollbar_height as f64,
            max_scrollbar_height as f64,
            viewport_size.height as f64 / content_size.height as f64,
        )
    };

    let scrollable_region_size = scrollable_region_size(viewport_size, content_size);
    let scrolled_proportion = scroll_position.y as f64 / scrollable_region_size.height as f64;
    let scrollbar_width = (scroll_bar_onto_size.width as f64 * 0.4) as isize;

    let scrollbar_tuck_in_proportion = 0.2;
    let max_scrollbar_y = viewport_size.height - scrollbar_vertical_padding;
    let max_nominal_scrollbar_origin_y = max_scrollbar_y as f64 - nominal_scrollbar_height;
    // Slightly tricky bit ahead: 
    // When not in the 'tuck in regions', the scrollbar should be linearly proportional to the scrolled proportion
    // We need to find the slope and y-intercept of the line that crosses (0, scrollbar_vertical_padding) and (1, max_nominal_scrollbar_origin_y)
    // Simplified slope intercept form
    let slope = 1.0 / (1.0 - (scrollbar_tuck_in_proportion * 2.0));
    let intercept = -slope * scrollbar_tuck_in_proportion;
    // And ensure we never cross the [0, 1] bounds
    let normalized_proportion = (intercept + (slope * scrolled_proportion)).max(0.0).min(1.0);
    let nominal_scrollbar_origin_y = lerp(
        scrollbar_vertical_padding as f64,
        max_nominal_scrollbar_origin_y,
        normalized_proportion,
    );

    // The scroll bar should 'tuck in' if the user is near either extrema
    let scrollbar_height = if scrolled_proportion < scrollbar_tuck_in_proportion {
        lerp(
            min_scrollbar_height as f64,
            nominal_scrollbar_height,
            scrolled_proportion / scrollbar_tuck_in_proportion,
        )
    } else if scrolled_proportion >= (1.0 - scrollbar_tuck_in_proportion) {
        lerp(
            nominal_scrollbar_height,
            min_scrollbar_height as f64,
            (scrolled_proportion % scrollbar_tuck_in_proportion) / scrollbar_tuck_in_proportion,
        )
    } else {
        nominal_scrollbar_height
    };

    let scrollbar_origin_y = if scrolled_proportion < scrollbar_tuck_in_proportion {
        scrollbar_vertical_padding as f64
    } else if scrolled_proportion >= (1.0 - scrollbar_tuck_in_proportion) {
        let max_tucked_scrollbar_origin_y =
            (viewport_size.height - scrollbar_vertical_padding - min_scrollbar_height) as f64;
        lerp(
            max_nominal_scrollbar_origin_y,
            max_tucked_scrollbar_origin_y,
            (scrolled_proportion % scrollbar_tuck_in_proportion) / scrollbar_tuck_in_proportion,
        )
    } else {
        nominal_scrollbar_origin_y
    };
    let mut scrollbar_origin = Point::new(
        ((scroll_bar_onto_size.width as f64 / 2.0) - (scrollbar_width as f64 / 2.0)) as isize,
        scrollbar_origin_y as isize,
    );
    let scrollbar_size = Size::new(scrollbar_width, scrollbar_height as _);

    // Final guard to make the scrollbar's max-y pixel perfect when scrolled all the way to the bottom
    if scrollbar_origin.y + scrollbar_size.height >= max_scrollbar_y {
        let diff = scrollbar_origin.y + scrollbar_size.height - max_scrollbar_y + 1;
        scrollbar_origin.y -= diff;
    }

    ScrollbarAttributes::new(scrollbar_origin, scrollbar_size)
}

#[cfg(test)]
mod test {
    use crate::scroll_view::{compute_scrollbar_attributes, ScrollbarAttributes};
    use crate::scroll_view::{ExpandingLayer, TileLayer};
    use agx_definitions::{PixelByteLayout, Point, Rect, Size};
    use alloc::vec;
    use alloc::vec::Vec;

    fn expect_insert_results_in_tile_frames_with_existing_tiles(
        rect: Rect,
        expected_tile_frames: Vec<Rect>,
        existing_tile_frames: Vec<Rect>,
    ) {
        let pixel_byte_layout = PixelByteLayout::RGBA;
        let layer = ExpandingLayer::new(pixel_byte_layout);
        {
            // Set up the tiles that should already be present
            let mut existing_tiles = layer.layers.borrow_mut();
            for existing_tile_frame in existing_tile_frames.iter() {
                existing_tiles.push(TileLayer::new(*existing_tile_frame, pixel_byte_layout));
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
