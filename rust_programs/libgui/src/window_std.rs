use core::cmp::max;
use core::fmt::{Display, Formatter};
use core::mem::transmute;
use std::{
    cell::RefCell,
    rc::{Rc, Weak},
};

use crate::ui_elements::UIElement;
use crate::window_events::KeyCode;
use agx_definitions::{
    scanline_compute_fill_lines_from_edges, Color, Drawable, FillMode, LikeLayerSlice, Line,
    LineF64, NestedLayerSlice, Point, PointF64, PolygonStack, Rect, Size, StrokeThickness,
    CHAR_HEIGHT, CHAR_WIDTH, FONT8X8,
};
use pixels::wgpu::TextureFormat;
use pixels::{Pixels, PixelsBuilder, SurfaceTexture};
use winit::event::{ElementState, Event};
use winit::event::{MouseButton, MouseScrollDelta};
use winit::event_loop::{ControlFlow, EventLoop};
use winit::keyboard::{Key, NamedKey};
use winit::window::{Window, WindowBuilder};
use winit::{dpi::LogicalSize, event::WindowEvent};

struct PixelLayerSlice {
    parent: Rc<RefCell<Pixels>>,
    parent_size: Size,
    frame: Rect,
    global_origin: Point,
}

impl PixelLayerSlice {
    fn new(
        parent: &Rc<RefCell<Pixels>>,
        parent_size: Size,
        frame: Rect,
        global_origin: Point,
    ) -> Self {
        Self {
            parent: Rc::clone(parent),
            parent_size,
            frame,
            global_origin,
        }
    }
}

impl Display for PixelLayerSlice {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "<PixelLayerSlice {}>", self.frame)
    }
}

impl LikeLayerSlice for PixelLayerSlice {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn fill_rect(&self, raw_rect: Rect, color: Color, thickness: StrokeThickness) {
        let mut rect = self.frame.constrain(raw_rect);
        rect.size.width = max(rect.size.width, 0);
        rect.size.height = max(rect.size.height, 0);
        /*
        println!(
            "PixelLayerSlice.fill_rect({rect}, {color:?}), global origin {}",
            self.global_origin
        );
        */

        let bpp = 4;
        let parent_size = self.parent_size;
        let parent_bytes_per_row = parent_size.width * bpp;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row);
        let slice_origin_offset = self.global_origin * bpp_multiple;
        let rect_origin_offset = slice_origin_offset + (rect.origin * bpp_multiple);
        //println!("\trect_origin_offset {rect_origin_offset}");

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
            let mut pixels = self.parent.borrow_mut();
            let fb = pixels.frame_mut();
            // Construct the filled row of pixels that we can copy row-by-row
            let bytes_in_row = (rect.width() * bpp) as usize;
            let mut src_row_slice = vec![0; bytes_in_row];
            for pixel_bytes_chunk in src_row_slice.chunks_exact_mut(bpp as _) {
                pixel_bytes_chunk[0] = color.b;
                pixel_bytes_chunk[1] = color.g;
                pixel_bytes_chunk[2] = color.r;
                pixel_bytes_chunk[3] = 0xff;
            }

