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
use libgui::window::AwmWindow;

use axle_rt::{
    amc_message_await, amc_message_send, amc_register_service,
    core_commands::AmcQueryServiceRequest, printf, AmcMessage,
};

use agx_definitions::{
    Color, Drawable, LayerSlice, LikeLayerSlice, Line, NestedLayerSlice, Point, Rect, RectInsets,
    Size, StrokeThickness,
};

use file_manager_messages::{
    str_from_u8_nul_utf8_unchecked, DirectoryContents, DirectoryEntry, LaunchProgram,
    ReadDirectory, FILE_SERVER_SERVICE_NAME,
};

fn select_current_path_view_height(superview_size: Size) -> isize {
    let min_height = 100;
    let divisor_for_top_view = 6;
    cmp::max(min_height, superview_size.height / divisor_for_top_view)
}

struct CurrentPathView {
    view: Rc<View>,
    current_path_label: RefCell<Rc<Label>>,
    pub current_path: RefCell<String>,
    pub back_button: Rc<Button>,
}

impl CurrentPathView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(sizer: F) -> Self {
        let view = Rc::new(View::new(Color::light_gray(), sizer));

        let initial_path = "/";
        let current_path_label = Rc::new(Label::new(
            Rect::new(10, 10, 400, 16),
            &format!("Current path: {}", initial_path),
            Color::new(30, 30, 30),
        ));
        let current_path_label_clone = Rc::clone(&current_path_label);
        Rc::clone(&view).add_component(current_path_label_clone);

        let back_button = Rc::new(Button::new("Go Back", |_b, superview_size| {
            let size = Size::new(80, 30);
            Rect::from_parts(
                Point::new(10, superview_size.height - size.height - 10),
                size,
            )
        }));
        let back_button_clone = Rc::clone(&back_button);
        Rc::clone(&view).add_component(back_button_clone);

        // TODO(PT): Read shortcuts from /config/file_browser/shortcuts.txt

        CurrentPathView {
            view: view,
            current_path_label: RefCell::new(Rc::clone(&current_path_label)),
            current_path: RefCell::new(initial_path.to_string()),
            back_button,
        }
    }

    pub fn set_path(&self, path: &str) {
        self.current_path.replace(path.to_string());
        let current_path_label = self.current_path_label.borrow();
        current_path_label.set_text(&format!("Current path: {}", self.current_path.borrow()));
    }
}

impl NestedLayerSlice for CurrentPathView {
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

impl Drawable for CurrentPathView {
    fn frame(&self) -> Rect {
        self.view.frame()
    }

    fn content_frame(&self) -> Rect {
        //self.view.content_frame()
        Bordered::content_frame(self)
    }

    fn draw(&self) {
        Bordered::draw(self)
    }
}

impl Bordered for CurrentPathView {
    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        self.view.draw_inner_content(outer_frame, onto);
    }

    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }
}

impl UIElement for CurrentPathView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered()
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited()
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

struct DirectoryEntryView {
    view: Rc<View>,
    pub entry: DirectoryEntry,
    background_color: Color,
    button: Rc<Button>,
}

impl DirectoryEntryView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(
        entry_index: usize,
        entry: DirectoryEntry,
        sizer: F,
    ) -> Self {
        let background_color = match entry_index % 2 {
            0 => Color::new(140, 140, 140),
            1 => Color::new(120, 120, 120),
            _ => panic!("Should never happen"),
        };
        let view = Rc::new(View::new(background_color, sizer));
        view.set_border_enabled(false);

        let button_width = 60;
        let height = 30;

        let entry_name = str_from_u8_nul_utf8_unchecked(&entry.name);
        let label_suffix = match entry.is_directory {
            true => "/",
            false => "",
        };
        let label_text = format!("{entry_name}{label_suffix}");
        let name_label = Rc::new(Label::new(
            Rect::from_parts(Point::new(10, 10), Size::new(300, height)),
            &label_text,
            Color::new(30, 30, 30),
        ));
        // TODO(PT): Set font size as attribute?
        Rc::clone(&view).add_component(name_label);

        let button_text = match entry.is_directory {
            true => "Browse",
            false => "Open",
        };
        let button = Rc::new(Button::new(button_text, move |_b, superview_size| {
            //printf!("Button sizer, Superview size {:?}\n", superview_size);
            let size = Size::new(button_width, height - 4);
            Rect::from_parts(Point::new(superview_size.width - size.width - 10, 1), size)
        }));
        let button_clone = Rc::clone(&button);
        Rc::clone(&view).add_component(button_clone);

        DirectoryEntryView {
            view,
            entry,
            background_color,
            button,
        }
    }
}

