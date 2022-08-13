use core::cell::RefCell;

use crate::{bordered::Bordered, ui_elements::UIElement, view::View};
use agx_definitions::{
    Color, Drawable, LayerSlice, LikeLayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
    StrokeThickness,
};
use alloc::boxed::Box;
use alloc::rc::{Rc, Weak};
use alloc::{collections::BTreeMap, string::String};
use core::borrow::Borrow;
use std::io::Write;
use libgui_derive::{Bordered, Drawable, NestedLayerSlice};

use crate::println;
use crate::text_view::{CursorPos, DrawnCharacter, TextView};
use crate::window_events::KeyCode;

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
        Rect::from_parts(Point::new(cursor.x - 1, cursor.y - 2), Size::new(2, 18))
        //Rect::from_parts(Point::new(cursor.x + 1, cursor.y), Size::new(1, 14))
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
        //println!("Cursor frame {cursor_frame} onto {}, {}", onto.frame(), onto);
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

        if !self.view.is_inserting_at_end() {
            let onto = &mut self.view.get_slice().get_slice(self.view.text_entry_frame());
            //println!("Drawing box {}", self.view.cursor_pos.borrow().1);
            //onto.fill_rect(Rect::from_parts(self.view.cursor_pos.borrow().1, self.view.font_size()), Color::white(), StrokeThickness::Filled);
        }

        self.view.draw_char_and_update_cursor(ch, Color::black());
        self.draw_cursor();

        //println!("Inserting at end? {}", self.view.is_inserting_at_end());
        // PT: Bug: When a later line has overflowed onto a new line, and a previous line overflows, we don't
        // cover up the old line
        /*
        Example:
        a

        bbbbbbbbbbbbbb
        bbbb

        With input:

        accccccccccccc
        cccc

        Results in rendering:
        accccccccccccc
        ccccbbbbbbbbbb
        bbbbbbbbbbbbbb
        bbbb

        The old characters need to be erased
         */
        if !self.view.is_inserting_at_end() {
            let text = self.view.text.borrow();
            let cursor_pos = self.view.cursor_pos.borrow();

            for drawn_ch in text[cursor_pos.0..].iter() {
                //println!("Redrawing later char {drawn_ch:?}");
                //drawn_ch.pos = Self::next_cursor_pos_for_char(drawn_ch.pos, drawn_ch.value, font_size, onto);
                {
                    let onto = &mut self.view.get_slice().get_slice(self.view.text_entry_frame());
                    //onto.fill_rect(Rect::from_parts(drawn_ch.pos, self.view.font_size()), Color::white(), StrokeThickness::Filled);
                }
                self.view.draw_char_with_description(*drawn_ch);
            }
        }
    }

    fn delete_char(&self) {
        self.erase_cursor();
        self.view.erase_char_and_update_cursor();
        self.draw_cursor();
        self.print_text();
    }

    fn print_text(&self) {
        let text = self.view.text.borrow();
        println!("Text: (len {})", text.len());
        for drawn_ch in text.iter() {
            print!("{}", drawn_ch.value);
            std::io::stdout().flush();
        }
        println!();
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
        //println!("TextInputView.handle_mouse_scrolled({delta_z})");
        self.view.handle_mouse_scrolled(mouse_point, delta_z)
    }

    fn handle_left_click(&self, mouse_point: Point) {
        let content_frame = Bordered::content_frame(self);
        if content_frame.contains(mouse_point) {
            let mouse_point = mouse_point - content_frame.origin;
            // Adjust based on our scroll position
            let mouse_point = mouse_point + *self.view.view.layer.scroll_offset.borrow();
            // Find the cursor position corresponding to this point
            //println!("Got mouse point {mouse_point}");
            let mut closest_char: Option<(f64, usize, DrawnCharacter)> = None;
            let text = self.view.text.borrow();
            for (i, drawn_ch) in text.iter().enumerate() {
                // Newlines are not selectable
                if drawn_ch.value == '\n' {
                    continue;
                }
                // Adjust the position of characters to make them easier to select
                let char_click_pos = drawn_ch.pos + Point::new(self.view.font_size().width, self.view.font_size().height);
                let distance = mouse_point.distance(char_click_pos).abs();
                if closest_char.is_none() || distance < closest_char.unwrap().0 {
                    closest_char = Some((distance, i, *drawn_ch));
                }
            }
            if let Some(closest_char) = closest_char {
                /*
                println!("\tFound closest char:");
                println!("\t\tChar    : {} ({})", closest_char.2.value, closest_char.2.value as u8);
                println!("\t\tCharIdx : {}", closest_char.1);
                println!("\t\tDistance: {}", closest_char.0);
                */
                self.erase_cursor();
                // Redraw the character that just had the cursor above it
                // Will be one past the cursor position
                let cursor_pos = *self.view.cursor_pos.borrow();
                let text = self.view.text.borrow();
                if cursor_pos.0 + 1 < text.len() {
                    let char_under_prev_cursor = text[cursor_pos.0];
                    //println!("Redrawing {}", char_under_prev_cursor.value);
                    self.view.draw_char_with_description(char_under_prev_cursor);
                }

                // If click point is past the halfway point on the character,
                // set the cursor just after the character
                if mouse_point.x > (closest_char.2.pos.x + ((closest_char.2.font_size.width as f64) * 0.7) as isize) {
                    self.view.set_cursor_pos(CursorPos(closest_char.1 + 1, closest_char.2.pos + Point::new(closest_char.2.font_size.width, 0)));
                }
                else {
                    self.view.set_cursor_pos(CursorPos(closest_char.1, closest_char.2.pos));
                }
                self.draw_cursor();
            }
        }

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
                    println!("Caught backspace!");
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