            for y in 0..rect.height() {
                let row_start = (rect_origin_offset.y
                    + (y * parent_bytes_per_row)
                    + rect_origin_offset.x) as usize;
                let dst_row_slice = &mut fb[row_start..row_start + ((rect.width() * bpp) as usize)];
                dst_row_slice.copy_from_slice(&src_row_slice);
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
        /*
        if !self.frame.contains(loc + self.frame.origin) {
            return;
        }
        */

        let bpp = 4;
        let parent_size = self.parent_size;
        let parent_bytes_per_row = parent_size.width * bpp;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row);
        let mut pixels = self.parent.borrow_mut();
        let fb = pixels.frame_mut();
        let slice_origin_offset = self.global_origin * bpp_multiple;
        //let off = slice_origin_offset + (loc.y * parent_bytes_per_row) + (loc.x * bpp);
        let point_offset = slice_origin_offset + (loc * bpp_multiple);
        let off = (point_offset.y + point_offset.x) as usize;
        fb[off + 0] = color.b;
        fb[off + 1] = color.g;
        fb[off + 2] = color.r;
        fb[off + 3] = 0xff;
    }

    fn getpixel(&self, _loc: Point) -> Color {
        todo!()
    }

    fn get_slice(&self, rect: Rect) -> Box<dyn LikeLayerSlice> {
        //println!("LikeLayerSlice for PixelLayerSlice.get_slice({rect})");
        let constrained = Rect::from_parts(Point::zero(), self.frame.size).constrain(rect);
        let to_current_coordinate_system =
            Rect::from_parts(self.frame.origin + rect.origin, constrained.size);
        Box::new(Self::new(
            &self.parent,
            self.parent_size,
            to_current_coordinate_system,
            self.global_origin + rect.origin,
        ))
    }

    fn blit(
        &self,
        _source_layer: &Box<dyn LikeLayerSlice>,
        _source_frame: Rect,
        _dest_origin: Point,
    ) {
        todo!()
    }

    fn blit2(&self, source_layer: &Box<dyn LikeLayerSlice>) {
        // TODO(PT): Share this implementation with LayerSlice?
        assert_eq!(
            self.frame().size,
            source_layer.frame().size,
            "{} != {}",
            self.frame().size,
            source_layer.frame().size
        );

        let bpp = 4;
        let parent_size = self.parent_size;
        let parent_bytes_per_row = parent_size.width * bpp;
        let bpp_multiple = Point::new(bpp, parent_bytes_per_row);
        let mut pixels = self.parent.borrow_mut();
        let fb = pixels.frame_mut();
        let slice_origin_offset = self.frame.origin * bpp_multiple;

        let (src_base, src_slice_row_size, src_parent_framebuf_row_size) =
            source_layer.get_buf_ptr_and_row_size();

        for y in 0..self.frame().height() {
            // Blit an entire row at once
            let point_offset = slice_origin_offset + (Point::new(0, y) * bpp_multiple);
            let off = (point_offset.y + point_offset.x) as usize;
            let dst_row_slice = &mut fb[off..off + ((self.frame.width() * bpp) as usize)];
            let src_row_slice = unsafe {
                let src_row_start = src_base.offset(y * (src_parent_framebuf_row_size as isize));
                core::slice::from_raw_parts(src_row_start, src_slice_row_size)
            };
            dst_row_slice.copy_from_slice(src_row_slice);
        }
    }

    fn pixel_data(&self) -> Vec<u8> {
        todo!()
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
        vec![]
        //todo!()
    }

    fn fill_polygon_stack(&self, polygon_stack: &PolygonStack, color: Color, fill_mode: FillMode) {
        let slice = self;
        let mut bounding_box = polygon_stack.bounding_box();

        match fill_mode {
            FillMode::Filled => {
                for line in
                scanline_compute_fill_lines_from_edges(&polygon_stack.lines()).into_iter()
                {
                    // Horizontal line?
                    if line.p1.y.round() == line.p2.y.round() {
                        let line = Line::from(line);
                        slice.fill_rect(
                            Rect::from_parts(
                                //self.frame.origin + line.p1,
                                line.p1,
                                Size::new(line.p2.x - line.p1.x, 1),
                            ),
                            color,
                            StrokeThickness::Filled,
                        )
                    } else {
                        for (x, y) in line.as_inclusive_bresenham_iterator() {
                            // We've guaranteed that we'll have tiles to cover all the lines in the polygon above
                            slice.putpixel(
                                //Point::new(x + self.frame.origin.x, y + self.frame.origin.y),
                                Point::new(x, y),
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
                        //line.p1 + PointF64::from(self.frame.origin),
                        //line.p2 + PointF64::from(self.frame.origin),
                        line.p1, line.p2,
                    );
                    for (x, y) in line.as_inclusive_bresenham_iterator() {
                        slice.putpixel(Point::new(x, y), color);
                    }
                    //line.draw(self, color);
                }
            }
        }
    }
}

pub struct PixelLayer {
    size: Size,
    _window: Window,
    pub pixel_buffer: Rc<RefCell<Pixels>>,
}

impl PixelLayer {
    pub fn new(title: &str, event_loop: &EventLoop<()>, size: Size) -> Self {
        let window = {
            let size = LogicalSize::new(size.width as f64, size.height as f64);
            let scaled_size = size;
            WindowBuilder::new()
                .with_title(title)
                .with_inner_size(scaled_size)
                .with_min_inner_size(scaled_size)
                .with_visible(true)
                .with_resizable(false)
                //.with_decorations(false)
                //.with_fullscreen(Some(Fullscreen::Borderless(None)))
                .build(&event_loop)
                .unwrap()
        };
        //window.set_cursor_visible(false);
        //println!("Window inner size {:?}", window.inner_size());
        let pixel_buffer = {
            let window_size = window.inner_size();
            let surface_texture =
                SurfaceTexture::new(window_size.width as _, window_size.height as _, &window);
            PixelsBuilder::new(
                size.width.try_into().unwrap(),
                size.height.try_into().unwrap(),
                surface_texture,
            )
                .surface_texture_format(TextureFormat::Bgra8Unorm)
                .build()
                .unwrap()
        };
        pixel_buffer.render().unwrap();
        Self {
            size,
            _window: window,
            pixel_buffer: Rc::new(RefCell::new(pixel_buffer)),
        }
    }
}

impl Display for PixelLayer {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "<PixelLayer>")
    }
}

