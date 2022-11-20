#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(format_args_nl)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate libc;

use alloc::boxed::Box;
use alloc::{collections::BTreeMap, format, rc::Weak, vec, vec::Vec};
use alloc::{
    rc::Rc,
    string::{String, ToString},
};
use core::cmp::{max, min};
use core::{cell::RefCell, cmp};

use axle_rt::{
    amc_has_message, amc_message_await, amc_message_await_untyped, amc_message_send,
    amc_register_service, printf, println, AmcMessage,
};
use axle_rt::{ContainsEventField, ExpectsEventField};

use agx_definitions::{
    Color, Drawable, Layer, LayerSlice, LikeLayerSlice, Line, NestedLayerSlice, Point, Rect,
    RectInsets, SingleFramebufferLayer, Size, StrokeThickness,
};
use awm_messages::{AwmCreateWindow, AwmCreateWindowResponse, AwmWindowRedrawReady};

use mouse_driver_messages::{MousePacket, MOUSE_DRIVER_SERVICE_NAME};

mod awm2_messages;
mod effects;

use crate::awm2_messages::AWM2_SERVICE_NAME;
use crate::effects::draw_radial_gradient;
use axle_rt::core_commands::{
    AmcAwmMapFramebuffer, AmcAwmMapFramebufferResponse, AmcSharedMemoryCreateRequest,
    AmcSharedMemoryCreateResponse, AMC_CORE_SERVICE_NAME,
};
use libgui::window_events::AwmWindowEvent;
use libgui::AwmWindow;

fn setup_awm_framebuffer() -> SingleFramebufferLayer {
    // Ask the kernel to map in the framebuffer and send us info about it
    amc_message_send(AMC_CORE_SERVICE_NAME, AmcAwmMapFramebuffer::new());
    let framebuffer_info_msg: AmcMessage<AmcAwmMapFramebufferResponse> =
        amc_message_await(Some(AMC_CORE_SERVICE_NAME));
    let framebuffer_info = framebuffer_info_msg.body();
    println!("Received framebuffer info: {framebuffer_info:?}");

    let bpp = framebuffer_info.bytes_per_pixel as isize;
    let screen_resolution = Size::new(
        framebuffer_info.width as isize,
        framebuffer_info.height as isize,
    );

    let framebuffer_slice = core::ptr::slice_from_raw_parts_mut(
        framebuffer_info.address as *mut libc::c_void,
        (bpp * screen_resolution.width * screen_resolution.height)
            .try_into()
            .unwrap(),
    );
    let framebuffer: &mut [u8] = unsafe { &mut *(framebuffer_slice as *mut [u8]) };
    SingleFramebufferLayer::from_framebuffer(
        unsafe { Box::from_raw(framebuffer) },
        bpp,
        screen_resolution,
    )
}

unsafe fn body_as_type_unchecked<T: ExpectsEventField + ContainsEventField>(body: &[u8]) -> &T {
    &*(body.as_ptr() as *const T)
}

/*
       typedef struct user_window {
           // TODO(PT): These fields can't be reordered because draw_queued_extra_draws()
           // interprets user_window_t as a view_t.
           Rect frame;
           ca_layer * layer;
           array_t * drawable_rects;
           array_t * extra_draws_this_cycle;
           bool should_scale_layer;
           uint32_t window_id;
           const char * owner_service;
           const char * title;
       }
*/
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
}

impl MouseState {
    fn new(pos: Point) -> Self {
        Self { pos }
    }

    fn handle_update(&mut self, packet: &MousePacket) {
        self.pos.x += packet.rel_x as isize;
        self.pos.y += packet.rel_y as isize;

        // Bind mouse to screen dimensions
        self.pos.x = max(0, self.pos.x);
        self.pos.y = max(0, self.pos.y);
        //mouse_pos.x = min(_screen.resolution.width - 4, mouse_pos.x);
        //mouse_pos.y = min(_screen.resolution.height - 10, mouse_pos.y);
    }
}

struct Desktop {
    desktop_frame: Rect,
    // The final video memory.
    video_memory_layer: SingleFramebufferLayer,
    screen_buffer_layer: SingleFramebufferLayer,
    desktop_background_layer: SingleFramebufferLayer,
    windows: Vec<Window>,
    mouse_state: MouseState,
}

impl Desktop {
    fn new() -> Self {
        let mut video_memory_layer = setup_awm_framebuffer();
        let desktop_frame = Rect::with_size(video_memory_layer.size());
        video_memory_layer.fill_rect(&desktop_frame, Color::yellow());

        let desktop_background_layer = SingleFramebufferLayer::new(desktop_frame.size);
        let screen_buffer_layer = SingleFramebufferLayer::new(desktop_frame.size);

        // Start the mouse in the middle of the screen
        let initial_mouse_pos = Point::new(desktop_frame.mid_x(), desktop_frame.mid_y());

        Self {
            desktop_frame: Rect::with_size(video_memory_layer.size()),
            video_memory_layer,
            screen_buffer_layer,
            desktop_background_layer,
            windows: vec![],
            mouse_state: MouseState::new(initial_mouse_pos),
        }
    }

