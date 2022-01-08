use core::cell::RefCell;

use agx_definitions::{Color, Layer, OwnedLayer, Point, Rect, Size};
use alloc::boxed::Box;
use alloc::string::{String, ToString};
use alloc::vec;

use crate::{font::draw_char, window::AwmWindow};

pub trait EventHandler {
    fn left_click_handler(&mut self, window: &mut AwmWindow, ctx: &String);
}

pub trait UIElement {
    fn frame(&self) -> Rect;
    fn layer(&mut self) -> &mut dyn Layer;
    fn draw(&self, onto: &mut dyn Layer);
    //fn handle_left_click(button: Rc<&Self>);
    fn handle_left_click(&self) {}
}

// Perhaps a ContainsDrawable trait and a Drawable trait
// A button containing a label has both, but a label only has Drawable

pub trait Data: 'static {}

pub struct Button {
    frame: Rect,
    layer: OwnedLayer,
    pub label: String,
    left_click_cb: RefCell<Option<Box<dyn Fn(&Self)>>>,
    background_color: Color,
}

impl Button {
    pub fn new(frame: Rect, label: &str, background_color: Color) -> Self {
        let bpp = 4;
        let max_size = Size::new(600, 480);
        let framebuf_size = max_size.width * max_size.height * bpp;
        let framebuf = vec![0; framebuf_size];
        let layer = OwnedLayer::new(framebuf, bpp, max_size);
        Button {
            frame,
            layer,
            label: label.to_string(),
            left_click_cb: RefCell::new(None),
            background_color,
        }
    }
    pub fn on_left_click<F: 'static + Fn(&Self)>(&self, f: F) {
        *self.left_click_cb.borrow_mut() = Some(Box::new(f));
    }
}

impl UIElement for Button {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn layer(&mut self) -> &mut dyn Layer {
        &mut self.layer
    }

    fn draw(&self, onto: &mut dyn Layer) {
        onto.fill_rect(&self.frame, &Color::gray());
        onto.fill_rect(&self.frame.inset_by(2, 2, 2, 2), &self.background_color);

        let draw_color = Color::black();
        let font_size = Size::new(8, 8);
        let label_size = Size::new(self.label.len() * font_size.width, font_size.height);

        let midpoint = self.frame.center();
        let label_origin = midpoint
            - Point::new(
                (label_size.width as f64 / 2f64) as usize,
                (label_size.height as f64 / 2f64) as usize,
            );

        let mut cursor = label_origin;
        for ch in self.label.chars() {
            draw_char(onto, ch, &cursor, &draw_color, &font_size);
            cursor.x += font_size.width;
        }
    }

    fn handle_left_click(&self) {
        let maybe_cb = &*self.left_click_cb.borrow();
        if let Some(cb) = maybe_cb {
            (cb)(self);
        }
    }
}

pub struct Label {
    frame: Rect,
    layer: OwnedLayer,
    pub text: String,
    color: Color,
}

impl Label {
    pub fn new(frame: Rect, text: &str, color: Color) -> Self {
        let bpp = 4;
        let max_size = Size::new(600, 480);
        let framebuf_size = max_size.width * max_size.height * bpp;
        let framebuf = vec![0; framebuf_size];
        let layer = OwnedLayer::new(framebuf, bpp, max_size);
        Label {
            frame,
            layer,
            text: text.to_string(),
            color,
        }
    }
}

impl UIElement for Label {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn layer(&mut self) -> &mut dyn Layer {
        &mut self.layer
    }

    fn draw(&self, onto: &mut dyn Layer) {
        let font_size = Size::new(8, 8);
        let mut cursor = self.frame.origin;
        for ch in self.text.chars() {
            draw_char(onto, ch, &cursor, &self.color, &font_size);
            cursor.x += font_size.width;
        }
    }
}
