use crate::window::Window;
use agx_definitions::{Point, Rect, Size};
use alloc::rc::Rc;
use alloc::vec;
use alloc::vec::Vec;
use core::cmp::{max, min};

#[derive(Debug)]
pub enum MouseStateChange {
    LeftClickBegan,
    LeftClickEnded,
    Moved(Point, Point),
    Scrolled(i8),
}

pub struct MouseState {
    pub pos: Point,
    desktop_size: Size,
    size: Size,
    pub left_click_down: bool,
}

impl MouseState {
    pub fn new(pos: Point, desktop_size: Size) -> Self {
        Self {
            pos,
            size: Size::new(14, 14),
            desktop_size,
            left_click_down: false,
        }
    }

    pub fn compute_state_changes(
        &mut self,
        new_pos: Option<Point>,
        delta_z: Option<i8>,
        status: i8,
    ) -> Vec<MouseStateChange> {
        let mut out = vec![];

        if let Some(delta_z) = delta_z {
            if delta_z != 0 {
                out.push(MouseStateChange::Scrolled(delta_z))
            }
        }

        if let Some(new_pos) = new_pos {
            let old_pos = self.pos;

            self.pos = new_pos;

            // Bind mouse to screen dimensions
            self.pos.x = max(self.pos.x, 0);
            self.pos.y = max(self.pos.y, 0);
            self.pos.x = min(self.pos.x, self.desktop_size.width - 4);
            self.pos.y = min(self.pos.y, self.desktop_size.height - 10);

            let delta = new_pos - old_pos;
            if delta.x != 0 || delta.y != 0 {
                out.push(MouseStateChange::Moved(self.pos, delta));
            }
        }

        // Is the left button clicked?
        if status & (1 << 0) != 0 {
            // Were we already tracking a left click?
            if !self.left_click_down {
                self.left_click_down = true;
                out.push(MouseStateChange::LeftClickBegan);
            }
        } else {
            // Did we just release a left click?
            if self.left_click_down {
                self.left_click_down = false;
                out.push(MouseStateChange::LeftClickEnded);
            }
        }

        out
    }

    pub fn frame(&self) -> Rect {
        Rect::from_parts(self.pos, self.size)
    }
}

pub enum MouseInteractionState {
    BackgroundHover,
    WindowHover(Rc<Window>),
    HintingWindowDrag(Rc<Window>),
    HintingWindowResize(Rc<Window>),
    PerformingWindowDrag(Rc<Window>),
    PerformingWindowResize(Rc<Window>),
}

impl PartialEq for MouseInteractionState {
    fn eq(&self, other: &Self) -> bool {
        match self {
            MouseInteractionState::BackgroundHover => match other {
                MouseInteractionState::BackgroundHover => true,
                _ => false,
            },
            MouseInteractionState::WindowHover(w1) => match other {
                MouseInteractionState::WindowHover(w2) => Rc::ptr_eq(w1, w2),
                _ => false,
            },
            MouseInteractionState::HintingWindowDrag(w1) => match other {
                MouseInteractionState::HintingWindowDrag(w2) => Rc::ptr_eq(w1, w2),
                _ => false,
            },
            MouseInteractionState::HintingWindowResize(w1) => match other {
                MouseInteractionState::HintingWindowResize(w2) => Rc::ptr_eq(w1, w2),
                _ => false,
            },
            MouseInteractionState::PerformingWindowDrag(w1) => match other {
                MouseInteractionState::PerformingWindowDrag(w2) => Rc::ptr_eq(w1, w2),
                _ => false,
            },
            MouseInteractionState::PerformingWindowResize(w1) => match other {
                MouseInteractionState::PerformingWindowResize(w2) => Rc::ptr_eq(w1, w2),
                _ => false,
            },
        }
    }
}
