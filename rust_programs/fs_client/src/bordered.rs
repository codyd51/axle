use agx_definitions::{
    Color, Drawable, LayerSlice, Line, Point, Rect, RectInsets, StrokeThickness,
};

use crate::UIElement;

pub trait Bordered: Drawable + UIElement {
    fn draw(&self) {
        let mut slice = self.get_slice();

        if !self.border_enabled() {
            self.set_interior_content_frame(Rect::from_parts(Point::zero(), slice.frame.size));
            self.draw_inner_content(slice.frame, &mut slice);
        } else {
            let inner_content_frame = self.draw_border();
            self.set_interior_content_frame(inner_content_frame);

            let mut content_slice = slice.get_slice(inner_content_frame);
            self.draw_inner_content(slice.frame, &mut content_slice);
        }
    }

    fn border_enabled(&self) -> bool {
        true
    }

    fn border_insets(&self) -> RectInsets;

    // TODO(PT): Can we remove the need for this by using border_insets()?
    fn set_interior_content_frame(&self, inner_content_frame: Rect);
    fn get_interior_content_frame(&self) -> Rect;

    fn content_frame(&self) -> Rect {
        self.get_interior_content_frame()
    }

    fn draw_border(&self) -> Rect {
        if !self.border_enabled() {
            return Rect::from_parts(Point::zero(), self.frame().size);
        }

        let onto = &mut self.get_slice();
        let insets = self.border_insets();

        // TODO(PT): This currently assumes an even inset across all sides
        // Verify this assumption first
        assert!(
            insets.left == insets.top
                && insets.left == insets.right
                && insets.left == insets.bottom
        );

        let highlight_border_width = 1;
        let border_effect_size = insets.top / 2;
        let outer_margin_size = border_effect_size;
        let inner_margin_size = border_effect_size;

        let outer_border = Rect::from_parts(Point::zero(), self.frame().size);

        let border_color = match self.currently_contains_mouse() {
            true => Color::new(200, 200, 200),
            false => Color::dark_gray(),
        };
        onto.fill_rect(
            outer_border,
            border_color,
            StrokeThickness::Width(highlight_border_width),
        );

        let outer_margin = outer_border.inset_by(
            highlight_border_width,
            highlight_border_width,
            highlight_border_width,
            highlight_border_width,
        );
        let inner_margin = outer_margin.inset_by(
            outer_margin_size,
            outer_margin_size,
            outer_margin_size,
            outer_margin_size,
        );

        onto.fill_rect(
            outer_margin,
            Color::light_gray(),
            StrokeThickness::Width(outer_margin_size),
        );
        onto.fill_rect(
            inner_margin,
            Color::dark_gray(),
            StrokeThickness::Width(inner_margin_size),
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
        top_left_inset.draw(onto, inset_color, StrokeThickness::Width(inner_margin_size));

        let top_right_inset_start = Point::new(inner_margin.max_x() - 1, inner_margin.min_y());
        let top_right_inset = Line::new(
            top_right_inset_start - x_adjustment,
            Point::new(
                top_right_inset_start.x - inner_margin_size,
                top_right_inset_start.y + inner_margin_size,
            ) - x_adjustment
                - fine_x
                + fine_y,
        );
        top_right_inset.draw(onto, inset_color, StrokeThickness::Width(inner_margin_size));

        let bottom_left_inset_start = Point::new(inner_margin.min_x(), inner_margin.max_y());
        let bottom_left_inset = Line::new(
            bottom_left_inset_start + x_adjustment - fine_y,
            Point::new(
                bottom_left_inset_start.x + inner_margin_size,
                bottom_left_inset_start.y - inner_margin_size,
            ) + x_adjustment
                - (fine_y * 2),
        );
        bottom_left_inset.draw(onto, inset_color, StrokeThickness::Width(inner_margin_size));

        let bottom_right_inset_start = Point::new(inner_margin.max_x() - 1, inner_margin.max_y());
        let bottom_right_inset = Line::new(
            bottom_right_inset_start - x_adjustment - fine_y,
            bottom_right_inset_start - inner_margin_as_point - x_adjustment - (fine_y * 2),
        );
        bottom_right_inset.draw(onto, inset_color, StrokeThickness::Width(inner_margin_size));
        let inner_content = inner_margin.inset_by(
            inner_margin_size,
            inner_margin_size,
            inner_margin_size,
            inner_margin_size,
        );
        inner_content
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut LayerSlice);
}