impl NestedLayerSlice for DirectoryEntryView {
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

impl Drawable for DirectoryEntryView {
    fn frame(&self) -> Rect {
        self.view.frame()
    }

    fn content_frame(&self) -> Rect {
        Bordered::content_frame(self)
    }

    fn draw(&self) {
        Bordered::draw(self);
        let button_origin_x = self.button.frame().min_x();
        let divider_origin_x = button_origin_x - 20;
        let gap = 8;

        let divider = Line::new(
            Point::new(divider_origin_x, gap - 1),
            Point::new(divider_origin_x, self.frame().size.height - gap),
        );
        divider.draw(
            &mut self.get_slice(),
            Color::new(80, 80, 80),
            StrokeThickness::Width(1),
        );
    }
}

// TODO(PT): Implement UnborderedView (Or View vs BorderedView)
impl Bordered for DirectoryEntryView {
    fn draw_border(&self) -> Rect {
        let onto = self.get_slice();
        let border_rect = Rect::from_parts(Point::zero(), onto.frame().size);

        let border_color = match self.currently_contains_mouse() {
            //true => Color::white(),
            true => Color::new(20, 80, 160),
            false => self.background_color,
        };
        let border_thickness = 1;
        onto.fill_rect(
            border_rect,
            border_color,
            StrokeThickness::Width(border_thickness),
        );
        let inner_content_rect = border_rect.inset_by(
            border_thickness,
            border_thickness,
            border_thickness,
            border_thickness,
        );
        inner_content_rect
    }

    fn border_insets(&self) -> RectInsets {
        RectInsets::new(0, 0, 0, 0)
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        self.view.draw_inner_content(outer_frame, onto);
    }
}

impl UIElement for DirectoryEntryView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered();
        self.draw_border();
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited();
        self.draw_border();
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

struct DirectoryContentsView {
    view: Rc<View>,
    dir_entry_views: Vec<Rc<DirectoryEntryView>>,
}

impl DirectoryContentsView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(path: &str, sizer: F) -> Self {
        let view = Rc::new(View::new(Color::new(170, 170, 170), sizer));

        // TODO(PT): Should return the normalized path (ie strip extra slashes and normalize ../)
        amc_message_send(FILE_SERVER_SERVICE_NAME, ReadDirectory::new(path));
        let dir_contents: AmcMessage<DirectoryContents> =
            amc_message_await(Some(FILE_SERVER_SERVICE_NAME));

        let entry_height = 30;

        let mut dir_entry_views = Vec::new();

        for (i, entry) in dir_contents
            .body()
            .entries
            .iter()
            .filter_map(|e| e.as_ref())
            .enumerate()
        {
            let entry_view = Rc::new(DirectoryEntryView::new(
                i,
                *entry,
                move |_dir_entry_view, superview_size| {
                    Rect::from_parts(
                        Point::new(0, (i * entry_height).try_into().unwrap()),
                        Size::new(superview_size.width, entry_height.try_into().unwrap()),
                    )
                },
            ));
            dir_entry_views.push(Rc::clone(&entry_view));
            Rc::clone(&view).add_component(entry_view);
        }

        DirectoryContentsView {
            view,
            dir_entry_views,
        }
    }
}

impl Drawable for DirectoryContentsView {
    fn frame(&self) -> Rect {
        self.view.frame()
    }

