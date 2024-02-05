#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(format_args_nl)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate libc;

use alloc::boxed::Box;
use alloc::{collections::BTreeMap, format, rc::Weak, vec::Vec};
use alloc::{
    rc::Rc,
    string::{String, ToString},
};
use core::{cell::RefCell, cmp};

use libc::ms_since_boot;
use libgui::bordered::Bordered;
use libgui::button::Button;
use libgui::label::Label;
use libgui::ui_elements::UIElement;
use libgui::view::View;
use libgui::AwmWindow;
use libgui::{Timer, TimerMode};

use axle_rt::{
    amc_message_await, amc_message_send, amc_register_service, printf, println, AmcMessage,
};
use axle_rt::{ContainsEventField, ExpectsEventField};

use agx_definitions::{
    Color, Drawable, LayerSlice, LikeLayerSlice, Line, NestedLayerSlice, Point, Rect, RectInsets,
    Size, StrokeThickness,
};
use menu_bar_messages::{AWM_MENU_BAR_HEIGHT, AWM_MENU_BAR_SERVICE_NAME};

// TODO(PT): Share this implementation with the dock?
struct GradientView {
    view: Rc<View>,
}

impl GradientView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(sizer: F) -> Self {
        // Background color of the inner View is disregarded
        let view = Rc::new(View::new(Color::green(), sizer));
        Self { view }
    }

    pub fn set_border_enabled(&self, enabled: bool) {
        self.view.set_border_enabled(enabled);
    }

    pub fn add_component(self: Rc<Self>, elem: Rc<dyn UIElement>) {
        Rc::clone(&self.view).add_component(elem)
    }

    pub fn remove_component(self: Rc<Self>, elem: &Rc<dyn UIElement>) {
        Rc::clone(&self.view).remove_component(elem)
    }
}

fn lerp(a: f64, b: f64, factor: f64) -> f64 {
    a + (b - a) * factor
}

impl Bordered for GradientView {
    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        // TODO(PT): Time this

        let end = Color::new(200, 200, 200);
        let start = Color::new(255, 255, 255);
        for y in 0..onto.frame().size.height {
            let factor = (y as f64 / onto.frame().size.height as f64);
            let row_color = Color::new(
                lerp(start.r.into(), end.r.into(), factor) as u8,
                lerp(start.g.into(), end.g.into(), factor) as u8,
                lerp(start.b.into(), end.b.into(), factor) as u8,
            );

            // Horizontal gradient
            for x in 0..onto.frame().size.width {
                let mut end2 = Color::new(
                    (row_color.r as f64 / 3.0) as u8,
                    (row_color.g as f64 / 3.0) as u8,
                    (row_color.b as f64 / 3.0) as u8,
                );
                let mut start2 = Color::new(0, 0, 0);

                let mid_x = onto.frame().size.width / 2;
                let mut factor2 = (x as f64 / mid_x as f64);
                if x > mid_x {
                    factor2 = 1.0 - ((x - mid_x) as f64 / (mid_x) as f64);
                }

                let mut col_color = Color::new(
                    lerp(start2.r.into(), end2.r.into(), factor - factor2) as u8,
                    lerp(start2.g.into(), end2.g.into(), factor - factor2) as u8,
                    lerp(start2.b.into(), end2.b.into(), factor - factor2) as u8,
                );
                let color = Color::new(
                    row_color.r - col_color.r,
                    row_color.g - col_color.g,
                    row_color.b - col_color.b,
                );
                onto.putpixel(Point::new(x, y), color);
            }
        }

        self.view.draw_subviews();
    }

    fn border_insets(&self) -> RectInsets {
        RectInsets::new(0, 2, 0, 0)
    }
}

impl UIElement for GradientView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered();
        Bordered::draw(self);
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited();
        Bordered::draw(self);
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

impl NestedLayerSlice for GradientView {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        self.view.get_parent()
    }

    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
        self.view.set_parent(parent);
    }

    fn get_slice(&self) -> Box<dyn LikeLayerSlice> {
        self.view.get_slice()
    }

    fn get_slice_for_render(&self) -> Box<dyn LikeLayerSlice> {
        self.view.get_slice()
    }
}

