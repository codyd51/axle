use agx_definitions::{
    Color, Drawable, LayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
};
use alloc::rc::{Rc, Weak};
use libgui::{bordered::Bordered, ui_elements::UIElement, view::View};

pub struct OutputView {
    view: Rc<View>,
}

impl OutputView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(sizer: F) -> Rc<Self> {
        let view = Rc::new(View::new(Color::new(30, 30, 30), sizer));

        Rc::new(Self { view })
    }
}

impl UIElement for OutputView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered();
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited();
    }

    fn handle_mouse_moved(&self, mouse_point: Point) {
        self.view.handle_mouse_moved(mouse_point)
    }

    fn handle_left_click(&self, mouse_point: Point) {
        self.view.handle_left_click(mouse_point)
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size)
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}

impl NestedLayerSlice for OutputView {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        self.view.get_parent()
    }

    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
        self.view.set_parent(parent);
    }

    fn get_slice(&self) -> LayerSlice {
        self.view.get_slice()
    }
}

impl Drawable for OutputView {
    fn frame(&self) -> Rect {
        self.view.frame()
    }

    fn content_frame(&self) -> Rect {
        Bordered::content_frame(self)
    }

    fn draw(&self) {
        Bordered::draw(self)
    }
}

impl Bordered for OutputView {
    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut LayerSlice) {
        self.view.draw_inner_content(outer_frame, onto);
    }
}
