use alloc::vec::Vec;
use alloc::{boxed::Box, rc::Rc};

use core::cell::RefCell;

use axle_rt::printf;
use axle_rt::AmcMessage;

use axle_rt::ExpectsEventField;
use axle_rt::{amc_message_await, amc_message_await_untyped, amc_message_send};

use agx_definitions::{Color, Layer, Point, Rect, SingleFramebufferLayer, Size};
use awm_messages::{AwmCreateWindow, AwmCreateWindowResponse, AwmWindowRedrawReady};

use crate::ui_elements::*;
use crate::window_events::*;

pub struct AwmWindow {
    pub layer: RefCell<SingleFramebufferLayer>,
    pub current_size: RefCell<Size>,

    _damaged_rects: Vec<Rect>,
    ui_elements: RefCell<Vec<Rc<dyn UIElement>>>,
    elements_containing_mouse: RefCell<Vec<Rc<dyn UIElement>>>,
}

impl AwmWindow {
    const AWM_SERVICE_NAME: &'static str = "com.axle.awm";

    pub fn new(size: Size) -> Self {
        // Start off by getting a window from awm
        amc_message_send(AwmWindow::AWM_SERVICE_NAME, AwmCreateWindow::new(&size));
        // awm should send back info about the window that was created
        let window_info: AmcMessage<AwmCreateWindowResponse> =
            amc_message_await(Some(AwmWindow::AWM_SERVICE_NAME));

        let bpp = window_info.body().bytes_per_pixel as isize;
        let screen_resolution = Size::from(&window_info.body().screen_resolution);

        let framebuffer_slice = core::ptr::slice_from_raw_parts_mut(
            window_info.body().framebuffer_ptr,
            (bpp * screen_resolution.width * screen_resolution.height)
                .try_into()
                .unwrap(),
        );
        let framebuffer: &mut [u8] = unsafe { &mut *(framebuffer_slice as *mut [u8]) };
        printf!(
            "Made framebuffer with layer {:p} {:p} {:p}\n",
            &framebuffer,
            &framebuffer.as_ptr(),
            window_info.body().framebuffer_ptr,
        );
        let layer = RefCell::new(SingleFramebufferLayer::from_framebuffer(
            unsafe { Box::from_raw(framebuffer) },
            bpp,
            screen_resolution,
        ));

        AwmWindow {
            layer,
            current_size: RefCell::new(size),
            _damaged_rects: Vec::new(),
            ui_elements: RefCell::new(Vec::new()),
            elements_containing_mouse: RefCell::new(Vec::new()),
        }
    }

    pub fn add_component(&self, elem: Rc<dyn UIElement>) {
        self.ui_elements.borrow_mut().push(elem);
    }

    pub fn drop_all_ui_elements(&self) {
        self.ui_elements.borrow_mut().clear();
    }

    pub fn draw(&self) {
        // Start off with a colored background
        let layer = &mut *self.layer.borrow_mut();
        /*
        layer.fill_rect(
            &Rect::from_parts(Point::zero(), *self.current_size.borrow()),
            Color::new(255, 255, 255),
        );
        */

        let elems = &*self.ui_elements.borrow();
        for elem in elems {
            elem.draw(layer);
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
        let mouse_point = Point::from(&event.mouse_pos);
        let elems = &*self.ui_elements.borrow();
        let mut elems_containing_mouse = &mut *self.elements_containing_mouse.borrow_mut();

        let layer = &mut *self.layer.borrow_mut();

        for elem in elems {
            let elem_contains_mouse = elem.frame().contains(mouse_point);
            let mut slice = layer.get_slice(elem.frame());

            // Did this element previously bound the mouse?
            if let Some(index) = elems_containing_mouse
                .iter()
                .position(|e| Rc::ptr_eq(e, elem))
            {
                // Did the mouse just exit this element?
                if !elem_contains_mouse {
                    elem.handle_mouse_exited(&mut slice);
                    // We don't need to preserve ordering, so swap_remove is OK
                    elems_containing_mouse.swap_remove(index);
                }
            } else if elem_contains_mouse {
                elem.handle_mouse_entered(&mut slice);
                elems_containing_mouse.push(Rc::clone(elem));
            }
        }

        for elem in elems_containing_mouse {
            let mut slice = layer.get_slice(elem.frame());
            elem.handle_mouse_moved(mouse_point, &mut slice);
        }

        //self.draw();
        self.commit();
    }

    fn mouse_dragged(&self, event: &MouseDragged) {
        printf!("Mouse dragged: {:?}\n", event);
    }

    fn mouse_left_click_down(&self, event: &MouseLeftClickStarted) {
        printf!("Mouse left click started: {:?}\n", event);

        let mut clicked_elem = None;
        {
            let elems = &*self.ui_elements.borrow();
            for elem in elems {
                printf!("Checking if elem contains point...\n");
                if elem.frame().contains(Point::from(&event.mouse_pos)) {
                    printf!("Found UI element that bounds click point, dispatching\n");
                    clicked_elem = Some(Rc::clone(&elem));
                    break;
                }
            }
        }

        if let Some(c) = clicked_elem {
            c.handle_left_click();
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
        let layer = &mut *self.layer.borrow_mut();
        let mut elems_containing_mouse = &mut *self.elements_containing_mouse.borrow_mut();
        for elem in elems_containing_mouse.drain(..) {
            let mut slice = layer.get_slice(elem.frame());
            elem.handle_mouse_exited(&mut slice);
        }
        self.commit();
    }

    fn mouse_scrolled(&self, event: &MouseScrolled) {
        printf!("Mouse scrolled: {:?}\n", event);
    }

    fn window_resized(&self, _event: &WindowResized) {
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

    pub fn await_next_event(&self) {
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

    pub fn enter_event_loop(&self) {
        // Commit once so the window shows its initial contents before we start processing messages
        self.draw();
        self.commit();

        loop {
            self.await_next_event();
        }
    }
}