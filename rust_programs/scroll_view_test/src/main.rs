#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate libc;

use alloc::{boxed::Box, collections::BTreeMap, format, rc::Weak, vec::Vec};
use alloc::{
    rc::Rc,
    string::{String, ToString},
};
use core::{cell::RefCell, cmp};

use libgui::bordered::Bordered;
use libgui::button::Button;
use libgui::label::Label;
use libgui::ui_elements::UIElement;
use libgui::view::View;
use libgui::window::AwmWindow;

use axle_rt::{amc_message_await, amc_message_send, amc_register_service, printf, AmcMessage};

use agx_definitions::{
    Color, Drawable, LayerSlice, Line, NestedLayerSlice, Point, Rect, RectInsets, Size,
    StrokeThickness,
};

use file_manager_messages::{
    str_from_u8_nul_utf8_unchecked, DirectoryContents, DirectoryEntry, LaunchProgram,
    ReadDirectory, FILE_SERVER_SERVICE_NAME,
};

struct ScrollView {
    container: RefCell<Option<RefCell<Weak<dyn NestedLayerSlice>>>>,
    pub frame: RefCell<Rect>,
    left_click_cb: RefCell<Option<Box<dyn Fn(&Self)>>>,
    background_color: Color,
    sizer: RefCell<Box<dyn Fn(&Self, Size) -> Rect>>,
    currently_contains_mouse_int: RefCell<bool>,

    sub_elements: RefCell<Vec<Rc<dyn UIElement>>>,
}

impl ScrollView {
    pub fn new<F: 'static + Fn(&Self, Size) -> Rect>(background_color: Color, sizer: F) -> Self {
        ScrollView {
            container: RefCell::new(None),
            frame: RefCell::new(Rect::zero()),
            left_click_cb: RefCell::new(None),
            background_color,
            sizer: RefCell::new(Box::new(sizer)),
            currently_contains_mouse_int: RefCell::new(false),
            sub_elements: RefCell::new(Vec::new()),
        }
    }

    pub fn set_border_enabled(&self, enabled: bool) {
        todo!();
    }

    pub fn on_left_click<F: 'static + Fn(&Self)>(&self, f: F) {
        *self.left_click_cb.borrow_mut() = Some(Box::new(f));
    }

    pub fn add_component(self: Rc<Self>, elem: Rc<dyn UIElement>) {
        // Ensure the component has a frame by running its sizer
        elem.handle_superview_resize(Bordered::content_frame(&*self).size);

        // Set up a link to the parent
        elem.set_parent(Rc::downgrade(&(Rc::clone(&self) as _)));

        self.sub_elements.borrow_mut().push(elem);
    }
}

impl NestedLayerSlice for ScrollView {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        Some(Weak::clone(
            &self.container.borrow().as_ref().unwrap().borrow(),
        ))
    }

    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
        self.container.replace(Some(RefCell::new(parent)));
    }
}

impl Bordered for ScrollView {
    fn draw_inner_content(&self, _outer_frame: Rect, onto: &mut LayerSlice) {
        onto.fill(self.background_color);

        let sub_elements = &self.sub_elements.borrow();
        for elem in sub_elements.iter() {
            elem.draw();
        }
    }

    fn border_enabled(&self) -> bool {
        true
    }

    fn border_insets(&self) -> RectInsets {
        RectInsets::new(11, 11, 11, 16)
    }
}

impl Drawable for ScrollView {
    fn frame(&self) -> Rect {
        *self.frame.borrow()
    }

    fn content_frame(&self) -> Rect {
        Bordered::content_frame(self)
    }

    fn draw(&self) {
        //panic!("View is drawn via Bordered::draw()");
        Bordered::draw(self);
    }
}

impl UIElement for ScrollView {
    fn handle_left_click(&self, _mouse_point: Point) {
        todo!();
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        let sizer = &*self.sizer.borrow();
        let frame = sizer(self, superview_size);
        self.frame.replace(frame);

        let inner_content_frame = frame.apply_insets(self.border_insets());

        let elems = &*self.sub_elements.borrow();
        for elem in elems {
            elem.handle_superview_resize(inner_content_frame.size);
        }
    }

    fn handle_mouse_entered(&self) {
        *self.currently_contains_mouse_int.borrow_mut() = true;
        self.draw_border();
    }

    fn handle_mouse_exited(&self) {
        *self.currently_contains_mouse_int.borrow_mut() = false;

        self.draw_border();
    }

    fn handle_mouse_moved(&self, mouse_point: Point) {}

    fn currently_contains_mouse(&self) -> bool {
        *self.currently_contains_mouse_int.borrow()
    }
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service("com.axle.scroll_view_test");

    let window = Rc::new(AwmWindow::new("Scroll View Test", Size::new(800, 400)));

    let scroll_view = Rc::new(ScrollView::new(Color::new(200, 40, 40), |v, size| {
        Rect::new(40, 20, size.width - 80, size.height - 40)
    }));
    let mut cursor = Point::new(60, -25);
    for i in 0..7 {
        let v = Rc::new(View::new(Color::blue(), move |v, s| {
            Rect::from_parts(cursor, Size::new(50, 50))
        }));
        Rc::clone(&scroll_view).add_component(Rc::clone(&v) as Rc<dyn UIElement>);

        cursor.y += 60;
    }
    Rc::clone(&window).add_component(Rc::clone(&scroll_view) as Rc<dyn UIElement>);

    window.enter_event_loop();
    0
}