impl LikeLayerSlice for PixelLayer {
    fn frame(&self) -> Rect {
        Rect::with_size(self.size)
    }

    fn fill_rect(&self, raw_rect: Rect, color: Color, thickness: StrokeThickness) {
        self.get_slice(Rect::with_size(self.size))
            .fill_rect(raw_rect, color, thickness)
    }

    fn fill(&self, color: Color) {
        self.get_slice(Rect::with_size(self.size)).fill(color)
    }

    fn putpixel(&self, _loc: Point, _color: Color) {
        todo!()
    }

    fn getpixel(&self, _loc: Point) -> Color {
        todo!()
    }

    fn get_slice(&self, rect: Rect) -> Box<dyn LikeLayerSlice> {
        let constrained = Rect::from_parts(Point::zero(), self.size).constrain(rect);
        //println!("Constrained {constrained}");
        Box::new(PixelLayerSlice::new(
            &self.pixel_buffer,
            self.size,
            constrained,
            rect.origin,
        ))
    }

    fn blit(
        &self,
        _source_layer: &Box<dyn LikeLayerSlice>,
        _source_frame: Rect,
        _dest_origin: Point,
    ) {
        todo!()
    }

    fn blit2(&self, _source_layer: &Box<dyn LikeLayerSlice>) {
        todo!()
    }

    fn pixel_data(&self) -> Vec<u8> {
        todo!()
    }

    fn draw_char(&self, _ch: char, _draw_loc: Point, _draw_color: Color, _font_size: Size) {
        todo!()
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

    fn fill_polygon_stack(&self, polygon_stack: &PolygonStack, color: Color, fill_mode: FillMode) {
        todo!()
    }
}

pub struct AwmWindow {
    title: String,
    layer: RefCell<Option<PixelLayer>>,
    current_size: RefCell<Size>,
    ui_elements: RefCell<Vec<Rc<dyn UIElement>>>,
    elements_containing_mouse: RefCell<Vec<Rc<dyn UIElement>>>,
    event_loop: EventLoop<()>,
}

impl AwmWindow {
    pub fn new(title: &str, size: Size) -> Self {
        //println!("AwmWindow::new({title:?}, {size:?})");
        let event_loop = EventLoop::new().expect("Failed to create event loop");
        let layer = PixelLayer::new(title, &event_loop, size);
        Self {
            title: title.to_string(),
            layer: RefCell::new(Some(layer)),
            current_size: RefCell::new(size),
            ui_elements: RefCell::new(vec![]),
            elements_containing_mouse: RefCell::new(Vec::new()),
            event_loop,
        }
    }

    pub fn add_component(self: Rc<Self>, elem: Rc<dyn UIElement>) {
        // Ensure the component has a frame by running its sizer
        elem.handle_superview_resize(*self.current_size.borrow());

        // Set up a link to the parent
        elem.set_parent(Rc::downgrade(&(Rc::clone(&self) as _)));

        self.ui_elements.borrow_mut().push(elem);
    }

