use core::cell::RefCell;

use agx_definitions::{
    Color, Drawable, LikeLayerSlice, Line, NestedLayerSlice, Point, Rect, RectInsets, Size,
    StrokeThickness,
};
use alloc::boxed::Box;
use alloc::rc::{Rc, Weak};
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use ttf_renderer::{rendered_string_size, Font};

use crate::font::{draw_char_with_font_onto, scaled_metrics_for_codepoint};
use crate::text_view::DrawnCharacter;
use crate::{bordered::Bordered, font::draw_char, ui_elements::UIElement};

pub struct Button {
    pub label: String,
    font: Font,
    frame: RefCell<Rect>,
    sizer: RefCell<Box<dyn Fn(&Self, Size) -> Rect>>,
    container: RefCell<Option<RefCell<Weak<dyn NestedLayerSlice>>>>,
    left_click_cb: RefCell<Option<Box<dyn Fn(&Self)>>>,
    currently_contains_mouse_int: RefCell<bool>,
}

impl Button {
    pub fn new<F: Fn(&Self, Size) -> Rect + 'static>(
        label: &str,
        font: Option<Font>,
        sizer: F,
    ) -> Rc<Self> {
        if font.is_none() {
            // TODO(PT): It'd be nice to have some kind of font API that allowed anyone to retrieve a reference to a
            // font from any point, instead of needing to pass references all the way through the control flow.
            // Maybe there's an in-process font store that caches scanlines, etc, and fetches fonts from the FS.
            // The 'fetch from FS' has a platform-specific implementation. To facilitate this (as the paths will be
            // different on each OS), we could have an enum to model the possible font options, with an escape hatch
            // 'get from this path' variant, which could perhaps hold different values depending on the OS.
            todo!("Must handle when no font is passed to a Button?")
        }
        Rc::new(Self {
            label: label.to_string(),
            font: font.unwrap(),
            frame: RefCell::new(Rect::zero()),
            sizer: RefCell::new(Box::new(sizer)),
            container: RefCell::new(None),
            left_click_cb: RefCell::new(None),
            currently_contains_mouse_int: RefCell::new(false),
        })
    }
    pub fn on_left_click<F: Fn(&Self) + 'static>(&self, f: F) {
        *self.left_click_cb.borrow_mut() = Some(Box::new(f));
    }
}

impl Bordered for Button {
    fn outer_border_insets(&self) -> RectInsets {
        RectInsets::new(5, 5, 5, 5)
    }

    fn inner_border_insets(&self) -> RectInsets {
        RectInsets::new(5, 5, 5, 5)
    }

    fn draw_border(&self) -> Rect {
        let onto = &mut self.get_slice();
        // TODO(PT): Update me to be computed via border_insets()
        let outer_margin_size = 4;
        let outer_border = Rect::from_parts(Point::zero(), self.frame().size);

        let border_color = match self.currently_contains_mouse() {
            true => Color::new(160, 160, 160),
            false => Color::new(50, 50, 50),
        };
        onto.fill_rect(outer_border, border_color, StrokeThickness::Width(1));

        let outer_margin = outer_border.inset_by(1, 1, 1, 1);
        let outer_margin_color = Color::new(60, 60, 60);
        onto.fill_rect(
            outer_margin,
            outer_margin_color,
            StrokeThickness::Width(outer_margin_size),
        );

        let inner_border = outer_margin.inset_by(
            outer_margin_size,
            outer_margin_size,
            outer_margin_size,
            outer_margin_size,
        );

        let inner_border_color = match self.currently_contains_mouse() {
            true => Color::new(200, 200, 200),
            false => Color::new(140, 140, 140),
        };
        onto.fill_rect(inner_border, inner_border_color, StrokeThickness::Width(1));

        // Draw diagonal lines indicating an inset
        let inset_color = match self.currently_contains_mouse() {
            true => Color::new(180, 180, 180),
            false => Color::new(100, 100, 100),
        };
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

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        onto.fill(Color::light_gray());

        let draw_color = Color::black();
        let font_size = Size::new(8, 8);
        let label_size = Size::new(
            self.label.len() as isize * font_size.width,
            font_size.height,
        );

        let button_frame_midpoint = Point::new(
            self.frame().size.width / 2.0 as isize,
            self.frame().size.height / 2.0 as isize,
        );
        // Translate the midpoint to the slice's coordinate system
        let midpoint = button_frame_midpoint - (onto.frame().origin - outer_frame.origin);

        let label_origin = midpoint
            - Point::new(
                (label_size.width as f64 / 2f64) as isize,
                (label_size.height as f64 / 2f64) as isize,
            );

        //let mut cursor = label_origin;
        let mut cursor = Point::zero();
        /*
        for ch in self.label.chars() {
            draw_char(onto, ch, &cursor, draw_color, &font_size);
            cursor.x += font_size.width;
        }
        */
        // TODO(PT): A more reusable string renderer that can be shared between Button and Label?
        let font_size = Size::new(36, 36);
        let rendered_string_size = rendered_string_size(&self.label, &self.font, font_size);
        let mut cursor = Point::new(
            ((onto.frame().size.width as f64 / 2.0) - (rendered_string_size.width as f64 / 2.0))
                as isize,
            //0,
            //((onto.frame().size.height as f64 / 2.0) - (rendered_string_size.height as f64 / 2.0))
            ((onto.frame().size.height as f64 / 2.0) - (font_size.height as f64 / 2.0)) as isize, //as isize,
        );

        //pub fn rendered_string_size(s: &str, font: &Font, font_size: Size) -> Size {
        for ch in self.label.chars() {
            let mut drawn_ch = DrawnCharacter::new(cursor, Color::black(), ch, font_size);
            let (bounding_box, glyph_metrics) =
                draw_char_with_font_onto(&mut drawn_ch, &self.font, onto);
            let scaled_glyph_metrics = scaled_metrics_for_codepoint(&self.font, font_size, ch);
            cursor.x += scaled_glyph_metrics.advance_width as isize + 2;
        }
    }
}

impl Drawable for Button {
    fn frame(&self) -> Rect {
        *self.frame.borrow()
    }

    fn content_frame(&self) -> Rect {
        Bordered::content_frame(self)
    }

    fn draw(&self) -> Vec<Rect> {
        Bordered::draw(self)
    }
}

impl UIElement for Button {
    fn handle_left_click(&self, _mouse_point: Point) {
        let maybe_cb = &*self.left_click_cb.borrow();
        if let Some(cb) = maybe_cb {
            (cb)(self);
        }
    }

    fn handle_mouse_entered(&self) {
        *self.currently_contains_mouse_int.borrow_mut() = true;
        Bordered::draw_border(self);
    }

    fn handle_mouse_exited(&self) {
        *self.currently_contains_mouse_int.borrow_mut() = false;
        Bordered::draw_border(self);
    }

    fn handle_mouse_moved(&self, _mouse_point: Point) {}

    fn handle_superview_resize(&self, superview_size: Size) {
        let sizer = &*self.sizer.borrow();
        self.frame.replace(sizer(self, superview_size));
    }

    fn currently_contains_mouse(&self) -> bool {
        *self.currently_contains_mouse_int.borrow()
    }
}

impl NestedLayerSlice for Button {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        Some(Weak::clone(
            &self.container.borrow().as_ref().unwrap().borrow(),
        ))
    }

    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
        self.container.replace(Some(RefCell::new(parent)));
    }

    fn get_slice_for_render(&self) -> Box<dyn LikeLayerSlice> {
        self.get_slice()
    }
}