    fn content_frame(&self) -> Rect {
        Bordered::content_frame(self)
    }

    fn draw(&self) {
        Bordered::draw(self);
    }
}

impl Bordered for DirectoryContentsView {
    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        self.view.draw_inner_content(outer_frame, onto);
    }

    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }
}

impl UIElement for DirectoryContentsView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered()
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited()
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

impl NestedLayerSlice for DirectoryContentsView {
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

struct FileBrowser2 {
    pub window: Rc<AwmWindow>,
    pub current_path_view: RefCell<Rc<CurrentPathView>>,
    pub directory_contents_view: RefCell<Option<Rc<DirectoryContentsView>>>,
    pub history: RefCell<Vec<String>>,
}

impl FileBrowser2 {
    fn new(window: Rc<AwmWindow>) -> Rc<Self> {
        let current_path_view =
            RefCell::new(Rc::new(CurrentPathView::new(move |_v, superview_size| {
                Rect::from_parts(
                    Point::zero(),
                    Size::new(
                        superview_size.width,
                        select_current_path_view_height(superview_size),
                    ),
                )
            })));
        let current_path_view_clone = Rc::clone(&current_path_view.borrow());
        let current_path_view_clone2 = Rc::clone(&current_path_view.borrow());
        let window_clone = Rc::clone(&window);
        let window_clone2 = Rc::clone(&window);

        window_clone.add_component(current_path_view_clone);

        let browser = Rc::new(FileBrowser2 {
            window: window,
            current_path_view: current_path_view,
            directory_contents_view: RefCell::new(None),
            history: RefCell::new(Vec::new()),
        });

        let browser_clone_for_back_button_closure = Rc::clone(&browser);
        current_path_view_clone2
            .back_button
            .on_left_click(move |_b| {
                printf!("Go back button clicked!\n");
                Rc::clone(&browser_clone_for_back_button_closure).browse_back();
            });

        let browser_clone = Rc::clone(&browser);
        let dir_contents_view = FileBrowser2::create_directory_contents_view(browser_clone);
        //let dir_contents_view_clone = Rc::clone(&dir_contents_view);
        window_clone2.add_component(dir_contents_view);

        browser
    }

    fn create_directory_contents_view(self: Rc<FileBrowser2>) -> Rc<DirectoryContentsView> {
        let directory_contents_view = Rc::new(DirectoryContentsView::new(
            &self.current_path_view.borrow().current_path.borrow(),
            move |_v, superview_size| {
                let current_path_view_height = select_current_path_view_height(superview_size);
                Rect::from_parts(
                    Point::new(0, current_path_view_height),
                    Size::new(
                        superview_size.width,
                        superview_size.height - current_path_view_height,
                    ),
                )
            },
        ));

        for entry_view in &directory_contents_view.dir_entry_views {
            let entry = entry_view.entry;
            let path = str_from_u8_nul_utf8_unchecked(&entry.name).to_string();
            let browser_clone = Rc::clone(&self);

            if entry.is_directory {
                let c = Rc::clone(&directory_contents_view);
                let w = Rc::clone(&self.window);
                entry_view.button.on_left_click(move |_b| {
                    printf!("Button with path {:?} clicked!\n", path);
                    let browser_clone = Rc::clone(&browser_clone);
                    // Fetch the contents of the new directory and add it to the view hierarchy
                    browser_clone.browse_by_appending_path_component(&path);
                });
            } else {
                entry_view.button.on_left_click(move |_b| {
                    printf!("Button with path {:?} clicked!\n", path);
                    let browser_clone = Rc::clone(&browser_clone);
                    let full_path = format!(
                        "{}/{}",
                        browser_clone
                            .current_path_view
                            .borrow()
                            .current_path
                            .borrow(),
                        path
                    );

                    if [".bmp", ".jpg", ".jpeg"]
                        .iter()
                        .any(|&ext| full_path.ends_with(ext))
                    {
                        printf!("\tSeems to be an image file, launching image viewer...\n");
                        FileBrowser2::launch_image_viewer_if_necessary();
                        image_viewer_messages::LoadImage::send(&full_path);
                    } else {
                        printf!("\tSeems to be a program, launching...\n");
                        amc_message_send(FILE_SERVER_SERVICE_NAME, LaunchProgram::new(&full_path));
                    }
                });
            }
        }

        let directory_contents_view_clone = Rc::clone(&directory_contents_view);
        self.directory_contents_view
            .replace(Some(directory_contents_view_clone));

        directory_contents_view
    }

