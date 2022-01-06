use alloc::vec::Vec;
use alloc::{boxed::Box, collections::BTreeMap};

use axle_rt::printf;
use axle_rt::AmcMessage;

use axle_rt::ExpectsEventField;
use axle_rt::{amc_message_await, amc_message_await_untyped, amc_message_send};

use agx_definitions::{Color, Layer, Point, Rect, Size, UnownedLayer};
use awm_messages::{AwmCreateWindow, AwmCreateWindowResponse, AwmWindowRedrawReady};

use crate::alloc::string::ToString;
use crate::ui_elements::*;
use crate::window_events::*;

pub struct AwmWindow {
    pub layer: UnownedLayer<'static>,
    pub current_size: Size,

    damaged_rects: Vec<Rect>,
    ui_elements: Vec<Box<dyn UIElement>>,
}

impl AwmWindow {
    const AWM_SERVICE_NAME: &'static str = "com.axle.awm";

    pub fn new(size: Size) -> Self {
        // Start off by getting a window from awm
        amc_message_send(AwmWindow::AWM_SERVICE_NAME, AwmCreateWindow::new(&size));
        // awm should send back info about the window that was created
        let window_info: AmcMessage<AwmCreateWindowResponse> =
            amc_message_await(Some(AwmWindow::AWM_SERVICE_NAME));

        let bpp = window_info.body().bytes_per_pixel as usize;
        let screen_resolution = Size::from(&window_info.body().screen_resolution);

        let framebuffer_slice = core::ptr::slice_from_raw_parts_mut(
            window_info.body().framebuffer_ptr,
            bpp * screen_resolution.width * screen_resolution.height,
        );
        let framebuffer: &mut [u8] = unsafe { &mut *(framebuffer_slice as *mut [u8]) };
        printf!(
            "Made framebuffer with layer {:p} {:p} {:p}\n",
            &framebuffer,
            &framebuffer.as_ptr(),
            window_info.body().framebuffer_ptr,
        );
        let layer = UnownedLayer::new(framebuffer, bpp, screen_resolution);

        AwmWindow {
            layer,
            current_size: size,
            damaged_rects: Vec::new(),
            ui_elements: Vec::new(),
        }
    }

    pub fn add_component(&mut self, elem: Box<dyn UIElement>) {
        self.ui_elements.push(elem);
    }

    pub fn drop_all_ui_elements(&mut self) {
        self.ui_elements.clear();
    }

    pub fn draw(&mut self) {
        // Start off with a colored background
        self.layer.fill_rect(
            &Rect::new(Point::zero(), self.current_size),
            &Color::new(128, 4, 56),
        );

        for elem in &self.ui_elements {
            elem.draw(&mut self.layer);
        }
    }

    pub fn commit(&self) {
        amc_message_send(AwmWindow::AWM_SERVICE_NAME, AwmWindowRedrawReady::new());
    }

    fn key_down(&self, event: &KeyDown) {
        printf!("Key down: {:?}\n", event);
    }

    fn key_up(&self, event: &KeyUp) {
        printf!("Key up: {:?}\n", event);
    }

    fn mouse_moved(&self, event: &MouseMoved) {
        //printf!("Mouse moved: {:?}\n", event);
    }

    fn mouse_dragged(&self, event: &MouseDragged) {
        printf!("Mouse dragged: {:?}\n", event);
    }

    fn mouse_left_click_down(&mut self, event: &MouseLeftClickStarted) {
        printf!("Mouse left click started: {:?}\n", event);
        let len = self.ui_elements.len();

        for i in 0..len {
            //let mut elem = self.ui_elements.swap_remove(i);
            //for (elem, left_click_cb) in &self.ui_elements {
            //if elem.frame().contains(Point::from(&event.mouse_pos)) {
            //elem.handle_left_click(&mut *handler);
            //}
        }
    }

    fn mouse_left_click_up(&self, event: &MouseLeftClickEnded) {
        printf!("Mouse left click ended: {:?}\n", event);
    }

    fn mouse_entered(&self, event: &MouseEntered) {
        printf!("Mouse entered: {:?}\n", event);
    }

    fn mouse_exited(&self, event: &MouseExited) {
        printf!("Mouse exited: {:?}\n", event);
    }

    fn mouse_scrolled(&self, event: &MouseScrolled) {
        printf!("Mouse scrolled: {:?}\n", event);
    }

    fn window_resized(&self, event: &WindowResized) {
        // Don't commit the window here as we'll receive tons of resize events
        // In the future, we can present a 'blur UI' while the window resizes
    }

    fn window_resize_ended(&self, event: &WindowResizeEnded) {
        printf!("Window resize ended: {:?}\n", event);
        self.commit();
    }

    fn window_close_requested(&self, event: &WindowCloseRequested) {
        printf!("Window close requested: {:?}\n", event);
    }

    unsafe fn body_as_type_unchecked<T: AwmWindowEvent>(body: &[u8]) -> &T {
        &*(body.as_ptr() as *const T)
    }

    pub fn await_next_event(&mut self) {
        let msg_unparsed: AmcMessage<[u8]> =
            unsafe { amc_message_await_untyped(Some(AwmWindow::AWM_SERVICE_NAME)).unwrap() };

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
            match event {
                // Keyboard events
                KeyDown::EXPECTED_EVENT => {
                    self.key_down(AwmWindow::body_as_type_unchecked(raw_body))
                }
                KeyUp::EXPECTED_EVENT => self.key_up(AwmWindow::body_as_type_unchecked(raw_body)),
                // Mouse events
                MouseMoved::EXPECTED_EVENT => {
                    self.mouse_moved(AwmWindow::body_as_type_unchecked(raw_body))
                }
                MouseDragged::EXPECTED_EVENT => {
                    self.mouse_dragged(AwmWindow::body_as_type_unchecked(raw_body))
                }
                MouseScrolled::EXPECTED_EVENT => {
                    self.mouse_scrolled(AwmWindow::body_as_type_unchecked(raw_body))
                }
                MouseLeftClickStarted::EXPECTED_EVENT => {
                    self.mouse_left_click_down(AwmWindow::body_as_type_unchecked(raw_body))
                }
                MouseLeftClickEnded::EXPECTED_EVENT => {
                    self.mouse_left_click_up(AwmWindow::body_as_type_unchecked(raw_body))
                }
                // Mouse focus events
                MouseEntered::EXPECTED_EVENT => {
                    self.mouse_entered(AwmWindow::body_as_type_unchecked(raw_body))
                }
                MouseExited::EXPECTED_EVENT => {
                    self.mouse_exited(AwmWindow::body_as_type_unchecked(raw_body))
                }
                // Window events
                WindowResized::EXPECTED_EVENT => {
                    self.window_resized(AwmWindow::body_as_type_unchecked(raw_body))
                }
                WindowResizeEnded::EXPECTED_EVENT => {
                    self.window_resize_ended(AwmWindow::body_as_type_unchecked(raw_body))
                }
                WindowCloseRequested::EXPECTED_EVENT => {
                    self.window_close_requested(AwmWindow::body_as_type_unchecked(raw_body))
                }
                _ => printf!("Unknown event: {}\n", event),
            }
        }
    }
}