    fn handle_mouse_scrolled(&self, mouse_point: Point, delta_y: isize) {
        let elems_containing_mouse = &mut *self.elements_containing_mouse.borrow_mut();
        for elem in elems_containing_mouse {
            elem.handle_mouse_scrolled(mouse_point, delta_y);
        }
    }

    fn mouse_moved(&self, mouse_point: Point) {
        let elems = &*self.ui_elements.borrow();
        let elems_containing_mouse = &mut *self.elements_containing_mouse.borrow_mut();

        for elem in elems {
            let elem_contains_mouse = elem.frame().contains(mouse_point);

            // Did this element previously bound the mouse?
            if let Some(index) = elems_containing_mouse
                .iter()
                .position(|e| Rc::ptr_eq(e, elem))
            {
                // Did the mouse just exit this element?
                if !elem_contains_mouse {
                    elem.handle_mouse_exited();
                    // We don't need to preserve ordering, so swap_remove is OK
                    elems_containing_mouse.swap_remove(index);
                }
            } else if elem_contains_mouse {
                elem.handle_mouse_entered();
                elems_containing_mouse.push(Rc::clone(elem));
            }
        }

        for elem in elems_containing_mouse {
            // Translate the mouse position to the element's coordinate system
            let elem_pos = mouse_point - elem.frame().origin;
            elem.handle_mouse_moved(elem_pos);
        }
    }

    fn left_click(&self, mouse_point: Point) {
        let elems_containing_mouse = &mut *self.elements_containing_mouse.borrow_mut();
        for elem in elems_containing_mouse {
            // Translate the mouse position to the element's coordinate system
            let elem_pos = mouse_point - elem.frame().origin;
            elem.handle_left_click(elem_pos);
        }
    }

    fn left_click_up(&self, mouse_point: Point) {
        // Note that we send this event to all elements, regardless of whether they bound the mouse
        // when it's released. This helps to implement drag states where the element stays dragged
        // even if the mouse leaves the element's frame.
        for elem in self.ui_elements.borrow().iter() {
            // Translate the mouse position to the element's coordinate system
            let elem_pos = mouse_point - elem.frame().origin;
            elem.handle_left_click_up(elem_pos);
        }
    }

    fn mouse_exited(&self) {
        let elems_containing_mouse = &mut *self.elements_containing_mouse.borrow_mut();
        for elem in elems_containing_mouse.drain(..) {
            elem.handle_mouse_exited();
        }
    }

    fn key_down(&self, key: char) {
        // TODO(PT): One element should have keyboard focus at a time. How to select?
        let elems = self.ui_elements.borrow();
        for elem in elems.iter() {
            elem.handle_key_pressed(KeyCode(key as u32));
        }
    }

    fn key_up(&self, key: char) {
        // TODO(PT): One element should have keyboard focus at a time. How to select?
        let elems = self.ui_elements.borrow();
        for elem in elems.iter() {
            elem.handle_key_released(KeyCode(key as u32));
        }
    }

