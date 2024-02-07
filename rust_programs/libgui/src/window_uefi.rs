use alloc::vec;
use alloc::{boxed::Box, rc::Rc};
use alloc::{rc::Weak, vec::Vec};

use core::cell::RefCell;

use axle_rt::core_commands::{AmcSleepUntilDelayOrMessage, AMC_CORE_SERVICE_NAME};

use axle_rt::{printf, ExpectsEventField};

use agx_definitions::{
    Drawable, Layer, LikeLayerSlice, NestedLayerSlice, PixelByteLayout, Point, Rect,
    SingleFramebufferLayer, Size,
};

use crate::ui_elements::*;
use crate::window_events::*;

pub struct AwmWindow {
    pub layer: RefCell<SingleFramebufferLayer>,
    pub current_size: RefCell<Size>,
    ui_elements: RefCell<Vec<Rc<dyn UIElement>>>,
    ui_elements_containing_mouse: RefCell<Vec<Rc<dyn UIElement>>>,
}

impl AwmWindow {
    pub fn new(size: Size) -> Rc<Self> {
        // PT: Assume 4 bytes per pixel everywhere...
        let bpp = 4;
        let framebuffer =
            vec![0; (size.width * size.height * (bpp as isize)) as usize].into_boxed_slice();
        let layer = RefCell::new(SingleFramebufferLayer::from_framebuffer(
            framebuffer,
            bpp,
            size,
            PixelByteLayout::BGRA,
        ));

        Rc::new(Self {
            layer,
            current_size: RefCell::new(size),
            ui_elements: RefCell::new(vec![]),
            ui_elements_containing_mouse: RefCell::new(vec![]),
        })
    }

    pub fn add_component(self: Rc<Self>, elem: Rc<dyn UIElement>) {
        // Ensure the component has a frame by running its sizer
        elem.handle_superview_resize(*self.current_size.borrow());
        // Set up a link to the parent
        elem.set_parent(Rc::downgrade(&(Rc::clone(&self) as _)));
        self.ui_elements.borrow_mut().push(elem);
    }

    pub fn draw(&self) {
        let elems = &*self.ui_elements.borrow();
        for elem in elems {
            elem.draw();
        }
    }

    pub fn commit(&self) {}

    pub fn resize_subviews(&self) {
        let elems = &*self.ui_elements.borrow();
        for elem in elems {
            elem.handle_superview_resize(*self.current_size.borrow());
        }
        self.draw();
        self.commit();
    }

    pub fn handle_key_pressed(&self, key_code: KeyCode) {
        let elems = self.ui_elements.borrow();
        for elem in elems.iter() {
            elem.handle_key_pressed(key_code);
        }
    }

    pub fn handle_key_released(&self, key_code: KeyCode) {
        let elems = self.ui_elements.borrow();
        for elem in elems.iter() {
            elem.handle_key_released(key_code);
        }
    }

    pub fn handle_mouse_moved(&self, mouse_point: Point) {
        let elems = &*self.ui_elements.borrow();
        let elems_containing_mouse = &mut *self.ui_elements_containing_mouse.borrow_mut();

        // First up, send entered/exited events for each element
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

    pub fn handle_mouse_left_click_down(&self, mouse_pos: Point) {
        let mut clicked_elem = None;
        {
            let elems = &*self.ui_elements.borrow();
            for elem in elems {
                if elem.frame().contains(Point::from(mouse_pos)) {
                    clicked_elem = Some(Rc::clone(elem));
                    break;
                }
            }
        }

        if let Some(c) = clicked_elem {
            // Translate the mouse position to the element's coordinate system
            let mouse_point = Point::from(mouse_pos);
            let elem_pos = mouse_point - c.frame().origin;
            c.handle_left_click(elem_pos);
        }
    }

    pub fn handle_mouse_left_click_up(&self, mouse_pos: Point) {
        // Nothing to do
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
