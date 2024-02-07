use core::cell::RefCell;

use agx_definitions::{Color, Drawable, NestedLayerSlice, Point, Rect, Size};
use alloc::boxed::Box;
use alloc::{
    rc::{Rc, Weak},
    string::{String, ToString},
    vec,
    vec::Vec,
};
use ttf_renderer::Font;

use crate::font::{draw_char_with_font_onto, load_font, scaled_metrics_for_codepoint};
use crate::text_view::DrawnCharacter;
use crate::view::View;
use crate::{font::draw_char, ui_elements::UIElement};

pub struct Label {
    sizer: RefCell<Box<dyn Fn(&Self, Size) -> Rect>>,
    // TODO(PT): Remove the nested RefCell?
    container: RefCell<Option<RefCell<Weak<dyn NestedLayerSlice>>>>,
    frame: RefCell<Rect>,
    pub text: RefCell<String>,
    font: Font,
    font_size: Size,
    color: Color,
}

impl Label {
    pub fn new<F: 'static + Fn(&Label, Size) -> Rect>(text: &str, color: Color, sizer: F) -> Self {
        let font = load_font("/fonts/sf_pro.ttf");
        Self {
            sizer: RefCell::new(Box::new(sizer)),
            container: RefCell::new(None),
            frame: RefCell::new(Rect::zero()),
            text: RefCell::new(text.to_string()),
            font,
            font_size: Size::new(18, 18),
            color,
        }
    }

    pub fn new_with_font<F: 'static + Fn(&Label, Size) -> Rect>(
        text: &str,
        color: Color,
        font: Font,
        font_size: Size,
        sizer: F,
    ) -> Rc<Self> {
        Rc::new(Self {
            sizer: RefCell::new(Box::new(sizer)),
            container: RefCell::new(None),
            frame: RefCell::new(Rect::zero()),
            text: RefCell::new(text.to_string()),
            font,
            font_size,
            color,
        })
    }

    pub fn set_text(&self, text: &str) {
        self.text.replace(text.to_string());
    }

    pub fn set_frame(self: &Rc<Self>, frame: Rect) {
        self.frame.replace(frame);
    }
}

impl UIElement for Label {
    fn handle_superview_resize(&self, superview_size: Size) {
        let sizer = &*self.sizer.borrow();
        let frame = sizer(self, superview_size);
        self.frame.replace(frame);
    }
}

impl NestedLayerSlice for Label {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        Some(Weak::clone(
            &self.container.borrow().as_ref().unwrap().borrow(),
        ))
    }

    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
        self.container.replace(Some(RefCell::new(parent)));
    }

    fn get_slice_for_render(&self) -> Box<dyn agx_definitions::LikeLayerSlice> {
        self.get_slice()
    }
}

impl Drawable for Label {
    fn frame(&self) -> Rect {
        *self.frame.borrow()
    }

    fn content_frame(&self) -> Rect {
        Rect::from_parts(Point::zero(), self.frame().size)
    }

    fn draw(&self) -> Vec<Rect> {
        /*
        println!("\nLabel::draw()");
        println!("\nLabel frame: {}", self.frame());
        */
        let onto = &mut self.get_slice();
        //println!("\nOnto frame: {}", onto.frame());
        let mut cursor = Point::zero();
        for ch in self.text.borrow().chars() {
            /*
            draw_char(onto, ch, &cursor, self.color, &self.font_size);
            cursor.x += self.font_size.width;
            */
            let mut drawn_ch = DrawnCharacter::new(cursor, self.color, ch, self.font_size);
            let (bounding_box, glyph_metrics) =
                draw_char_with_font_onto(&mut drawn_ch, &self.font, onto);
            //let scaled_glyph_metrics = scaled_metrics_for_codepoint(&self.font, self.font_size, ch);
            //let scaled_glyph_metrics = scaled_metrics_for_codepoint(&self.font, self.font_size, ch);
            //cursor.x += scaled_glyph_metrics.advance_width as isize;
            cursor.x += glyph_metrics.advance_width as isize;
            //cursor.x += bounding_box.width();
            //cursor.x += scaled_glyph_metrics.advance_width as isize;
        }
        // TODO: Damages
        vec![]
    }
}