    pub fn enter_event_loop(self: &Rc<Self>) {
        println!("Entering event loop...");
        let current_size = self.current_size.borrow();

        let self_clone = Rc::clone(self);
        let scale_factor = 2;
        let mut last_cursor_pos = Point::zero();

        // Safety: This function never returns, so it's fine to take ownership of the event loop
        let event_loop_ptr: *const EventLoop<()> = &self.event_loop as *const EventLoop<()>;
        let event_loop: EventLoop<()> = unsafe { core::ptr::read(event_loop_ptr) };

        event_loop.run(move |event, event_loop_window_target| {
            event_loop_window_target.set_control_flow(ControlFlow::Poll);
            match event {
                Event::AboutToWait => self_clone.draw(),
                Event::WindowEvent { event, .. } => {
                    match event {
                        WindowEvent::MouseInput { state, button, .. } => {
                            println!("MouseInput {state:?}, button {button:?}");
                            match state {
                                ElementState::Pressed => match button {
                                    MouseButton::Left => {
                                        self_clone.left_click(last_cursor_pos);
                                    }
                                    _ => (),
                                },
                                ElementState::Released => match button {
                                    MouseButton::Left => {
                                        self_clone.left_click_up(last_cursor_pos);
                                    }
                                    _ => (),
                                }
                                _ => (),
                            }
                        }
                        WindowEvent::CursorMoved { position, .. } => {
                            //println!("CursorMoved position {position:?}");
                            let mouse_pos = Point::new(
                                (position.x as isize) / scale_factor,
                                (position.y as isize) / scale_factor,
                            );
                            self_clone.mouse_moved(mouse_pos);
                            last_cursor_pos = mouse_pos;
                        }
                        WindowEvent::CursorLeft { .. } => {
                            self_clone.mouse_exited();
                        }
                        WindowEvent::MouseWheel { delta, phase, .. } => {
                            //println!("MouseWheel event delta {delta:?} phase {phase:?}");
                            let delta_y = {
                                match delta {
                                    MouseScrollDelta::LineDelta(_, _) => todo!(),
                                    MouseScrollDelta::PixelDelta(phys_delta) => phys_delta.y,
                                }
                            };
                            self_clone
                                .handle_mouse_scrolled(last_cursor_pos, -(delta_y / 6.0) as _);
                            //elem.handle_mouse_scrolled(Point::from(event.mouse_point), event.delta_z as _);
                        }
                        WindowEvent::KeyboardInput { event, .. } => {
                            let maybe_key_code_as_char = {
                                if event.logical_key == Key::Named(NamedKey::Enter) {
                                    Some('\n')
                                } else if event.logical_key == Key::Named(NamedKey::Backspace) {
                                    // TODO(PT): Hack
                                    Some(0x08 as char)
                                } else {
                                    if let Some(c) = event.logical_key.to_text() {
                                        if c.len() > 1 {
                                            panic!(
                                                "Expected keypress to only be a single character"
                                            );
                                        }
                                        Some(c.chars().collect::<Vec<_>>()[0])
                                    } else {
                                        None
                                    }
                                }
                            };

                            if let Some(key_code_as_char) = maybe_key_code_as_char {
                                if event.state == ElementState::Pressed {
                                    self_clone.key_down(key_code_as_char);
                                } else {
                                    self_clone.key_up(key_code_as_char);
                                }
                            }
                        }
                        //_ => println!("Unhandled window event"),
                        _ => (),
                    }
                }
                _ => (),
            }
        });
    }

    pub fn draw(&self) {
        //printf!("Window drawing all contents...\n");
        let elems = &*self.ui_elements.borrow();
        for elem in elems {
            elem.draw();
        }
        //printf!("Window drawing all contents finished\n");
        let layer = self.layer.borrow();
        //layer.as_ref().unwrap().fill(Color::green());
        let pixel_buffer = layer.as_ref().unwrap().pixel_buffer.borrow_mut();

        /*
        let mut frame = pixel_buffer.get_frame();
        for pixel in frame.chunks_exact_mut(4) {
            pixel[0] = 0xff; // R
            pixel[1] = 0xf0; // G
            pixel[2] = 0x55; // B
            pixel[3] = 0xab; // A
        }
        */
        pixel_buffer.render().unwrap();
    }
}

impl NestedLayerSlice for AwmWindow {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        None
    }

    fn set_parent(&self, _parent: Weak<dyn NestedLayerSlice>) {
        panic!("Not supported for AwmWindow");
    }

    fn get_slice(&self) -> Box<dyn LikeLayerSlice> {
        self.layer
            .borrow_mut()
            .as_ref()
            .unwrap()
            .get_slice(Rect::from_parts(Point::zero(), *self.current_size.borrow()))
    }

    fn get_slice_for_render(&self) -> Box<dyn LikeLayerSlice> {
        self.get_slice()
    }
}

impl Drawable for AwmWindow {
    fn frame(&self) -> Rect {
        Rect::from_parts(Point::zero(), *self.current_size.borrow())
    }

    fn content_frame(&self) -> Rect {
        self.frame()
    }

    fn draw(&self) -> Vec<Rect> {
        panic!("Not available for AwmWindow");
    }
}
