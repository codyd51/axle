use agx_definitions::{
    Color, Drawable, LikeLayerSlice, Line, Point, Rect, RectInsets, Size, StrokeThickness,
};
use alloc::boxed::Box;
use alloc::rc::Rc;
use alloc::vec::Vec;

use crate::ui_elements::UIElement;

pub fn draw_outer_mouse_highlight(
    onto: &mut Box<dyn LikeLayerSlice>,
    self_size: Size,
    currently_contains_mouse: bool,
) -> Rect {
    let highlight_border_width = 1;
    let outer_border = Rect::from_parts(Point::zero(), self_size);
    let border_color = match currently_contains_mouse {
        true => Color::new(200, 200, 200),
        false => Color::dark_gray(),
    };
    onto.fill_rect(
        outer_border,
        border_color,
        StrokeThickness::Width(highlight_border_width),
    );
    let frame_without_outer_highlight = outer_border.inset_by(
        highlight_border_width,
        highlight_border_width,
        highlight_border_width,
        highlight_border_width,
    );
    frame_without_outer_highlight
}

fn draw_inner_margin(
    onto: &mut Box<dyn LikeLayerSlice>,
    inner_border_insets: RectInsets,
    frame_of_inner_margin: Rect,
) {
    onto.fill_rect(
        frame_of_inner_margin,
        //Color::dark_gray(),
        Color::yellow(),
        StrokeThickness::Width(inner_border_insets.bottom),
    );

    // TODO(PT): Hack that assumes the insets are even along all edges
    let inner_margin_size = inner_border_insets.bottom;

    // Draw diagonal lines indicating an inset
    //let inset_color = Color::new(50, 50, 50);
    let inset_color = Color::blue();
    let inner_margin_as_point = Point::new(inner_margin_size, inner_margin_size);

    // Account for our line thickness algo
    let fine_x = Point::new(1, 0);
    let fine_y = Point::new(0, 1);
    let x_adjustment = Point::new(inner_margin_size / 2, 0);

    let top_left_inset_start = frame_of_inner_margin.origin;
    let top_left_inset = Line::new(
        top_left_inset_start + x_adjustment,
        top_left_inset_start + inner_margin_as_point + x_adjustment + fine_x + fine_y,
    );
    top_left_inset.draw(onto, inset_color, StrokeThickness::Width(inner_margin_size));

    let top_right_inset_start = Point::new(
        frame_of_inner_margin.max_x() - 1,
        frame_of_inner_margin.min_y(),
    );
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

    let bottom_left_inset_start =
        Point::new(frame_of_inner_margin.min_x(), frame_of_inner_margin.max_y());
    let bottom_left_inset = Line::new(
        bottom_left_inset_start + x_adjustment - fine_y,
        Point::new(
            bottom_left_inset_start.x + inner_margin_size,
            bottom_left_inset_start.y - inner_margin_size,
        ) + x_adjustment
            - (fine_y * 2),
    );
    bottom_left_inset.draw(onto, inset_color, StrokeThickness::Width(inner_margin_size));

    let bottom_right_inset_start = Point::new(
        frame_of_inner_margin.max_x() - 1,
        frame_of_inner_margin.max_y(),
    );
    let bottom_right_inset = Line::new(
        bottom_right_inset_start - x_adjustment - fine_y,
        bottom_right_inset_start - inner_margin_as_point - x_adjustment - (fine_y * 2),
    );
    bottom_right_inset.draw(onto, inset_color, StrokeThickness::Width(inner_margin_size));
}

// Extracted so that we can re-use the drawing code within a specialized impl for the scroll view border
pub fn draw_border_with_insets(
    onto: &mut Box<dyn LikeLayerSlice>,
    outer_border_insets: RectInsets,
    inner_border_insets: RectInsets,
    self_size: Size,
    highlight_enabled: bool,
    currently_contains_mouse: bool,
) -> (Rect, Rect) {
    let frame_without_outer_highlight = if highlight_enabled {
        draw_outer_mouse_highlight(onto, self_size, currently_contains_mouse)
    } else {
        Rect::from_parts(Point::zero(), self_size)
    };

    let frame_of_outer_margin = frame_without_outer_highlight;
    onto.fill_rect(
        frame_of_outer_margin,
        Color::light_gray(),
        // TODO(PT): fill_rect supports per-side thickness, so we can respect the thickness that was passed in
        StrokeThickness::Width(outer_border_insets.bottom),
    );

    let frame_of_inner_margin = frame_without_outer_highlight.inset_by(
        outer_border_insets.bottom,
        outer_border_insets.left,
        outer_border_insets.right,
        outer_border_insets.top,
    );
    draw_inner_margin(onto, inner_border_insets, frame_of_inner_margin);

    let inner_content = frame_of_inner_margin.inset_by(
        inner_border_insets.bottom,
        inner_border_insets.left,
        inner_border_insets.right,
        inner_border_insets.top,
    );
    (frame_of_inner_margin, inner_content)
}

pub trait Bordered: Drawable + UIElement {
    fn outer_border_insets(&self) -> RectInsets;

    fn inner_border_insets(&self) -> RectInsets;

    fn border_insets(&self) -> RectInsets {
        self.outer_border_insets() + self.inner_border_insets()
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>);

    fn draw(&self) -> Vec<Rect> {
        let slice = self.get_slice_for_render();
        //let slice = self.get_slice();

        if !self.border_enabled() {
            // Invoke get_slice even though it'll have the same time as the parent to allow any layer
            // swapping to take place
            // TODO(PT): Not needed?
            let mut content_slice = slice.get_slice(Rect::with_size(slice.frame().size));
            self.draw_inner_content(slice.frame(), &mut content_slice);
        } else {
            let mut content_slice = slice.get_slice(self.draw_border());
            //println!("Bordered.draw() ContentSlice {content_slice}, slice {}", slice.frame());
            self.draw_inner_content(slice.frame(), &mut content_slice);
        }

        slice.drain_damages()
    }

    fn draw_rc(self: Rc<Self>) -> Vec<Rect> {
        let mut slice = self.get_slice_for_render();

        if !self.border_enabled() {
            self.draw_inner_content(slice.frame(), &mut slice);
        } else {
            let mut content_slice = slice.get_slice(self.draw_border());
            self.draw_inner_content(slice.frame(), &mut content_slice);
        }

        slice.drain_damages()
    }

    fn border_enabled(&self) -> bool {
        true
    }

    fn content_frame(&self) -> Rect {
        let f = Rect::with_size(self.frame().size);
        f.inset_by_insets(self.border_insets())
    }

    fn draw_border(&self) -> Rect {
        if !self.border_enabled() {
            return Rect::from_parts(Point::zero(), self.frame().size);
        }
        let onto = &mut self.get_slice_for_render();
        self.draw_border_with_insets(onto)
    }

    fn draw_border_with_insets(&self, onto: &mut Box<dyn LikeLayerSlice>) -> Rect {
        draw_border_with_insets(
            onto,
            self.outer_border_insets(),
            self.inner_border_insets(),
            self.frame().size,
            true,
            self.currently_contains_mouse(),
        )
        .1
    }
}
