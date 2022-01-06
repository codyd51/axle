use core::any::Any;

use agx_definitions::{Color, Layer, OwnedLayer, Point, Rect, Size};
use alloc::boxed::Box;
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use alloc::{fmt::Debug, vec};

use axle_rt::printf;

use crate::{font::draw_char, window::AwmWindow};

pub trait EventHandler {
    fn left_click_handler(&mut self, window: &mut AwmWindow, ctx: &String);
}

pub trait UIElement {
    fn frame(&self) -> Rect;
    fn layer(&mut self) -> &mut dyn Layer;
    fn draw(&self, onto: &mut dyn Layer);
    fn handle_left_click(&mut self, window: &mut dyn EventHandler);
}

pub trait ClickableElement {
    fn on_left_click<F: 'static + Fn(&Self, &mut dyn EventHandler)>(&mut self, f: F);
}

// Perhaps a ContainsDrawable trait and a Drawable trait
// A button containing a label has both, but a label only has Drawable

pub struct Button {
    frame: Rect,
    layer: OwnedLayer,
    pub label: String,
    left_click_cb: Option<Box<dyn Fn(&Self, &mut dyn EventHandler)>>,
}

impl Button {
    pub fn new(frame: Rect, label: &str) -> Self {
        let bpp = 4;
        let max_size = Size::new(600, 480);
        let framebuf_size = max_size.width * max_size.height * bpp;
        let framebuf = vec![0; framebuf_size];
        let layer = OwnedLayer::new(framebuf, bpp, max_size);
        Button {
            frame,
            layer,
            label: label.to_string(),
            left_click_cb: None,
        }
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
        onto.fill_rect(&self.frame.inset_by(2, 2, 2, 2), &Color::white());

        let draw_color = Color::black();
        let font_size = Size::new(8, 8);
        let label_size = Size::new(self.label.len() * font_size.width, font_size.height);

        let midpoint = self.frame.center();
        let label_origin = midpoint
            - Point::new(
                (label_size.width as f64 / 2f64) as usize,
                (label_size.height as f64 / 2f64) as usize,
            );

        printf!(
            "Label size {:?}, midpoint {:?}, label origin {:?}\n",
            label_size,
            midpoint,
            label_origin
        );

        let mut cursor = label_origin;
        for ch in self.label.chars() {
            draw_char(onto, ch, &cursor, &draw_color, &font_size);
            cursor.x += font_size.width;
        }
    }

    fn handle_left_click(&mut self, handler: &mut dyn EventHandler) {
        printf!("Handling left click...\n");
        if let Some(cb) = &self.left_click_cb {
            cb(self, handler);
        }
    }
}

impl ClickableElement for Button {
    fn on_left_click<F: 'static + Fn(&Self, &mut dyn EventHandler)>(&mut self, f: F) {
        self.left_click_cb = Some(Box::new(f));
    }
}
