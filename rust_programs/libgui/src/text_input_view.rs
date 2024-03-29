use core::cell::RefCell;

use crate::{bordered::Bordered, ui_elements::UIElement, view::View};
use agx_definitions::{
    Color, Drawable, LikeLayerSlice, NestedLayerSlice, PixelByteLayout, Point, Rect, RectInsets,
    Size, StrokeThickness,
};
use alloc::boxed::Box;
use alloc::rc::{Rc, Weak};
use alloc::vec::Vec;
use alloc::{collections::BTreeMap, string::String};
use libgui_derive::{Bordered, Drawable, NestedLayerSlice};
use num_traits::Float;
use ttf_renderer::Font;

use crate::text_view::{CursorPos, DrawnCharacter, TextView};
use crate::window_events::KeyCode;

#[derive(Drawable, NestedLayerSlice)]
pub struct TextInputView {
    pub view: Rc<TextView>,
    is_shift_held: RefCell<bool>,
    key_pressed_cb: RefCell<Option<Box<dyn Fn(&Self, KeyCode)>>>,
}

impl TextInputView {
    pub fn new<F: Fn(&View, Size) -> Rect + 'static>(
        font_path: Option<&str>,
        font_size: Size,
        sizer: F,
    ) -> Rc<Self> {
        let view = TextView::new(
            Color::white(),
            font_path,
            font_size,
            RectInsets::new(8, 8, 8, 8),
            sizer,
        );

        Rc::new(Self {
            view,
            is_shift_held: RefCell::new(false),
            key_pressed_cb: RefCell::new(None),
        })
    }

    pub fn new_with_font<F: 'static + Fn(&View, Size) -> Rect>(
        font: Font,
        font_size: Size,
        text_insets: RectInsets,
        sizer: F,
        pixel_byte_layout: PixelByteLayout,
    ) -> Rc<Self> {
        let view = TextView::new_with_font(
            Color::white(),
            font,
            font_size,
            text_insets,
            sizer,
            pixel_byte_layout,
        );

        Rc::new(Self {
            view,
            is_shift_held: RefCell::new(false),
            key_pressed_cb: RefCell::new(None),
        })
    }

    pub fn clear(&self) {
        self.view.clear()
    }

    pub fn add_component(self: Rc<Self>, elem: Rc<dyn UIElement>) {
        Rc::clone(&self.view).add_component(elem)
    }

    pub fn get_text(&self) -> String {
        self.view.get_text()
    }

    pub fn get_cursor_pos(&self) -> CursorPos {
        *self.view.cursor_pos.borrow()
    }

    fn cursor_frame(&self) -> Rect {
        let cursor = (*self.view.cursor_pos.borrow()).1;
        let font_size = self.view.font_size();
        let font = &self.view.font;
        let scale_y = font_size.height as f64 / (font.units_per_em as f64);
        let scaled_bounding_box_height = (font.bounding_box.height() as f64 * scale_y) as isize;
        let cursor_height_frac = (scaled_bounding_box_height as f64 * 0.7) as isize;
        Rect::from_parts(
            //Point::new(cursor.x + 6, cursor.y + (cursor_height_frac / 2)),
            Point::new(
                cursor.x + 6,
                cursor.y + ((scaled_bounding_box_height - cursor_height_frac) / 2),
            ),
            Size::new(2, cursor_height_frac),
        )
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
            let _onto = &mut self
                .view
                .get_slice()
                .get_slice(self.view.text_entry_frame());
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
                    let _onto = &mut self
                        .view
                        .get_slice()
                        .get_slice(self.view.text_entry_frame());
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
        /*
        println!("Text: (len {})", text.len());
        for drawn_ch in text.iter() {
            print!("{}", drawn_ch.value);
        }
        println!();
        */
    }

    pub fn set_on_key_pressed<F: 'static + Fn(&Self, KeyCode)>(&self, f: F) {
        *self.key_pressed_cb.borrow_mut() = Some(Box::new(f));
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
                let char_click_pos = drawn_ch.pos
                    + Point::new(self.view.font_size().width, self.view.font_size().height);
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
                if mouse_point.x
                    > (closest_char.2.pos.x
                        + ((closest_char.2.font_size.width as f64) * 0.7) as isize)
                {
                    self.view.set_cursor_pos(CursorPos(
                        closest_char.1 + 1,
                        closest_char.2.pos + Point::new(closest_char.2.font_size.width, 0),
                    ));
                } else {
                    self.view
                        .set_cursor_pos(CursorPos(closest_char.1, closest_char.2.pos));
                }
                self.draw_cursor();
            }
        }

        self.view.handle_left_click(mouse_point)
    }

    fn handle_key_pressed(&self, key: KeyCode) {
        // First, dispatch the user-provided callback, if set
        let callback = self.key_pressed_cb.borrow();
        if let Some(callback) = callback.as_ref() {
            callback(self, key);
        }

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
            //println!("Ignoring non-renderable character {key:?}");
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
    fn outer_border_insets(&self) -> RectInsets {
        self.view.outer_border_insets()
    }

    fn inner_border_insets(&self) -> RectInsets {
        self.view.inner_border_insets()
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        self.view.draw_inner_content(outer_frame, onto);
        self.draw_cursor()
    }

    fn draw_border_with_insets(&self, onto: &mut Box<dyn LikeLayerSlice>) -> Rect {
        self.view.draw_border_with_insets(onto)
    }

    fn draw_border(&self) -> Rect {
        self.view.draw_border()
    }

    fn draw(&self) -> Vec<Rect> {
        Bordered::draw(&self.view as &TextView)
    }
}
