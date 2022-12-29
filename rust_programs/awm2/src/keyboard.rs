use alloc::collections::BTreeSet;

#[derive(Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Debug)]
pub enum KeyboardModifier {
    Shift,
    Control,
    Command,
    Option,
}

pub struct KeyboardState {
    pub pressed_modifiers: BTreeSet<KeyboardModifier>,
}

impl KeyboardState {
    pub fn new() -> Self {
        Self {
            pressed_modifiers: BTreeSet::new(),
        }
    }

    pub fn is_control_held(&self) -> bool {
        self.pressed_modifiers.contains(&KeyboardModifier::Control)
    }
}
