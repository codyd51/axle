use alloc::vec;
use alloc::{boxed::Box, rc::Rc};
use alloc::{rc::Weak, vec::Vec};

use core::cell::RefCell;

use axle_rt::AmcMessage;
use axle_rt::{amc_message_await__u32_event, printf};

use axle_rt::ExpectsEventField;
use axle_rt::{amc_message_await_untyped, amc_message_send};

use agx_definitions::{
    Drawable, Layer, LikeLayerSlice, NestedLayerSlice, Point, Rect, SingleFramebufferLayer, Size,
};
use awm_messages::{
    AwmCreateWindow, AwmCreateWindowResponse, AwmWindowPartialRedraw, AwmWindowRedrawReady,
    AwmWindowUpdateTitle,
};

use crate::ui_elements::*;
use crate::window_events::*;

pub struct AwmWindow {
    pub layer: RefCell<SingleFramebufferLayer>,
    pub current_size: RefCell<Size>,

    damaged_rects: RefCell<Vec<Rect>>,
    ui_elements: RefCell<Vec<Rc<dyn UIElement>>>,
    elements_containing_mouse: RefCell<Vec<Rc<dyn UIElement>>>,
    amc_message_cb: RefCell<Option<Box<dyn Fn(&Self, AmcMessage<[u8]>)>>>,
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
            .get_slice(Rect::from_parts(Point::zero(), *self.current_size.borrow()))
    }

    fn get_slice_for_render(&self) -> Box<dyn LikeLayerSlice> {
        self.get_slice()
    }
}

impl AwmWindow {
    pub const AWM_SERVICE_NAME: &'static str = "com.axle.awm";

    pub fn new(title: &str, size: Size) -> Self {
        // Start off by getting a window from awm
        amc_message_send(AwmWindow::AWM_SERVICE_NAME, AwmCreateWindow::new(size));
        // awm should send back info about the window that was created
        let window_info: AmcMessage<AwmCreateWindowResponse> =
            amc_message_await__u32_event(AwmWindow::AWM_SERVICE_NAME);

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

        AwmWindow::set_title(title);

        AwmWindow {
            layer,
            current_size: RefCell::new(size),
            damaged_rects: RefCell::new(vec![]),
            ui_elements: RefCell::new(vec![]),
            elements_containing_mouse: RefCell::new(vec![]),
            amc_message_cb: RefCell::new(None),
        }
    }

    pub fn add_component(self: Rc<Self>, elem: Rc<dyn UIElement>) {
        // Ensure the component has a frame by running its sizer
        elem.handle_superview_resize(*self.current_size.borrow());

        // Set up a link to the parent
        elem.set_parent(Rc::downgrade(&(Rc::clone(&self) as _)));

        self.ui_elements.borrow_mut().push(elem);
    }

    pub fn drop_all_ui_elements(&self) {
        self.ui_elements.borrow_mut().clear();
    }

    pub fn remove_element(&self, elem: Rc<dyn UIElement>) {
        let elems_containing_mouse = &mut *self.elements_containing_mouse.borrow_mut();
        if let Some(index) = elems_containing_mouse
            .iter()
            .position(|e| Rc::ptr_eq(e, &elem))
        {
            // We don't need to preserve ordering, so swap_remove is OK
            elems_containing_mouse.swap_remove(index);
        }

        let ui_elements = &mut *self.ui_elements.borrow_mut();
        if let Some(index) = ui_elements.iter().position(|e| Rc::ptr_eq(e, &elem)) {
            // We don't need to preserve ordering, so swap_remove is OK
            ui_elements.swap_remove(index);
        }
    }

    pub fn draw(&self) {
        //printf!("Window drawing all contents...\n");
        let elems = &*self.ui_elements.borrow();
        let mut damages = self.damaged_rects.borrow_mut();
        for elem in elems {
            damages.append(&mut elem.draw());
        }
        //printf!("Window drawing all contents finished\n");
    }

    pub fn commit(&self) {
        amc_message_send(AwmWindow::AWM_SERVICE_NAME, AwmWindowRedrawReady::new());
    }

    pub fn commit_partial(&self) {
        amc_message_send(
            AwmWindow::AWM_SERVICE_NAME,
            AwmWindowPartialRedraw::new(&self.damaged_rects.borrow_mut().drain(..).collect()),
        );
    }

    pub fn set_title(title: &str) {
        amc_message_send(
            AwmWindow::AWM_SERVICE_NAME,
            AwmWindowUpdateTitle::new(title),
        );
    }

    fn key_down(&self, event: &KeyDown) {
        //printf!("Key down: {:?}\n", event);
        // TODO(PT): One element should have keyboard focus at a time. How to select?
        let elems = self.ui_elements.borrow();
        for elem in elems.iter() {
            elem.handle_key_pressed(KeyCode(event.key));
        }
        // TODO(PT): Hack
        self.draw();
        self.commit();
    }

    fn key_up(&self, event: &KeyUp) {
        printf!("Key up: {:?}\n", event);
        // TODO(PT): One element should have keyboard focus at a time. How to select?
        let elems = self.ui_elements.borrow();
        for elem in elems.iter() {
            elem.handle_key_released(KeyCode(event.key));
        }
    }

