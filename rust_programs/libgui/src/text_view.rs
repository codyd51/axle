use core::{
    cell::RefCell,
    cmp::{max, min},
    fmt::Display,
};

use crate::window_events::KeyCode;
use crate::{
    bordered::Bordered,
    font::{draw_char, CHAR_HEIGHT, CHAR_WIDTH, FONT8X8},
    println,
    ui_elements::UIElement,
    view::View,
};
use agx_definitions::{
    Color, Drawable, Layer, LikeLayerSlice, NestedLayerSlice, Point, Rect, RectInsets,
    SingleFramebufferLayer, Size, StrokeThickness,
};
use alloc::boxed::Box;
use alloc::collections::BTreeSet;
use alloc::fmt::Debug;
use alloc::vec;
use alloc::{
    rc::{Rc, Weak},
    string::String,
    vec::Vec,
};
use core::fmt::Formatter;
use libgui_derive::{Bordered, Drawable, NestedLayerSlice, UIElement};
use rand::Rng;
use crate::scroll_view::ScrollView;

#[derive(Debug, Copy, Clone)]
pub struct CursorPos(pub usize, pub Point);

#[derive(Debug, Copy, Clone)]
pub struct DrawnCharacter {
    pub value: char,
    pub pos: Point,
    pub color: Color,
    pub font_size: Size,
}

impl DrawnCharacter {
    fn new(pos: Point, color: Color, ch: char, font_size: Size) -> Self {
        Self {
            value: ch,
            pos,
            color,
            font_size,
        }
    }
}

impl Display for DrawnCharacter {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "<DrawnChar '{}' @ {}>",
            self.value,
            Rect::from_parts(self.pos, self.font_size)
        )
    }
}

#[derive(Drawable, NestedLayerSlice, UIElement)]
pub struct TextView {
    pub view: Rc<ScrollView>,
    font_size: Size,
    text_insets: RectInsets,
    pub text: RefCell<Vec<DrawnCharacter>>,
    pub cursor_pos: RefCell<CursorPos>,
}