impl Drawable for GradientView {
    fn frame(&self) -> Rect {
        self.view.frame()
    }

    fn content_frame(&self) -> Rect {
        Bordered::content_frame(self)
    }

    fn draw(&self) -> Vec<Rect> {
        Bordered::draw(self)
    }
}

struct MenuBar {
    pub window: Rc<AwmWindow>,
    container_view: Rc<GradientView>,
    vanity_label: Rc<Label>,
    uptime_label: RefCell<Rc<Label>>,
}

impl MenuBar {
    fn new(window: Rc<AwmWindow>, _menu_bar_size: Size) -> Rc<Self> {
        let container_view = Rc::new(GradientView::new(move |_view, superview_size| {
            Rect::from_parts(Point::zero(), superview_size)
        }));
        container_view.set_border_enabled(false);
        Rc::clone(&window).add_component(Rc::clone(&container_view) as Rc<dyn UIElement>);

        //let font_size = Size::new(20, 28);
        let font_size = Size::new(8, 10);
        let vanity_label = Rc::new(Label::new(
            "axle OS",
            Color::new(30, 30, 30),
            move |_, _| {
                Rect::from_parts(
                    //Point::new(8, (AWM_MENU_BAR_HEIGHT / 2) - font_size.height / 2),
                    Point::new(8, 0),
                    //Point::new(8, 0),
                    Size::new(300, AWM_MENU_BAR_HEIGHT),
                )
            },
        ));
        let vanity_label_clone = Rc::clone(&vanity_label);
        Rc::clone(&container_view).add_component(vanity_label_clone);

        let uptime_label = Rc::new(Label::new(
            "Uptime: 0s",
            Color::new(30, 30, 30),
            move |_, _| {
                Rect::from_parts(
                    Point::new(1920 - 110, (AWM_MENU_BAR_HEIGHT / 2) - font_size.height / 2),
                    //Point::new(1920 - 110, 0),
                    Size::new(110, AWM_MENU_BAR_HEIGHT),
                )
            },
        ));
        let uptime_label_clone = Rc::clone(&uptime_label);
        Rc::clone(&container_view).add_component(uptime_label_clone);

        Rc::new(Self {
            window,
            container_view,
            vanity_label,
            uptime_label: RefCell::new(uptime_label),
        })
    }

    pub fn redraw(self: Rc<Self>) {
        let window = Rc::clone(&self.window);
        window.resize_subviews();
    }

    pub fn update_clock(self: Rc<Self>) {
        {
            let uptime_label = self.uptime_label.borrow();
            uptime_label.set_text(&format!("Uptime: {}s", unsafe { ms_since_boot() } / 1024));
        }
        self.redraw();
    }
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service(AWM_MENU_BAR_SERVICE_NAME);

    let menu_bar_size = Size::new(1920, AWM_MENU_BAR_HEIGHT);
    let window = Rc::new(AwmWindow::new("Menu Bar", menu_bar_size));
    let menu_bar = Rc::new(RefCell::new(MenuBar::new(
        Rc::clone(&window),
        menu_bar_size,
    )));

    window.add_message_handler(move |_window, msg_unparsed: AmcMessage<[u8]>| {
        if msg_unparsed.source() == AwmWindow::AWM_SERVICE_NAME {
            let raw_body = msg_unparsed.body();
            let event = u32::from_ne_bytes(
                // We must slice the array to the exact size of a u32 for the conversion to succeed
                raw_body[..core::mem::size_of::<u32>()]
                    .try_into()
                    .expect("Failed to get 4-length array from message body"),
            );

            let consumed = unsafe {
                match event {
                    _ => false,
                }
            };
            if consumed {
                return;
            }
        }
    });

    window.add_timer(Timer::new(1000, TimerMode::Periodic, move |_window| {
        //println!("Timer fired!!!! {}", unsafe { ms_since_boot() });
        Rc::clone(&menu_bar.borrow()).update_clock();
    }));

    window.enter_event_loop();
    0
}
