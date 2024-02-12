#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
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

use libgui::bordered::Bordered;
use libgui::button::Button;
use libgui::label::Label;
use libgui::ui_elements::UIElement;
use libgui::view::View;
use libgui::AwmWindow;

use axle_rt::{amc_message_await, amc_message_send, amc_register_service, printf, AmcMessage};
use axle_rt::{ContainsEventField, ExpectsEventField};

use agx_definitions::{
    Color, Drawable, LayerSlice, LikeLayerSlice, Line, NestedLayerSlice, Point, Rect, RectInsets,
    Size, StrokeThickness,
};

use dock_messages::{
    AwmDockEvent, AwmDockTaskViewClicked, AwmDockTaskViewHoverExited, AwmDockTaskViewHovered,
    AwmDockWindowClosed, AwmDockWindowCreatedEvent, AwmDockWindowMinimizeRequestedEvent,
    AwmDockWindowMinimizeWithInfo, AwmDockWindowTitleUpdatedEvent, AWM_DOCK_HEIGHT,
    AWM_DOCK_SERVICE_NAME,
};

struct TaskView {
    entry_index: RefCell<usize>,
    window_id: u32,
    title: RefCell<String>,
    title_label: Rc<Label>,
    view: Rc<View>,
    background_color: Color,
}

impl TaskView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(
        entry_index: usize,
        window_id: u32,
        title: &str,
        sizer: F,
    ) -> Self {
        let background_color = Color::new(200, 200, 200);
        let insets = Self::border_insets();
        let view = Rc::new(View::new(background_color, sizer));
        // We'll draw the border ourselves, so the inner View shouldn't
        view.set_border_enabled(false);

        // TODO(PT): Sizer
        let title_label = Rc::new(Label::new(
            &title,
            Color::new(30, 30, 30),
            move |label, superview_size| {
                Rect::from_parts(Point::new(insets.left + 4, insets.top), Size::new(300, 30))
            },
        ));
        let title_label_clone = Rc::clone(&title_label);
        // TODO(PT): Set font size as attribute?
        Rc::clone(&view).add_component(title_label_clone);

        TaskView {
            entry_index: RefCell::new(entry_index),
            window_id,
            title: RefCell::new(title.to_string()),
            title_label,
            view,
            background_color,
        }
    }

    fn text_width(self: &Rc<Self>) -> usize {
        20 * self.title.borrow().len()
    }

    fn resize_title_label(self: &Rc<Self>, superview_size: Size) {
        // TODO(PT): This is a hack until labels have sizers
        // TODO(PT): Expose font size of labels
        //let font_size = Size::new(20, 20);
        let font_size = Size::new(8, 10);
        let insets = Self::border_insets();
        let usable_frame = Rect::from_parts(Point::zero(), superview_size).inset_by_insets(insets);
        let text_width = self.text_width() as _;

        let label_frame = Rect::from_parts(
            Point::new(
                usable_frame.mid_x() - (text_width / 2),
                usable_frame.mid_y() - (font_size.height / 2),
                //usable_frame.min_y(),
            ),
            Size::new(text_width, font_size.height),
        );
        /*
        printf!(
            "Resized label {} to frame {:?}\n",
            *self.title_label.text.borrow(),
            label_frame,
        );
        */
        self.title_label.set_frame(label_frame);
    }

    fn border_insets() -> RectInsets {
        RectInsets::new(2, 2, 2, 2)
    }

    pub fn set_title(self: Rc<Self>, title: &str) {
        *self.title.borrow_mut() = title.to_string();
        self.title_label.set_text(title);
    }

    pub fn set_entry_index(&self, entry_index: usize) {
        *self.entry_index.borrow_mut() = entry_index
    }
}

impl NestedLayerSlice for TaskView {
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
        self.get_slice()
    }
}

impl Drawable for TaskView {
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

impl Bordered for TaskView {
    fn outer_border_insets(&self) -> RectInsets {
        RectInsets::new(1, 1, 1, 1)
    }

    fn inner_border_insets(&self) -> RectInsets {
        RectInsets::new(1, 1, 1, 1)
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        self.view.draw_inner_content(outer_frame, onto);
    }

