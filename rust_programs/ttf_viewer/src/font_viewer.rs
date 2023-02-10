extern crate alloc;

use crate::utils::render_all_glyphs_in_font;
use agx_definitions::{
    Color, Drawable, LikeLayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
};
use alloc::boxed::Box;
use alloc::rc::Weak;
use alloc::vec::Vec;
use alloc::{rc::Rc, string::String};
use core::cell::RefCell;
use libgui::bordered::Bordered;
use libgui::text_input_view::TextInputView;
use libgui::text_view::TextView;
use libgui::ui_elements::UIElement;
use libgui::view::View;
use libgui::{AwmWindow, KeyCode};
use libgui_derive::{Bordered, Drawable, NestedLayerSlice, UIElement};

#[derive(Drawable, NestedLayerSlice, UIElement, Bordered)]
struct AllGlyphsView {
    view: Rc<TextView>,
}

impl AllGlyphsView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(font_path: Option<&str>, sizer: F) -> Self {
        let view = TextView::new(
            Color::green(),
            font_path,
            Size::new(32, 32),
            RectInsets::new(2, 2, 2, 2),
            sizer,
        );
        //let mut slice = view.get_slice();
        //render_all_glyphs_in_font(&mut slice, &view.font, &Size::new(32, 32), Some(64));
        Self { view }
    }

    pub fn handle_resize(&self, new_frame: &Rect) {
        let mut slice = self
            .view
            .get_slice()
            .get_slice(Rect::with_size(new_frame.size));
        render_all_glyphs_in_font(&mut slice, &self.view.font, &Size::new(32, 32), Some(256));
    }
}

#[derive(Drawable, NestedLayerSlice, UIElement, Bordered)]
struct ScratchpadView {
    view: Rc<TextInputView>,
}

impl ScratchpadView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(font_path: Option<&str>, sizer: F) -> Self {
        let view = TextInputView::new(font_path, Size::new(32, 32), sizer);
        Self { view }
    }
}

pub struct FontViewer {
    pub window: Rc<AwmWindow>,
    all_glyphs_view: Rc<AllGlyphsView>,
    scratchpad_view: Rc<ScratchpadView>,
}

impl FontViewer {
    pub fn new(window: Rc<AwmWindow>, font_path: Option<&str>) -> Rc<Self> {
        let all_glyphs_view_sizer = move |superview_size: Size| {
            Rect::with_size(Size::new(superview_size.width, superview_size.height / 2))
        };
        let all_glyphs_view = Rc::new(AllGlyphsView::new(
            font_path,
            move |view, superview_size| {
                let new_frame = all_glyphs_view_sizer(superview_size);
                //view.handle_resize(&new_frame);
                new_frame
            },
        ));
        let all_glyphs_view_clone = Rc::clone(&all_glyphs_view);
        all_glyphs_view
            .view
            .view
            .view
            .set_sizer(move |view, superview_size| {
                let new_frame = all_glyphs_view_sizer(superview_size);
                all_glyphs_view_clone.handle_resize(&new_frame);
                new_frame
            });
        Rc::clone(&window).add_component(Rc::clone(&all_glyphs_view) as Rc<dyn UIElement>);

        let scratchpad_view_sizer = move |superview_size: Size| {
            let all_glyphs_view_frame = all_glyphs_view_sizer(superview_size);
            let usable_height = superview_size.height - all_glyphs_view_frame.height();
            Rect::new(
                0,
                all_glyphs_view_frame.max_y(),
                superview_size.width,
                usable_height,
            )
        };
        let scratchpad_view = Rc::new(ScratchpadView::new(font_path, move |_, superview_size| {
            scratchpad_view_sizer(superview_size)
        }));
        Rc::clone(&window).add_component(Rc::clone(&scratchpad_view) as Rc<dyn UIElement>);

        Rc::new(Self {
            window,
            all_glyphs_view,
            scratchpad_view,
        })
    }
}
