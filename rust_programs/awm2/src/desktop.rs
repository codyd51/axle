use crate::effects::draw_radial_gradient;
use agx_definitions::{
    Color, Layer, LikeLayerSlice, Point, Rect, RectInsets, SingleFramebufferLayer, Size,
    StrokeThickness,
};
use alloc::rc::Rc;
use awm_messages::AwmCreateWindow;
use axle_rt::core_commands::AmcSharedMemoryCreateRequest;
use mouse_driver_messages::MousePacket;
use std::cmp::{max, min};

struct Window {
    owner_service: String,
    frame: Rect,
    layer: SingleFramebufferLayer,
}

impl Window {
    fn new(owner_service: &str, frame: Rect, window_layer: SingleFramebufferLayer) -> Self {
        Self {
            owner_service: owner_service.to_string(),
            frame,
            layer: window_layer,
        }
    }
}

struct MouseState {
    pos: Point,
    desktop_size: Size,
    size: Size,
}

impl MouseState {
    fn new(pos: Point, desktop_size: Size) -> Self {
        Self {
            pos,
            size: Size::new(14, 14),
            desktop_size,
        }
    }

    fn handle_update(&mut self, packet: &MousePacket) {
        self.pos.x += packet.rel_x as isize;
        self.pos.y += packet.rel_y as isize;

        // Bind mouse to screen dimensions
        self.pos.x = max(self.pos.x, 0);
        self.pos.y = max(self.pos.y, 0);
        self.pos.x = min(self.pos.x, self.desktop_size.width - 4);
        self.pos.y = min(self.pos.y, self.desktop_size.height - 10);
    }

    fn frame(&self) -> Rect {
        Rect::from_parts(self.pos, self.size)
    }
}

struct CompositorState {
    /// While compositing the frame, awm will determine what individual elements
    /// must be redrawn to composite these rectangles.
    /// These may include portions of windows, the desktop background, etc.
    rects_to_fully_redraw: Vec<Rect>,
}

impl CompositorState {
    fn new() -> Self {
        Self {
            rects_to_fully_redraw: vec![],
        }
    }

    fn queue_full_redraw(&mut self, in_rect: Rect) {
        self.rects_to_fully_redraw.push(in_rect)
    }
}

pub struct Desktop {
    desktop_frame: Rect,
    // The final video memory.
    video_memory_layer: Rc<dyn LikeLayerSlice>,
    screen_buffer_layer: Box<SingleFramebufferLayer>,
    desktop_background_layer: Box<SingleFramebufferLayer>,
    windows: Vec<Window>,
    mouse_state: MouseState,
    compositor_state: CompositorState,
}

impl Desktop {
    pub fn new(video_memory_layer: Rc<dyn LikeLayerSlice>) -> Self {
        let desktop_frame = Rect::with_size(video_memory_layer.frame().size);
        video_memory_layer.fill_rect(desktop_frame, Color::yellow(), StrokeThickness::Filled);

        let desktop_background_layer = Box::new(SingleFramebufferLayer::new(desktop_frame.size));
        let screen_buffer_layer = Box::new(SingleFramebufferLayer::new(desktop_frame.size));

        // Start the mouse in the middle of the screen
        let initial_mouse_pos = Point::new(desktop_frame.mid_x(), desktop_frame.mid_y());

        Self {
            desktop_frame: Rect::with_size(video_memory_layer.frame().size),
            video_memory_layer,
            screen_buffer_layer,
            desktop_background_layer,
            windows: vec![],
            mouse_state: MouseState::new(initial_mouse_pos, desktop_frame.size),
            compositor_state: CompositorState::new(),
        }
    }

    pub fn draw_background(&self) {
        let gradient_outer = Color::new(255, 120, 120);
        let gradient_inner = Color::new(200, 120, 120);
        draw_radial_gradient(
            &self.desktop_background_layer,
            self.desktop_background_layer.size(),
            gradient_inner,
            gradient_outer,
            (self.desktop_background_layer.width() as f64 / 2.0) as isize,
            (self.desktop_background_layer.height() as f64 / 2.0) as isize,
            self.desktop_background_layer.height() as f64 * 0.65,
        );
    }

    fn draw_mouse(&mut self) -> Rect {
        let mouse_color = Color::green();
        let mouse_rect = self.mouse_state.frame();
        let onto = self.screen_buffer_layer.get_slice(mouse_rect);
        onto.fill(Color::black());
        onto.fill_rect(
            Rect::with_size(mouse_rect.size).apply_insets(RectInsets::new(2, 2, 2, 2)),
            mouse_color,
            StrokeThickness::Filled,
        );
        mouse_rect
    }

    pub fn blit_background(&mut self) {
        self.screen_buffer_layer
            .copy_from(&self.desktop_background_layer);
    }