    fn draw_border_with_insets(&self, onto: &mut Box<dyn LikeLayerSlice>) -> Rect {
        let mut frame = Rect::from_parts(Point::zero(), self.frame().size);
        let insets = Self::border_insets();

        // Bottom edge gets a dark line
        let bottom_edge = Line::new(
            Point::new(frame.min_x(), frame.max_y() - insets.bottom),
            Point::new(frame.max_x(), frame.max_y() - insets.bottom),
        );
        bottom_edge.draw(
            onto,
            Color::new(40, 40, 40),
            StrokeThickness::Width(insets.bottom),
        );

        // Left and right edges get medium-dark lines
        let left_edge = Line::new(
            Point::new(frame.min_x(), frame.min_y()),
            Point::new(frame.min_x(), frame.max_y() + 1),
        );
        left_edge.draw(
            onto,
            Color::new(80, 80, 80),
            StrokeThickness::Width(insets.left),
        );

        let right_edge = Line::new(
            Point::new(frame.max_x() - insets.right, frame.min_y()),
            Point::new(frame.max_x() - insets.right, frame.max_y() + 1),
        );
        right_edge.draw(
            onto,
            Color::new(80, 80, 80),
            StrokeThickness::Width(insets.right),
        );

        // Top edge gets a light line
        // TODO(PT): Fixme + 1
        let top_edge = Line::new(
            Point::new(frame.min_x(), frame.min_y()),
            Point::new(frame.max_x() + 1, frame.min_y()),
        );
        top_edge.draw(
            onto,
            Color::new(160, 160, 160),
            StrokeThickness::Width(insets.top),
        );
        // Draw a highlight if the mouse is currently inside the task view
        if self.currently_contains_mouse() {
            let highlight_thickness = 2;
            onto.fill_rect(
                frame,
                Color::white(),
                StrokeThickness::Width(highlight_thickness),
            );
            /*
            frame = frame.inset_by(
                highlight_thickness,
                highlight_thickness,
                highlight_thickness,
                highlight_thickness,
            );
            */
        }

        frame.inset_by_insets(insets)
    }
}

impl UIElement for TaskView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered();
        self.draw_border();
        amc_message_send(
            AwmWindow::AWM_SERVICE_NAME,
            AwmDockTaskViewHovered::new(self.window_id, self.frame()),
        );
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited();
        self.draw_border();
        printf!("Mouse exited task view!\n");
        amc_message_send(
            AwmWindow::AWM_SERVICE_NAME,
            AwmDockTaskViewHoverExited::new(self.window_id),
        );
    }

    fn handle_mouse_moved(&self, mouse_point: Point) {
        self.view.handle_mouse_moved(mouse_point)
    }

    fn handle_left_click(&self, mouse_point: Point) {
        self.view.handle_left_click(mouse_point);
        amc_message_send(
            AwmWindow::AWM_SERVICE_NAME,
            AwmDockTaskViewClicked::new(self.window_id),
        );
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size)
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}

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

        let end = Color::new(100, 100, 100);
        let start = Color::new(200, 200, 200);
        for y in 0..onto.frame().size.height {
            let factor = (y as f64 / onto.frame().size.height as f64);
            let row_color = Color::new(
                lerp(start.r.into(), end.r.into(), factor) as u8,
                lerp(start.g.into(), end.g.into(), factor) as u8,
                lerp(start.b.into(), end.b.into(), factor) as u8,
            );
            let row_line = Line::new(Point::new(0, y), Point::new(onto.frame().size.width, y));
            //row_line.draw(onto, row_color, StrokeThickness::Width(1));

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
        //self.draw_border();
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited();
        Bordered::draw(self);
        //self.draw_border();
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

struct Dock {
    pub window: Rc<AwmWindow>,
    container_view: Rc<GradientView>,
    task_views: RefCell<Vec<Rc<TaskView>>>,
    task_view_height: isize,
    task_views_origin: Point,
    task_views_spacing_x: isize,
}

impl Dock {
    fn new(window: Rc<AwmWindow>, dock_size: Size) -> Rc<Self> {
        let container_view = Rc::new(GradientView::new(move |_view, superview_size| {
            Rect::from_parts(Point::zero(), superview_size)
        }));
        container_view.set_border_enabled(false);
        Rc::clone(&window).add_component(Rc::clone(&container_view) as Rc<dyn UIElement>);

        let task_view_height = (dock_size.height as f64 * 0.75) as _;
        printf!(
            "Chose task view height {} for dock height {}\n",
            task_view_height,
            dock_size.height
        );
        let task_views_spacing_x = 10;
        Rc::new(Self {
            window,
            container_view,
            task_views: RefCell::new(Vec::new()),
            task_view_height,
            task_views_origin: Point::new(
                task_views_spacing_x * 2,
                (dock_size.height / 2) - (task_view_height / 2),
            ),
            task_views_spacing_x,
        })
    }

    pub unsafe fn body_as_type_unchecked<T: AwmDockEvent>(body: &[u8]) -> &T {
        &*(body.as_ptr() as *const T)
    }

    pub fn handle_window_created(self: Rc<Self>, event: &AwmDockWindowCreatedEvent) {
        let title_with_null_bytes = core::str::from_utf8(&(*event).title).unwrap();
        let title_without_null_bytes = title_with_null_bytes.trim_matches(char::from(0));
        printf!(
            "Window created! ID {}, title {}\n",
            event.window_id,
            title_without_null_bytes
        );

        let task_view = Rc::new(TaskView::new(
            self.task_views.borrow().len(),
            event.window_id,
            &title_without_null_bytes,
            move |_view, superview_size| {
                // The real sizer is set up down below
                Rect::zero()
            },
        ));

        let self_clone = Rc::clone(&self);
        // let self_clone = Rc::clone(&self);
        let task_view_clone = Rc::clone(&task_view);

        task_view.view.set_sizer(move |_view, superview_size| {
            let entry_index = *task_view_clone.entry_index.borrow();
            let origin_x = match entry_index {
                0 => self_clone.task_views_origin.x,
                _ => {
                    let self_clone = Rc::clone(&self_clone);
                    let task_views_clone = &self_clone.task_views;
                    let previous_task_view = &task_views_clone.borrow()[entry_index - 1];
                    previous_task_view.frame().max_x() + self_clone.task_views_spacing_x
                }
            };
            let frame = Rect::from_parts(
                Point::new(origin_x, self_clone.task_views_origin.y),
                Size::new(
                    (18 + task_view_clone.text_width()).try_into().unwrap(),
                    self_clone.task_view_height,
                ),
            );
            //printf!("Task view sizer returned {:?}\n", frame);

            // Center the title label to be centered
            task_view_clone.resize_title_label(frame.size);

            frame
        });

        self.task_views.borrow_mut().push(Rc::clone(&task_view));
        //Rc::clone(&self.window).add_component(task_view);
        Rc::clone(&self.container_view).add_component(task_view);

        self.redraw();
    }

    pub fn handle_window_title_updated(self: Rc<Self>, event: &AwmDockWindowTitleUpdatedEvent) {
        let title_with_null_bytes = core::str::from_utf8(&(*event).title).unwrap();
        let title_without_null_bytes = title_with_null_bytes.trim_matches(char::from(0));
        /*
        printf!(
            "Window title updated! ID {}, title {}\n",
            event.window_id,
            title_without_null_bytes
        );
        */

        // Update the title of the affected task view
        let self_clone = Rc::clone(&self);
        let task_views = self.task_views.borrow();
        let task_view = task_views
            .iter()
            .filter(|tv| tv.window_id == event.window_id)
            .next();

        if let None = task_view {
            printf!("No task view for the provided window ID, skipping it...\n");
            return;
        }
        let task_view = task_view.unwrap();

        Rc::clone(&task_view).set_title(title_without_null_bytes);
        self_clone.redraw();

        // TODO(PT): Run sizers for everything
    }

    pub fn handle_window_minimize_request(
        self: Rc<Self>,
        event: &AwmDockWindowMinimizeRequestedEvent,
    ) {
        printf!(
            "Got request to minimize a window! Window ID {}\n",
            event.window_id
        );
        // Update the title of the affected task view
        let task_views = self.task_views.borrow();
        let task_view = task_views
            .iter()
            .filter(|tv| tv.window_id == event.window_id)
            .next();

        if let None = task_view {
            printf!("No task view for the provided window ID, skipping it...");
            return;
        }
        let task_view = task_view.unwrap();
        // Send back info to awm informing it where to minimize the window to
        amc_message_send(
            AwmWindow::AWM_SERVICE_NAME,
            AwmDockWindowMinimizeWithInfo::new(task_view.window_id, task_view.frame()),
        );
    }

    fn remove_task_view_from_superview_by_window_id(&self, window_id: u32) {
        let task_views = self.task_views.borrow();
        let task_view = task_views
            .iter()
            .filter(|tv| tv.window_id == window_id)
            .next();

        if let None = task_view {
            printf!("No task view for the provided window ID, skipping it...");
            return;
        }
        let task_view = task_view.unwrap();
        printf!("Found task view with window ID {}\n", task_view.window_id);

        // Remove the task view from its superview
        Rc::clone(&self.container_view)
            .remove_component(&(Rc::clone(task_view) as Rc<dyn UIElement>));
    }

    fn remove_task_view_by_window_id(&self, window_id: u32) {
        self.remove_task_view_from_superview_by_window_id(window_id);
        // Drop the specified task view
        // Note that if an invalid window ID is passed, the error will be silently dropped
        let mut task_views = self.task_views.borrow_mut();
        task_views.retain(|task_view| task_view.window_id != window_id);

        // Update the entry indexes of each task view
        for (entry_idx, task_view) in task_views.iter_mut().enumerate() {
            task_view.set_entry_index(entry_idx)
        }

        // Run sizers for each task view, now that their positions need re-adjusting
    }

    pub fn handle_window_closed(self: Rc<Self>, event: &AwmDockWindowClosed) {
        printf!("Window closed! {}\n", event.window_id);

        self.remove_task_view_by_window_id(event.window_id);
        // TODO(PT): Run sizers for everything
        self.redraw();
    }

    pub fn redraw(self: Rc<Self>) {
        let window = Rc::clone(&self.window);
        window.resize_subviews();

        let task_views = self.task_views.borrow();
        for task_view in task_views.iter() {
            Bordered::draw_rc(Rc::clone(&task_view));
        }

        //window.resize_subviews();
        //window.commit();
    }
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service(AWM_DOCK_SERVICE_NAME);

    let dock_size = Size::new(1920, AWM_DOCK_HEIGHT);
    let window = Rc::new(AwmWindow::new("Dock", dock_size));
    let dock = Rc::new(RefCell::new(Dock::new(Rc::clone(&window), dock_size)));

    window.add_message_handler(move |_window, msg_unparsed: AmcMessage<[u8]>| {
        printf!("Dock got message from {}!\n", msg_unparsed.source());
        if (msg_unparsed.source() == AwmWindow::AWM_SERVICE_NAME) {
            let raw_body = msg_unparsed.body();
            let event = u32::from_ne_bytes(
                // We must slice the array to the exact size of a u32 for the conversion to succeed
                raw_body[..core::mem::size_of::<u32>()]
                    .try_into()
                    .expect("Failed to get 4-length array from message body"),
            );

            let consumed = unsafe {
                match event {
                    AwmDockWindowCreatedEvent::EXPECTED_EVENT => {
                        Rc::clone(&dock.borrow())
                            .handle_window_created(Dock::body_as_type_unchecked(raw_body));
                        true
                    }
                    AwmDockWindowTitleUpdatedEvent::EXPECTED_EVENT => {
                        printf!("Received dock window created event!\n");
                        Rc::clone(&dock.borrow())
                            .handle_window_title_updated(Dock::body_as_type_unchecked(raw_body));
                        true
                    }
                    AwmDockWindowMinimizeRequestedEvent::EXPECTED_EVENT => {
                        printf!("Received window minimize request!\n");
                        Rc::clone(&dock.borrow())
                            .handle_window_minimize_request(Dock::body_as_type_unchecked(raw_body));
                        true
                    }
                    AwmDockWindowClosed::EXPECTED_EVENT => {
                        printf!("Received window closed event!\n");
                        Rc::clone(&dock.borrow())
                            .handle_window_closed(Dock::body_as_type_unchecked(raw_body));
                        true
                    }
                    _ => false,
                }
            };
            if consumed {
                return;
            }
        }

        printf!("Didn't consume event because it was unknown!\n");
    });

    window.enter_event_loop();
    0
}
