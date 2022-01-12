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
        onto.fill_rect(outer_border, border_color, StrokeThickness::Width(1));

        let outer_margin = outer_border.inset_by(1, 1, 1, 1);
        onto.fill_rect(
            outer_margin,
            Color::new(60, 60, 60),
            StrokeThickness::Width(outer_margin_size),
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
        onto.fill_rect(inner_border, inner_border_color, StrokeThickness::Width(1));

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
        top_left_inset.draw(onto, inset_color, StrokeThickness::Width(inset_width));

        let top_right_inset_start = Point::new(outer_margin.max_x(), outer_margin.min_y());
        let top_right_inset = Line::new(
            top_right_inset_start - x_adjustment,
            Point::new(
                top_right_inset_start.x - outer_margin_size,
                top_right_inset_start.y + outer_margin_size,
            ) - x_adjustment
                + fine_y,
        );
        top_right_inset.draw(onto, inset_color, StrokeThickness::Width(inset_width));

        let bottom_left_inset_start = Point::new(outer_margin.min_x(), outer_margin.max_y());
        let bottom_left_inset = Line::new(
            bottom_left_inset_start + x_adjustment - fine_y,
            Point::new(
                bottom_left_inset_start.x + outer_margin_size,
                bottom_left_inset_start.y - outer_margin_size,
            ) + x_adjustment
                - (fine_y * 2),
        );
        bottom_left_inset.draw(onto, inset_color, StrokeThickness::Width(inset_width));

        let bottom_right_inset_start = Point::new(outer_margin.max_x(), outer_margin.max_y());
        let bottom_right_inset = Line::new(
            bottom_right_inset_start - x_adjustment - fine_y,
            bottom_right_inset_start - outer_margin_as_point - x_adjustment - (fine_y * 2),
        );
        bottom_right_inset.draw(onto, inset_color, StrokeThickness::Width(inset_width));

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
