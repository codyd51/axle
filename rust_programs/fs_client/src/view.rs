use core::cell::RefCell;

use agx_definitions::{
    Color, Drawable, Layer, LayerSlice, Line, Point, Rect, SingleFramebufferLayer, Size,
    StrokeThickness,
};
use alloc::vec;
use alloc::{boxed::Box, vec::Vec};
use alloc::{
    rc::Rc,
    string::{Drain, String, ToString},
};

use crate::{bordered::Bordered, font::draw_char, ui_elements::UIElement, window::AwmWindow};
use axle_rt::printf;

pub struct View {
    frame: RefCell<Rect>,
    left_click_cb: RefCell<Option<Box<dyn Fn(&Self)>>>,
    background_color: Color,
    sizer: RefCell<Box<dyn Fn(&Self, Size) -> Rect>>,
    currently_contains_mouse_int: RefCell<bool>,

    current_inner_content_frame: RefCell<Rect>,

    sub_elements: RefCell<Vec<Rc<dyn UIElement>>>,
    sub_elements_containing_mouse: RefCell<Vec<Rc<dyn UIElement>>>,
}

impl View {
    pub fn new<F: 'static + Fn(&Self, Size) -> Rect>(background_color: Color, sizer: F) -> Self {
        View {
            frame: RefCell::new(Rect::zero()),
            current_inner_content_frame: RefCell::new(Rect::zero()),
            left_click_cb: RefCell::new(None),
            background_color,
            sizer: RefCell::new(Box::new(sizer)),
            currently_contains_mouse_int: RefCell::new(false),
            sub_elements: RefCell::new(Vec::new()),
            sub_elements_containing_mouse: RefCell::new(Vec::new()),
        }
    }

    pub fn on_left_click<F: 'static + Fn(&Self)>(&self, f: F) {
        *self.left_click_cb.borrow_mut() = Some(Box::new(f));
    }

    pub fn add_component(&self, elem: Rc<dyn UIElement>) {
        self.sub_elements.borrow_mut().push(elem);
    }
}

impl Bordered for View {
    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut LayerSlice) {
        let mut inner_content_rect_ref = self.current_inner_content_frame.borrow_mut();
        *inner_content_rect_ref = onto.frame;

        onto.fill(self.background_color);

        let sub_elements = &self.sub_elements.borrow();
        /*
        printf!(
            "DrawInnerContent for view {:?}, layer slice {:?}\n",
            self.frame(),
            onto.frame
        );
        */
        for elem in sub_elements.iter() {
            //printf!("Getting slice for elem {:?}\n", elem.frame());
            let mut slice = onto.get_slice(elem.frame());
            //printf!("Got slice with frame {:?}\n", slice.frame);
            elem.draw(&mut slice);
        }
    }
}

impl Drawable for View {
    fn frame(&self) -> Rect {
        *self.frame.borrow()
    }

    fn draw(&self, onto: &mut LayerSlice) {
        Bordered::draw(self, onto);
    }
}

impl UIElement for View {
    fn handle_left_click(&self) {
        let maybe_cb = &*self.left_click_cb.borrow();
        if let Some(cb) = maybe_cb {
            (cb)(self);
        }

        let mut elems_containing_mouse = &mut *self.sub_elements_containing_mouse.borrow_mut();
        for elem in elems_containing_mouse {
            elem.handle_left_click();
        }
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        let mut frame_mut = self.frame.borrow_mut();
        let sizer = &*self.sizer.borrow();
        let frame = sizer(self, superview_size);
        *frame_mut = frame;
    }

    fn handle_mouse_entered(&self, onto: &mut LayerSlice) {
        printf!("Mouse entered view!\n");
        *self.currently_contains_mouse_int.borrow_mut() = true;
        Bordered::draw_border(self, onto);
    }

    fn handle_mouse_exited(&self, onto: &mut LayerSlice) {
        printf!("Mouse exited view!\n");
        *self.currently_contains_mouse_int.borrow_mut() = false;

        let mut elems_containing_mouse = &mut *self.sub_elements_containing_mouse.borrow_mut();
        for elem in elems_containing_mouse.drain(..) {
            let mut slice = onto.get_slice(elem.frame());
            elem.handle_mouse_exited(&mut slice);
        }
    }

    fn handle_mouse_moved(&self, mouse_point: Point, onto: &mut LayerSlice) {
        //printf!("Mouse moved in view! {:?}\n", mouse_point);
        let elems = &*self.sub_elements.borrow();
        let mut elems_containing_mouse = &mut *self.sub_elements_containing_mouse.borrow_mut();

        let inner_content_origin = (*self.current_inner_content_frame.borrow()).origin;

        for elem in elems {
            let mut slice = onto.get_slice(Rect::from_parts(
                elem.frame().origin + inner_content_origin,
                elem.frame().size,
            ));
            //let mut slice = onto.get_slice(elem.frame());
            let elem_contains_mouse = elem.frame().contains(mouse_point);

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
                let mut slice = onto.get_slice(Rect::from_parts(
                    elem.frame().origin + inner_content_origin,
                    elem.frame().size,
                ));
                elem.handle_mouse_entered(&mut slice);
                elems_containing_mouse.push(Rc::clone(elem));
            }
        }

        for elem in elems_containing_mouse {
            elem.handle_mouse_moved(mouse_point, onto);
        }
    }

    fn currently_contains_mouse(&self) -> bool {
        *self.currently_contains_mouse_int.borrow()
    }
}
