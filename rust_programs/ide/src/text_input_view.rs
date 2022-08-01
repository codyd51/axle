use core::cell::RefCell;

use agx_definitions::{
    Color, Drawable, LayerSlice, LikeLayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
    StrokeThickness,
};
use alloc::boxed::Box;
use alloc::rc::{Rc, Weak};
use alloc::{collections::BTreeMap, string::String};
use axle_rt::println;
use libgui::{bordered::Bordered, ui_elements::UIElement, view::View, window::KeyCode};
use libgui_derive::{Bordered, Drawable, NestedLayerSlice};

use crate::text_view::{CursorPos, TextView};

#[derive(Drawable, NestedLayerSlice)]
pub struct TextInputView {
    pub view: Rc<TextView>,
    is_shift_held: RefCell<bool>,
}

impl TextInputView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(sizer: F) -> Rc<Self> {
        let view = TextView::new(
            Color::white(),
            Size::new(16, 16),
            //RectInsets::new(8, 8, 8, 8),
            RectInsets::new(8, 8, 8, 8),
            sizer,
        );

        Rc::new(Self {
            view,
            is_shift_held: RefCell::new(false),
        })
    }

    pub fn get_text(&self) -> String {
        self.view.get_text()
    }

    pub fn get_cursor_pos(&self) -> CursorPos {
        *self.view.cursor_pos.borrow()
    }

    fn cursor_frame(&self) -> Rect {
        let cursor = (*self.view.cursor_pos.borrow()).1;
        //Rect::from_parts(Point::new(cursor.x + 1, cursor.y - 1), Size::new(1, 14))
        Rect::from_parts(Point::new(cursor.x + 1, cursor.y), Size::new(1, 14))
    }

    fn erase_cursor(&self) {
        let onto = &mut self.get_slice().get_slice(self.view.text_entry_frame());
        let cursor_frame = self.cursor_frame();
        onto.fill_rect(cursor_frame, Color::white(), StrokeThickness::Filled);
    }

    pub fn erase_char_and_update_cursor(&self) {
        self.view.erase_char_and_update_cursor()
    }

    pub fn draw_char_and_update_cursor(&self, ch: char, color: Color) {
        self.view.draw_char_and_update_cursor(ch, color)
    }

    pub fn draw_cursor(&self) {
        let onto = &mut self.get_slice().get_slice(self.view.text_entry_frame());
        let cursor_frame = self.cursor_frame();
        //println!("Cursor frame {cursor_frame} onto {}", onto.frame());
        onto.fill_rect(cursor_frame, Color::dark_gray(), StrokeThickness::Filled);
    }

    fn _set_cursor(&self, cursor_pos: CursorPos) {
        //println!("Setting cursor to {cursor_pos:?}");
        self.erase_cursor();
        *self.view.cursor_pos.borrow_mut() = cursor_pos;
        self.draw_cursor();
    }

    fn put_char(&self, ch: char) {
        self.erase_cursor();
        self.view.draw_char_and_update_cursor(ch, Color::black());
        self.draw_cursor();
    }

    fn delete_char(&self) {
        self.erase_cursor();
        self.view.erase_char_and_update_cursor();
        self.draw_cursor();
    }
}

// TODO(PT): Model keycodes in Rust
const KEY_IDENT_LEFT_SHIFT: u32 = 0x995;
const KEY_IDENT_RIGHT_SHIFT: u32 = 0x994;

fn is_key_shift(key: KeyCode) -> bool {
    [KEY_IDENT_LEFT_SHIFT, KEY_IDENT_RIGHT_SHIFT].contains(&key.0)
}

impl UIElement for TextInputView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered();
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited();
    }

    fn handle_mouse_moved(&self, mouse_point: Point) {
        self.view.handle_mouse_moved(mouse_point)
    }

    fn handle_mouse_scrolled(&self, mouse_point: Point, delta_z: isize) {
        self.view.handle_mouse_scrolled(mouse_point, delta_z)
    }

    fn handle_left_click(&self, mouse_point: Point) {
        self.view.handle_left_click(mouse_point)
    }

    fn handle_key_pressed(&self, key: KeyCode) {
        if is_key_shift(key) {
            *self.is_shift_held.borrow_mut() = true;
        }

        if let Some(mut ch) = char::from_u32(key.0) {
            if ch < (128 as char) {
                if *self.is_shift_held.borrow() {
                    if ch.is_alphabetic() {
                        ch = ch.to_uppercase().next().unwrap();
                    }
                    let shifted_symbols = BTreeMap::from([
                        ('1', '!'),
                        ('2', '@'),
                        ('3', '#'),
                        ('4', '$'),
                        ('5', '%'),
                        ('6', '^'),
                        ('7', '&'),
                        ('8', '*'),
                        ('9', '('),
                        ('0', ')'),
                        ('-', '_'),
                        (';', ':'),
                        ('\'', '"'),
                        ('/', '?'),
                    ]);
                    ch = *shifted_symbols.get(&ch).unwrap_or(&ch);
                }

                if ch == '\t' {
                    for _ in 0..4 {
                        self.put_char(' ');
                    }
                // TODO(PT): Extract this constant (backspace)
                } else if ch == 0x08 as char {
                    //println!("Caught backspace!");
                    self.delete_char();
                } else {
                    self.put_char(ch);
                }
            }
        } else {
            println!("Ignoring non-renderable character {key:?}");
        }
    }

    fn handle_key_released(&self, key: KeyCode) {
        if is_key_shift(key) {
            *self.is_shift_held.borrow_mut() = false;
        }
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size)
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}

impl Bordered for TextInputView {
    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        self.view.draw_inner_content(outer_frame, onto);
        self.draw_cursor()
    }
}