impl TextView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(
        background_color: Color,
        font_size: Size,
        text_insets: RectInsets,
        sizer: F,
    ) -> Rc<Self> {
        let view = ScrollView::new(sizer);
        //let view = Rc::new(View::new(background_color, sizer));

        Rc::new(Self {
            view,
            font_size,
            text_insets,
            text: RefCell::new(vec![]),
            cursor_pos: RefCell::new(CursorPos(0, Point::zero())),
        })
    }

    pub fn font_size(&self) -> Size {
        self.font_size
    }

    pub fn get_text(&self) -> String {
        let mut out = vec![];
        let text = self.text.borrow();
        for ch in text.iter() {
            out.push(ch.value);
        }
        out.iter().collect()
    }

    pub fn text_entry_frame(&self) -> Rect {
        let content_frame = Rect::with_size(Bordered::content_frame(self).size);
        content_frame.apply_insets(self.text_insets)
    }

    pub fn draw_char_and_update_cursor3(&self, ch: char, color: Color) {
        let content_slice_frame = self.view.get_content_slice_frame();
        println!("Content slice frame {content_slice_frame}");
    }

    fn next_cursor_pos_for_char(
        cursor_pos: Point,
        ch: char,
        font_size: Size,
        onto: &Box<dyn LikeLayerSlice>,
    ) -> Point {
        let mut cursor_pos = cursor_pos;
        if ch == '\n' || cursor_pos.x + (font_size.width * 2) >= onto.frame().width() {
            cursor_pos.x = 0;
            cursor_pos.y += font_size.height + 2;
        } else {
            cursor_pos.x += font_size.width;
        }
        cursor_pos
    }

    pub fn draw_char_and_update_cursor(&self, ch: char, color: Color) {
        let mut cursor_pos = self.cursor_pos.borrow_mut();
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        let font_size = self.font_size();

        let mut text = self.text.borrow_mut();
        let is_inserting_at_end = cursor_pos.0 == text.len();
        //println!("draw_char_and_update({ch}) is_insert_at_end? {is_inserting_at_end}, {cursor_pos:?} {}", text.len());

        if !is_inserting_at_end {
            // If we just inserted a newline, we need to adjust our cursor position
            let mut cursor_point =
                Self::next_cursor_pos_for_char(cursor_pos.1, ch, font_size, onto);
            //println!("Base cursor for later chars: {cursor_point}");
            for drawn_ch in text[cursor_pos.0..].iter_mut() {
                //println!("\tFound later {}, originally placed at {}", drawn_ch.value, drawn_ch.pos);
                // Cover up this as we'll redraw it somewhere else
                onto.fill_rect(
                    Rect::from_parts(drawn_ch.pos, font_size),
                    Color::white(),
                    StrokeThickness::Filled,
                );
                drawn_ch.pos = cursor_point;
                //println!("\tShifted to {}", drawn_ch.pos);
                cursor_point =
                    Self::next_cursor_pos_for_char(cursor_point, drawn_ch.value, font_size, onto);
            }
        }

        onto.draw_char(ch, cursor_pos.1, color, font_size);

        // TODO(PT): This is not correct if we're not inserting at the end
        // We'll need to adjust the positions of every character that comes after this one
        let insertion_point = cursor_pos.0;
        cursor_pos.0 += 1;
        let mut cursor_point = &mut cursor_pos.1;

        // TODO(PT): If not inserting at the end, we need to move everything along and insert at an index
        let draw_desc = DrawnCharacter::new(*cursor_point, color, ch, font_size);
        if is_inserting_at_end {
            text.push(draw_desc);
        } else {
            println!("Inserting at {insertion_point}: {draw_desc:?}");
            text.insert(insertion_point, draw_desc);
        }
        *cursor_point = Self::next_cursor_pos_for_char(*cursor_point, ch, font_size, onto);
        //Bordered::draw(self)
    }

    pub fn erase_char_and_update_cursor(&self) {
        let mut cursor_pos = self.cursor_pos.borrow_mut();
        if cursor_pos.0 == 0 {
            //println!("Cannot delete with no text!");
            return;
        }

        let mut text = self.text.borrow_mut();

        let is_deleting_from_end = cursor_pos.0 == text.len();
        //println!("Erasing! From end? {is_deleting_from_end}");

        let mut chars_after_delete = vec![];
        for drawn_ch in text[cursor_pos.0 - 1..].iter() {
            chars_after_delete.push(*drawn_ch);
        }

        let removed_char = text.remove(cursor_pos.0 - 1);

        // Cover up the deleted character
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        let font_size = self.font_size;
        onto.fill_rect(
            Rect::from_parts(removed_char.pos, font_size),
            Color::white(),
            StrokeThickness::Filled,
        );

        cursor_pos.0 -= 1;

        if cursor_pos.0 == 0 {
            cursor_pos.1 = Point::zero();
        } else {
            let prev = text[cursor_pos.0 - 1];
            cursor_pos.1 =
                Self::next_cursor_pos_for_char(prev.pos, prev.value, prev.font_size, onto);
        }

        if !is_deleting_from_end {
            // Shift back the characters in front of this one
            //println!("{chars_after_delete:?}");
            for (i, drawn_ch) in text[cursor_pos.0..].iter_mut().enumerate() {
                let prev = chars_after_delete[i];
                // Cover up its previous position
                onto.fill_rect(
                    Rect::from_parts(drawn_ch.pos, drawn_ch.font_size),
                    Color::white(),
                    StrokeThickness::Filled,
                );
                //println!("Shifting back {} from {} to {} ({})", drawn_ch.value, drawn_ch.pos, prev.pos, prev);
                //cursor_pos.1 = Self::next_cursor_pos_for_char(prev.pos, prev.value, prev.font_size, onto);
                drawn_ch.pos = prev.pos;
                onto.draw_char(
                    drawn_ch.value,
                    drawn_ch.pos,
                    drawn_ch.color,
                    drawn_ch.font_size,
                );

                if prev.value == '\n' {
                    let mut next_cursor_pos = drawn_ch.pos;
                    for next_ch in chars_after_delete[i + 1..].iter_mut() {
                        //print!("Shifting forward {next_ch} to ");
                        next_ch.pos = Self::next_cursor_pos_for_char(
                            next_cursor_pos,
                            next_ch.value,
                            next_ch.font_size,
                            onto,
                        );
                        next_cursor_pos = next_ch.pos;
                        //println!("{}", next_ch.pos);
                    }
                }
            }
        }
        //println!("Removing char {removed_char:?}");
    }

    pub fn set_cursor_pos(&self, cursor: CursorPos) {
        let mut cursor_pos = self.cursor_pos.borrow_mut();
        *cursor_pos = cursor;
    }

    pub fn is_inserting_at_end(&self) -> bool {
        let mut cursor_pos = self.cursor_pos.borrow();
        let ret = cursor_pos.0 == self.text.borrow().len();
        //println!("{} {}", cursor_pos.0, self.text.borrow().len());
        ret
    }

    pub fn draw_char_with_description(&self, char_description: DrawnCharacter) {
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        onto.draw_char(
            char_description.value,
            char_description.pos,
            char_description.color,
            self.font_size,
        );
    }

    pub fn clear(&self) {
        *self.text.borrow_mut() = vec![];
        *self.cursor_pos.borrow_mut() = CursorPos(0, Point::zero());
        Bordered::draw(&*self.view);
    }
}

impl Bordered for TextView {
    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        //println!("draw_inner_content({outer_frame}, {onto})");
        self.view.draw_inner_content(outer_frame, onto);
        /*
        // PT: No need to use text_entry_frame() as `onto` is already in the content frame coordinate space
        let text_entry_slice = onto.get_slice(
            Rect::from_parts(Point::zero(), onto.frame().size).apply_insets(self.text_insets),
        );
        // TODO(PT): Only draw what's visible?
        let rendered_text = self.text.borrow();
        for drawn_char in rendered_text.iter() {
            text_entry_slice.draw_char(
                drawn_char.value,
                drawn_char.pos,
                drawn_char.color,
                drawn_char.font_size,
            );
        }
        */
        //println!("Finished call to draw_inner_content()");
    }
}