    fn mouse_moved(&self, event: &MouseMoved) {
        let mouse_point = Point::from(event.mouse_pos);
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
                    // TODO(PT): Remove
                    self.commit();
                }
            } else if elem_contains_mouse {
                elem.handle_mouse_entered();
                elems_containing_mouse.push(Rc::clone(elem));
                // TODO(PT): Remove
                self.commit();
            }
        }

        for elem in elems_containing_mouse {
            // Translate the mouse position to the element's coordinate system
            let elem_pos = mouse_point - elem.frame().origin;
            /*
            printf!(
                "Window.mouse_moved({mouse_point:?}, elem.frame.origin {:?}\n",
                elem.frame().origin
            );
            */
            elem.handle_mouse_moved(elem_pos);
        }

        //self.draw();
        // TODO(PT): Remove
        self.commit();
    }

    fn mouse_dragged(&self, _event: &MouseDragged) {
        //printf!("Mouse dragged: {:?}\n", event);
    }

    fn mouse_left_click_down(&self, event: &MouseLeftClickStarted) {
        printf!("Mouse left click started: {:?}\n", event);

        let mut clicked_elem = None;
        {
            let elems = &*self.ui_elements.borrow();
            for elem in elems {
                printf!("Checking if elem contains point...\n");
                if elem.frame().contains(Point::from(event.mouse_pos)) {
                    printf!("Found UI element that bounds click point, dispatching\n");
                    clicked_elem = Some(Rc::clone(elem));
                    break;
                }
            }
        }

        if let Some(c) = clicked_elem {
            // Translate the mouse position to the element's coordinate system
            let mouse_point = Point::from(event.mouse_pos);
            let elem_pos = mouse_point - c.frame().origin;
            c.handle_left_click(elem_pos);
        }

        self.draw();
        self.commit();
    }

    fn mouse_left_click_up(&self, event: &MouseLeftClickEnded) {
        //printf!("Mouse left click ended: {:?}\n", event);
    }

    fn mouse_entered(&self, event: &MouseEntered) {
        //printf!("Mouse entered: {:?}\n", event);
    }

    fn mouse_exited(&self, event: &MouseExited) {
        //printf!("Mouse exited: {:?}\n", event);
        let elems_containing_mouse = &mut *self.elements_containing_mouse.borrow_mut();
        for elem in elems_containing_mouse.drain(..) {
            elem.handle_mouse_exited();
        }
        self.commit();
    }

    fn mouse_scrolled(&self, event: &MouseScrolled) {
        let elems_containing_mouse = &self.elements_containing_mouse.borrow();
        for elem in elems_containing_mouse.iter() {
            elem.handle_mouse_scrolled(Point::from(event.mouse_point), event.delta_z as _);
        }
        /*
        let ui_elements = self.ui_elements.borrow();
        for elem in ui_elements.iter() {
            elem.handle_mouse_scrolled(Point::from(event.mouse_point), event.delta_z as _);
        }
        */
        // TODO(PT): Testing scroll views
        self.draw();
        self.commit();
    }

    fn window_resized(&self, event: &WindowResized) {
        // TODO(PT): The below comment seems untrue?
        // Don't commit the window here as we'll receive tons of resize events
        // In the future, we can present a 'blur UI' while the window resizes
        {
            let mut size = self.current_size.borrow_mut();
            *size = Size::from(&event.new_size);
        }

        self.resize_subviews();
    }

    fn window_resize_ended(&self, event: &WindowResizeEnded) {
        printf!("Window resize ended: {:?}\n", event);
        self.resize_subviews();
    }

    pub fn resize_subviews(&self) {
        let elems = &*self.ui_elements.borrow();
        for elem in elems {
            elem.handle_superview_resize(*self.current_size.borrow());
        }
        self.draw();
        self.commit();
    }

    fn window_close_requested(&self, event: &WindowCloseRequested) {
        printf!("Window close requested: {:?}\n", event);
        unsafe { axle_rt::libc::exit(0) }
    }

    unsafe fn body_as_type_unchecked<T: AwmWindowEvent>(body: &[u8]) -> &T {
        &*(body.as_ptr() as *const T)
    }

    pub fn await_next_event(&self) {
        let msg_unparsed: AmcMessage<[u8]> = unsafe { amc_message_await_untyped(None).unwrap() };

        if msg_unparsed.source() == AwmWindow::AWM_SERVICE_NAME {
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
            let consumed = unsafe {
                match event {
                    // Keyboard events
                    KeyDown::EXPECTED_EVENT => {
                        self.key_down(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    KeyUp::EXPECTED_EVENT => {
                        self.key_up(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    // Mouse events
                    MouseMoved::EXPECTED_EVENT => {
                        self.mouse_moved(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    MouseDragged::EXPECTED_EVENT => {
                        self.mouse_dragged(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    MouseScrolled::EXPECTED_EVENT => {
                        self.mouse_scrolled(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    MouseLeftClickStarted::EXPECTED_EVENT => {
                        self.mouse_left_click_down(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    MouseLeftClickEnded::EXPECTED_EVENT => {
                        self.mouse_left_click_up(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    // Mouse focus events
                    MouseEntered::EXPECTED_EVENT => {
                        self.mouse_entered(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    MouseExited::EXPECTED_EVENT => {
                        self.mouse_exited(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    // Window events
                    WindowResized::EXPECTED_EVENT => {
                        self.window_resized(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    WindowResizeEnded::EXPECTED_EVENT => {
                        self.window_resize_ended(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    WindowCloseRequested::EXPECTED_EVENT => {
                        self.window_close_requested(AwmWindow::body_as_type_unchecked(raw_body));
                        true
                    }
                    _ => {
                        // Forwarded to generic message handler down below
                        false
                    }
                }
            };
            if consumed {
                return;
            }
        }

        // Has the app set up a generic message handler?
        let cb = &*self.amc_message_cb.borrow_mut();
        if let Some(cb) = cb {
            printf!("Dispatching message to custom message handler!\n");
            cb(self, msg_unparsed);
        } else {
            printf!("Dropping unknown message because no custom message handler is set up\n");
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

    pub fn add_message_handler<F: 'static + Fn(&Self, AmcMessage<[u8]>)>(
        &self,
        message_handler: F,
    ) {
        *self.amc_message_cb.borrow_mut() = Some(Box::new(message_handler));
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
