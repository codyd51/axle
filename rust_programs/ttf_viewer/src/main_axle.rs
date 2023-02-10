use crate::font_viewer::FontViewer;
use agx_definitions::Size;
use alloc::rc::Rc;
use axle_rt::amc_register_service;
use core::cell::RefCell;
use libgui::AwmWindow;

pub fn main() {
    amc_register_service("com.axle.ttf_viewer");

    let window = Rc::new(AwmWindow::new("Font Viewer", Size::new(800, 600)));
    Rc::new(RefCell::new(FontViewer::new(Rc::clone(&window), None)));
    window.enter_event_loop()
}
