use core::cell::RefCell;

use agx_definitions::{
    Color, DrawThickness, Drawable, Layer, LayerSlice, Line, Point, Rect, SingleFramebufferLayer,
    Size,
};
use alloc::vec;
use alloc::{boxed::Box, vec::Vec};
use alloc::{
    rc::Rc,
    string::{Drain, String, ToString},
};

use crate::{bordered::Bordered, font::draw_char, window::AwmWindow};
use axle_rt::printf;

pub trait UIElement: Drawable {
    fn handle_mouse_entered(&self, onto: &mut LayerSlice) {}
    fn handle_mouse_exited(&self, onto: &mut LayerSlice) {}
    fn handle_mouse_moved(&self, mouse_point: Point, onto: &mut LayerSlice) {}

    fn handle_left_click(&self) {}

    fn handle_superview_resize(&self, superview_size: Size) {}

    fn currently_contains_mouse(&self) -> bool {
        false
    }
}

// Perhaps a ContainsDrawable trait and a Drawable trait
// A button containing a label has both, but a label only has Drawable
//
// Resizable trait?

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
    pub fn new<F: 'static + Fn(&Self, Size) -> Rect>(
        frame: Rect,
        background_color: Color,
        sizer: F,
    ) -> Self {
        View {
            frame: RefCell::new(frame),
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
    fn draw_inner_content(&self, onto: &mut LayerSlice) {
        let mut inner_content_rect_ref = self.current_inner_content_frame.borrow_mut();
        *inner_content_rect_ref = onto.frame;

        onto.fill(self.background_color);

        let sub_elements = &self.sub_elements.borrow();
        for elem in sub_elements.iter() {
            let mut slice = onto.get_slice(elem.frame());
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

pub struct Button {
    frame: Rect,
    pub label: String,
    left_click_cb: RefCell<Option<Box<dyn Fn(&Self)>>>,
    currently_contains_mouse_int: RefCell<bool>,
}

impl Button {
    pub fn new(frame: Rect, label: &str) -> Self {
        Button {
            frame,
            label: label.to_string(),
            left_click_cb: RefCell::new(None),
            currently_contains_mouse_int: RefCell::new(false),
        }
    }
    pub fn on_left_click<F: 'static + Fn(&Self)>(&self, f: F) {
        *self.left_click_cb.borrow_mut() = Some(Box::new(f));
    }
}

impl Bordered for Button {
    fn draw_border(&self, onto: &mut LayerSlice) -> Rect {
        let outer_margin_size = 4;
        let outer_border = Rect::from_parts(Point::zero(), self.frame().size);

        let border_color = match self.currently_contains_mouse() {
            true => Color::new(200, 200, 200),
            false => Color::new(50, 50, 50),
        };
        onto.fill_rect(outer_border, border_color, DrawThickness::PartialFill(1));

        let outer_margin = outer_border.inset_by(1, 1, 1, 1);
        onto.fill_rect(
            outer_margin,
            Color::new(60, 60, 60),
            DrawThickness::PartialFill(outer_margin_size),
        );

        let inner_border = outer_margin.inset_by(
            outer_margin_size,
            outer_margin_size,
            outer_margin_size,
            outer_margin_size,
        );

        let inner_border_color = match self.currently_contains_mouse() {
            true => Color::white(),
            false => Color::new(140, 140, 140),
        };
        onto.fill_rect(
            inner_border,
            inner_border_color,
            DrawThickness::PartialFill(1),
        );

        // Draw diagonal lines indicating an inset
        let inset_color = Color::new(100, 100, 100);
        let inset_width = outer_margin_size / 2;
        let outer_margin_as_point = Point::new(outer_margin_size, outer_margin_size);

        // Account for our line thickness algo
        let fine_x = Point::new(1, 0);
        let fine_y = Point::new(0, 1);
        let x_adjustment = Point::new(inset_width / 2, 0);

        let top_left_inset_start = outer_margin.origin;
        let top_left_inset = Line::new(
            top_left_inset_start + x_adjustment,
            top_left_inset_start + outer_margin_as_point + x_adjustment + fine_x + fine_y,
        );
        top_left_inset.draw(onto, inset_color, DrawThickness::PartialFill(inset_width));

        let top_right_inset_start = Point::new(outer_margin.max_x(), outer_margin.min_y());
        let top_right_inset = Line::new(
            top_right_inset_start - x_adjustment,
            Point::new(
                top_right_inset_start.x - outer_margin_size,
                top_right_inset_start.y + outer_margin_size,
            ) - x_adjustment
                + fine_y,
        );
        top_right_inset.draw(onto, inset_color, DrawThickness::PartialFill(inset_width));

        let bottom_left_inset_start = Point::new(outer_margin.min_x(), outer_margin.max_y());
        let bottom_left_inset = Line::new(
            bottom_left_inset_start + x_adjustment - fine_y,
            Point::new(
                bottom_left_inset_start.x + outer_margin_size,
                bottom_left_inset_start.y - outer_margin_size,
            ) + x_adjustment
                - (fine_y * 2),
        );
        bottom_left_inset.draw(onto, inset_color, DrawThickness::PartialFill(inset_width));

        let bottom_right_inset_start = Point::new(outer_margin.max_x(), outer_margin.max_y());
        let bottom_right_inset = Line::new(
            bottom_right_inset_start - x_adjustment - fine_y,
            bottom_right_inset_start - outer_margin_as_point - x_adjustment - (fine_y * 2),
        );
        bottom_right_inset.draw(onto, inset_color, DrawThickness::PartialFill(inset_width));

        let content_frame = inner_border.inset_by(1, 1, 1, 1);
        content_frame
    }

    fn draw_inner_content(&self, onto: &mut LayerSlice) {
        onto.fill(Color::light_gray());

        let draw_color = Color::black();
        let font_size = Size::new(8, 8);
        let label_size = Size::new(
            self.label.len() as isize * font_size.width,
            font_size.height,
        );

        let midpoint = Point::new(
            onto.frame.size.width / 2.0 as isize,
            onto.frame.size.height / 2.0 as isize,
        );
        let label_origin = midpoint
            - Point::new(
                (label_size.width as f64 / 2f64) as isize,
                (label_size.height as f64 / 2f64) as isize,
            );

        let mut cursor = label_origin;
        for ch in self.label.chars() {
            draw_char(onto, ch, &cursor, draw_color, &font_size);
            cursor.x += font_size.width;
        }
    }
}

impl Drawable for Button {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn draw(&self, onto: &mut LayerSlice) {
        Bordered::draw(self, onto);
    }
}

impl UIElement for Button {
    fn handle_left_click(&self) {
        let maybe_cb = &*self.left_click_cb.borrow();
        if let Some(cb) = maybe_cb {
            (cb)(self);
        }
    }

    fn handle_mouse_entered(&self, onto: &mut LayerSlice) {
        *self.currently_contains_mouse_int.borrow_mut() = true;
        /*
        self.queue_partial_redraw(|onto: LayerSlice| {
            Bordered::draw_border(Rc::clone(&self), onto);
        });
        */
        Bordered::draw_border(self, onto);
    }

    fn handle_mouse_exited(&self, onto: &mut LayerSlice) {
        *self.currently_contains_mouse_int.borrow_mut() = false;
        Bordered::draw_border(self, onto);
    }

    fn handle_mouse_moved(&self, mouse_point: Point, onto: &mut LayerSlice) {}

    fn currently_contains_mouse(&self) -> bool {
        *self.currently_contains_mouse_int.borrow()
    }
}

pub struct Label {
    frame: Rect,
    pub text: String,
    color: Color,
}

impl Label {
    pub fn new(frame: Rect, text: &str, color: Color) -> Self {
        let max_size = Size::new(600, 480);
        Label {
            frame,
            text: text.to_string(),
            color,
        }
    }
}

impl UIElement for Label {}

impl Drawable for Label {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn draw(&self, onto: &mut LayerSlice) {
        let font_size = Size::new(8, 8);
        let mut cursor = self.frame.origin;
        for ch in self.text.chars() {
            //draw_char(onto, ch, &cursor, self.color, &font_size);
            cursor.x += font_size.width;
        }
    }
}

/*
Goal: Scrollable list of buttons

Goal: Scrollable terminal with backlog
    Memory usage will grow unbounded if we keep stitching more layers together
    Instead, we need two representations:
        Growable list of the stored text
        Several

Differentiate between scrollable view and scrollable layer?
    Consider:
        Image.render_onto(Layer)
    If the layer is actually backed by several layers stitched together, how does this API work?

    Similar to libgui,
*/

/*
pub struct ScrollView {
    // Only refers to the visible portion
    frame: Rect,
    layer: ScrollableLayer,
}

impl ScrollView {
    pub fn new(frame: Rect, text: &str, color: Color) -> Self {
        ScrollView {
            frame,
            layer: ScrollableLayer::new(frame.size),
        }
    }
}

impl UIElement for ScrollView {
    fn frame(&self) -> Rect {
        self.frame
    }
}

impl Drawable for ScrollView {
    fn draw(&self, onto: &mut dyn Layer) {
    }
}
*/