    fn launch_image_viewer_if_necessary() {
        let exists_msg = AmcQueryServiceRequest::send("com.axle.image_viewer");
        printf!("Got response from query service: {exists_msg:?}\n");
        if exists_msg.service_exists {
            printf!("Will not launch image viewer because it's already active\n");
            return;
        }

        let path = "/usr/applications/image_viewer";
        amc_message_send(FILE_SERVER_SERVICE_NAME, LaunchProgram::new(&path));
    }

    fn browse_to_path_without_appending_to_history(self: Rc<FileBrowser2>, path: &str) {
        // Remove the old view
        {
            let browser_clone = Rc::clone(&self);
            let directory_contents_field_attr = &*browser_clone.directory_contents_view.borrow();
            if let Some(old_directory_contents_field) = directory_contents_field_attr {
                let old_directory_contents_field_clone = Rc::clone(&old_directory_contents_field);
                self.window
                    .remove_element(old_directory_contents_field_clone);
            }
        }

        let browser_clone = Rc::clone(&self);

        // Append the path component in the status bar
        let current_path_view = browser_clone.current_path_view.borrow();
        current_path_view.set_path(path);

        // Read the directory and create a new directory contents view
        let directory_contents_view = self.create_directory_contents_view();
        let directory_contents_view_clone = Rc::clone(&directory_contents_view);
        let directory_contents_view_clone2 = Rc::clone(&directory_contents_view);
        *browser_clone.directory_contents_view.borrow_mut() = Some(directory_contents_view_clone);

        let window = Rc::clone(&browser_clone.window);
        let window_clone = Rc::clone(&window);
        window.add_component(directory_contents_view_clone2);

        // Redraw the status bar since the current path has updated
        Bordered::draw(&**current_path_view);

        // Redraw the contents view as we've got new directory contents to display
        Bordered::draw(&*directory_contents_view);

        // TODO(PT): Remove
        window_clone.commit();
    }

    fn browse_to_path(self: Rc<FileBrowser2>, path: &str) {
        {
            // TODO(PT): Might need its own scope
            // Store the path we're browsing away from in the history
            let mut history = self.history.borrow_mut();
            history.push((self.current_path_view.borrow().current_path.borrow()).clone());
        }

        self.browse_to_path_without_appending_to_history(path);
    }

    fn browse_by_appending_path_component(self: Rc<FileBrowser2>, path_component: &str) {
        let path = self
            .current_path_view
            .borrow()
            .current_path
            .borrow()
            .clone();
        let new_path = match path.as_str() {
            "/" => format!("/{path_component}"),
            _ => format!("{path}/{path_component}"),
        };
        self.browse_to_path(&new_path);
    }

    fn pop_from_history(self: Rc<FileBrowser2>) -> Option<String> {
        let mut history = self.history.borrow_mut();
        history.pop()
    }

    fn browse_back(self: Rc<FileBrowser2>) {
        match Rc::clone(&self).pop_from_history() {
            None => {
                printf!("Will not browse back because history is empty\n");
            }
            Some(last_path) => {
                printf!("Browsing back to {last_path}\n");
                self.browse_to_path_without_appending_to_history(&last_path);
            }
        };
    }
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service("com.axle.file_browser");

    // TODO(PT): Add a main content view to Window?
    let window = Rc::new(AwmWindow::new("File Browser", Size::new(500, 600)));
    let _file_browser = Rc::new(RefCell::new(FileBrowser2::new(Rc::clone(&window))));

    window.enter_event_loop();
    0
}
