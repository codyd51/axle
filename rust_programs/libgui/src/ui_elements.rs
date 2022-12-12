use crate::window_events::KeyCode;
use agx_definitions::{Drawable, NestedLayerSlice, Point, Size};

pub trait UIElement: Drawable + NestedLayerSlice {
    fn handle_mouse_entered(&self) {}
    fn handle_mouse_exited(&self) {}
    fn handle_mouse_moved(&self, _mouse_point: Point) {}
    fn handle_mouse_scrolled(&self, _mouse_point: Point, _delta_z: isize) {}

    fn handle_left_click(&self, _mouse_point: Point) {}

    fn handle_key_pressed(&self, _key: KeyCode) {}
    fn handle_key_released(&self, _key: KeyCode) {}

    fn handle_superview_resize(&self, _superview_size: Size) {}

    fn currently_contains_mouse(&self) -> bool {
        false
    }
}

// Perhaps a ContainsDrawable trait and a Drawable trait
// A button containing a label has both, but a label only has Drawable
//
// Resizable trait?

/*
Goal: Scrollable list of buttons

Goal: Scrollable terminal with backlog
    Memory usage will grow unbounded if we keep stitching more layers together
    Instead, we need two representations:
        Growable list of the stored text
        Several

Differentiate between scrollable view and scrollable layer?
    Consider:
        Image.render_onto(Layer)
    If the layer is actually backed by several layers stitched together, how does this API work?

    Similar to libgui,
*/