    fn draw_background(&self) {
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

    fn draw_mouse(&mut self) {
        let mouse_color = Color::green();
        let cursor_size = Size::new(14, 14);
        let mouse_rect = Rect::from_parts(self.mouse_state.pos, cursor_size);
        let onto = self.screen_buffer_layer.get_slice(mouse_rect);
        onto.fill(Color::black());
        onto.fill_rect(
            Rect::with_size(mouse_rect.size).apply_insets(RectInsets::new(2, 2, 2, 2)),
            mouse_color,
            StrokeThickness::Filled,
        );
    }

    fn blit_background(&mut self) {
        self.screen_buffer_layer
            .copy_from(&self.desktop_background_layer);
    }

    fn draw_frame(&mut self) {
        // Start off by drawing a blank canvas consisting of the desktop background
        self.blit_background();

        // Draw each window
        for window in self.windows.iter_mut() {
            let dest_slice = self.screen_buffer_layer.get_slice(window.frame);
            // Start off at the origin within the window's coordinate space
            let source_slice = window.layer.get_slice(Rect::with_size(window.frame.size));
            dest_slice.blit2(&source_slice);
        }

        // Finally, draw the mouse cursor
        self.draw_mouse();

        // Now blit the screen buffer to the backing video memory
        self.video_memory_layer.copy_from(&self.screen_buffer_layer);
    }

    fn spawn_window(&mut self, source: String, request: &AwmCreateWindow) {
        println!("Creating window of size {:?} for {}", request.size, source);

        // Place the window in the center of the screen
        let res = self.desktop_frame.size;
        let window_size = Size::from(&request.size);
        let new_window_origin = Point::new(
            (res.width / 2) - (window_size.width / 2),
            (res.height / 2) - (window_size.height / 2),
        );

        // Ask the kernel to set up a shared memory mapping we'll use for the framebuffer
        // The framebuffer will be the screen size to allow window resizing
        let desktop_size = self.desktop_frame.size;
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
        let window_layer = SingleFramebufferLayer::from_framebuffer(
            unsafe { Box::from_raw(framebuffer) },
            bytes_per_pixel,
            desktop_size,
        );

        let window_created_msg = AwmCreateWindowResponse::new(
            desktop_size,
            bytes_per_pixel as u32,
            shared_memory_response.remote_buffer_start,
        );
        println!("Sending response to {source} {source:?}: {window_created_msg:?}");
        amc_message_send(&source, window_created_msg);

        let new_window = Window::new(
            &source,
            Rect::from_parts(new_window_origin, window_size),
            window_layer,
        );
        self.windows.push(new_window);
    }
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service(AWM2_SERVICE_NAME);

    let mut desktop = Desktop::new();
    desktop.draw_background();
    desktop.draw_frame();

    loop {
        /*
        if !amc_has_message(None) {
            continue;
        }
        */

        let msg_unparsed: AmcMessage<[u8]> = unsafe { amc_message_await_untyped(None).unwrap() };

        // Parse the first bytes of the message as a u32 event field
        let raw_body = msg_unparsed.body();
        let event = u32::from_ne_bytes(
            // We must slice the array to the exact size of a u32 for the conversion to succeed
            raw_body[..core::mem::size_of::<u32>()]
                .try_into()
                .expect("Failed to get 4-length array from message body"),
        );

        // Each inner call to body_as_type_unchecked is unsafe because we must be
        // sure we're casting to the right type.
        // Since we verify the type on the LHS, each usage is safe.
        //
        // Wrap the whole thing in an unsafe block to reduce
        // boilerplate in each match arm.
        unsafe {
            match msg_unparsed.source() {
                MOUSE_DRIVER_SERVICE_NAME => match event {
                    MousePacket::EXPECTED_EVENT => desktop
                        .mouse_state
                        .handle_update(body_as_type_unchecked(raw_body)),
                    _ => {
                        println!("Ignoring unknown message from mouse driver")
                    }
                },
                _ => {
                    // Unknown sender - probably a client wanting to interact with the window manager
                    match event {
                        // Mouse driver events
                        // libgui events
                        // Keyboard events
                        AwmCreateWindow::EXPECTED_EVENT => {
                            desktop.spawn_window(
                                msg_unparsed.source().to_string(),
                                body_as_type_unchecked(raw_body),
                            );
                        }
                        AwmWindowRedrawReady::EXPECTED_EVENT => {
                            //println!("Window said it was ready to redraw!");
                        }
                        _ => {
                            println!("Awm ignoring message with unknown event type: {event}");
                        }
                    }
                }
            }
        }

        desktop.draw_frame();
    }

    /*
    amc_msg_u32_1__send(AXLE_CORE_SERVICE_NAME, AMC_AWM_MAP_FRAMEBUFFER);

    amc_message_t* msg;
    amc_message_await__u32_event(AXLE_CORE_SERVICE_NAME, AMC_AWM_MAP_FRAMEBUFFER_RESPONSE, &msg);
    amc_framebuffer_info_t*uframebuffer_info = (amc_framebuffer_info_t*)msg->body;
    */

    0
}
