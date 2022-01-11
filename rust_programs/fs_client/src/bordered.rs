use core::cell::RefCell;

use agx_definitions::{
    Color, DrawThickness, Drawable, Layer, LayerSlice, Line, Point, Rect, SingleFramebufferLayer,
    Size,
};
use alloc::string::{Drain, String, ToString};
use alloc::vec;
use alloc::{boxed::Box, vec::Vec};

use crate::{font::draw_char, window::AwmWindow, UIElement};

pub trait Bordered: Drawable + UIElement {
    fn draw(&self, onto: &mut LayerSlice) {
        let inner_content_frame = self.draw_border(onto);
        let mut content_slice = onto.get_slice(inner_content_frame);
        self.draw_inner_content(&mut content_slice);
    }

    fn draw_border(&self, onto: &mut LayerSlice) -> Rect {
        let outer_margin_size = 6;
        let inner_margin_size = 6;

        let outer_border = Rect::from_parts(Point::zero(), self.frame().size);

        let border_color = match self.currently_contains_mouse() {
            true => Color::new(200, 200, 200),
            false => Color::dark_gray(),
        };
        onto.fill_rect(outer_border, border_color, DrawThickness::PartialFill(1));

        let outer_margin = outer_border.inset_by(1, 1, 1, 1);
        let inner_margin = outer_margin.inset_by(
            outer_margin_size,
            outer_margin_size,
            outer_margin_size,
            outer_margin_size,
        );

        onto.fill_rect(
            outer_margin,
            Color::light_gray(),
            DrawThickness::PartialFill(outer_margin_size),
        );
        onto.fill_rect(
            inner_margin,
            Color::dark_gray(),
            DrawThickness::PartialFill(inner_margin_size),
        );

        // Draw diagonal lines indicating an inset
        let inset_color = Color::new(50, 50, 50);
        let inner_margin_as_point = Point::new(inner_margin_size, inner_margin_size);

        // Account for our line thickness algo
        let fine_x = Point::new(1, 0);
        let fine_y = Point::new(0, 1);
        let x_adjustment = Point::new(inner_margin_size / 2, 0);

        let top_left_inset_start = inner_margin.origin;
        let top_left_inset = Line::new(
            top_left_inset_start + x_adjustment,
            top_left_inset_start + inner_margin_as_point + x_adjustment + fine_x + fine_y,
        );
        top_left_inset.draw(
            onto,
            inset_color,
            DrawThickness::PartialFill(inner_margin_size),
        );

        let top_right_inset_start = Point::new(inner_margin.max_x(), inner_margin.min_y());
        let top_right_inset = Line::new(
            top_right_inset_start - x_adjustment,
            Point::new(
                top_right_inset_start.x - inner_margin_size,
                top_right_inset_start.y + inner_margin_size,
            ) - x_adjustment
                - fine_x
                + fine_y,
        );
        top_right_inset.draw(
            onto,
            inset_color,
            DrawThickness::PartialFill(inner_margin_size),
        );

        let bottom_left_inset_start = Point::new(inner_margin.min_x(), inner_margin.max_y());
        let bottom_left_inset = Line::new(
            bottom_left_inset_start + x_adjustment - fine_y,
            Point::new(
                bottom_left_inset_start.x + inner_margin_size,
                bottom_left_inset_start.y - inner_margin_size,
            ) + x_adjustment
                - (fine_y * 2),
        );
        bottom_left_inset.draw(
            onto,
            inset_color,
            DrawThickness::PartialFill(inner_margin_size),
        );

        let bottom_right_inset_start = Point::new(inner_margin.max_x(), inner_margin.max_y());
        let bottom_right_inset = Line::new(
            bottom_right_inset_start - x_adjustment - fine_y,
            bottom_right_inset_start - inner_margin_as_point - x_adjustment - (fine_y * 2),
        );
        bottom_right_inset.draw(
            onto,
            inset_color,
            DrawThickness::PartialFill(inner_margin_size),
        );
        let inner_content = inner_margin.inset_by(
            inner_margin_size,
            inner_margin_size,
            inner_margin_size,
            inner_margin_size,
        );
        inner_content
    }

    fn draw_inner_content(&self, onto: &mut LayerSlice);
}