    /// Very expensive and should be used sparingly
    pub fn commit_entire_buffer_to_video_memory(&mut self) {
        Self::copy_rect(
            &mut *self.screen_buffer_layer.get_slice(self.desktop_frame),
            &mut *self.video_memory_layer.get_slice(self.desktop_frame),
            self.desktop_frame,
        );
    }

    pub fn draw_frame(&mut self) {
        //println!("draw_frame()");
        // Start off by drawing a blank canvas consisting of the desktop background
        //self.blit_background();

        // Composite the frames for which we need to do the full walk of the desktop to find out what to draw
        for full_redraw_rect in self.compositor_state.rects_to_fully_redraw.iter() {
            //println!("\tProcessing full redraw rect {full_redraw_rect}");
            // For now, just composite the desktop background here
            Self::copy_rect(
                &mut *self.desktop_background_layer.get_slice(self.desktop_frame),
                &mut *self.screen_buffer_layer.get_slice(self.desktop_frame),
                *full_redraw_rect,
            );
        }

        // Draw each window
        for window in self.windows.iter_mut() {
            let dest_slice = self.screen_buffer_layer.get_slice(window.frame);
            // Start off at the origin within the window's coordinate space
            let source_slice = window.layer.get_slice(Rect::with_size(window.frame.size));
            dest_slice.blit2(&source_slice);
        }

        // Finally, draw the mouse cursor
        let mouse_rect = self.draw_mouse();

        // Now blit the screen buffer to the backing video memory
        // Follow the same steps as above to only copy what's changed
        // And empty queues as we go
        for full_redraw_rect in self.compositor_state.rects_to_fully_redraw.drain(..) {
            Self::copy_rect(
                &mut *self.screen_buffer_layer.get_slice(self.desktop_frame),
                &mut *self.video_memory_layer.get_slice(self.desktop_frame),
                full_redraw_rect,
            );
        }

        Self::copy_rect(
            &mut *self.screen_buffer_layer.get_slice(self.desktop_frame),
            &mut *self.video_memory_layer.get_slice(self.desktop_frame),
            mouse_rect,
        );
    }

    fn copy_rect(src: &mut dyn LikeLayerSlice, dst: &mut dyn LikeLayerSlice, rect: Rect) {
        let src_slice = src.get_slice(rect);
        let dst_slice = dst.get_slice(rect);
        dst_slice.blit2(&src_slice);
    }

    pub fn spawn_window(&mut self, source: String, request: &AwmCreateWindow) {
        println!("Creating window of size {:?} for {}", request.size, source);

        // Place the window in the center of the screen
        let res = self.desktop_frame.size;
        let window_size = Size::from(&request.size);
        let new_window_origin = Point::new(
            (res.width / 2) - (window_size.width / 2),
            (res.height / 2) - (window_size.height / 2),
        );

        let desktop_size = self.desktop_frame.size;
        #[cfg(target_os = "axle")]
        let window_layer = {
            // Ask the kernel to set up a shared memory mapping we'll use for the framebuffer
            // The framebuffer will be the screen size to allow window resizing
            let bytes_per_pixel = self.video_memory_layer.bytes_per_pixel();
            let shared_memory_size = desktop_size.width * desktop_size.height * bytes_per_pixel;
            println!("Requesting shared memory of size {shared_memory_size} {desktop_size:?}");
            let shared_memory_response =
                AmcSharedMemoryCreateRequest::send(&source, shared_memory_size as u32);

            let framebuffer_slice = core::ptr::slice_from_raw_parts_mut(
                shared_memory_response.local_buffer_start as *mut libc::c_void,
                shared_memory_size as usize,
            );
            let framebuffer: &mut [u8] = unsafe { &mut *(framebuffer_slice as *mut [u8]) };
            let window_created_msg = AwmCreateWindowResponse::new(
                desktop_size,
                bytes_per_pixel as u32,
                shared_memory_response.remote_buffer_start,
            );
            println!("Sending response to {source} {source:?}: {window_created_msg:?}");
            amc_message_send(&source, window_created_msg);
            SingleFramebufferLayer::from_framebuffer(
                unsafe { Box::from_raw(framebuffer) },
                bytes_per_pixel,
                desktop_size,
            )
        };
        #[cfg(not(target_os = "axle"))]
        let window_layer = SingleFramebufferLayer::new(desktop_size);

        let new_window = Window::new(
            &source,
            Rect::from_parts(new_window_origin, window_size),
            window_layer,
        );
        self.windows.push(new_window);
    }

    pub fn handle_mouse_update(&mut self, packet: &MousePacket) {
        // Previous mouse position should be redrawn
        self.compositor_state
            .queue_full_redraw(self.mouse_state.frame());

        self.mouse_state.handle_update(packet);
        // Don't bother queueing the new mouse position to redraw
        // For simplicity, the compositor will always draw the mouse over each frame
    }

    pub fn set_cursor_pos(&mut self, pos: Point) {
        self.mouse_state.pos = pos
    }
}
