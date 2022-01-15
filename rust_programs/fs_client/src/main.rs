#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate libc;

use alloc::{collections::BTreeMap, fmt::format, format, rc::Weak};
use alloc::{
    rc::Rc,
    string::{String, ToString},
};
use bordered::Bordered;
use button::Button;
use core::{cell::RefCell, cmp};
use label::Label;
use view::View;

use axle_rt::{amc_message_await, amc_message_send, amc_register_service, printf, AmcMessage};

use agx_definitions::{
    Color, Drawable, Layer, LayerSlice, NestedLayerSlice, Point, Rect, Size, StrokeThickness,
};

use file_manager_messages::{
    str_from_u8_nul_utf8_unchecked, FileManagerDirectoryContents, FileManagerReadDirectory,
};

mod bordered;
mod button;
mod font;
mod label;
mod ui_elements;
mod view;
mod window;
mod window_events;

use ui_elements::UIElement;
use window::AwmWindow;

struct FileBrowser {
    pub window: AwmWindow,
    pub current_path: RefCell<String>,
}

impl FileBrowser {
    fn new(window: AwmWindow) -> Self {
        FileBrowser {
            window: window,
            current_path: RefCell::new("/".to_string()),
        }
    }

    fn layout(controller_rc: Rc<RefCell<FileBrowser>>) {
        /*
        let fs_server = "com.axle.file_manager2";
        let current_path = {
            let controller_borrow = controller_rc.borrow();
            let p = controller_borrow.current_path.borrow();
            p.clone()
        };
        // TODO(PT): Should return the normalized path (ie strip extra slashes and normalize ../)
        amc_message_send(fs_server, FileManagerReadDirectory::new(&current_path));
        let dir_contents: AmcMessage<FileManagerDirectoryContents> =
            amc_message_await(Some(fs_server));

        let window = &controller_rc.borrow().window;
        window.drop_all_ui_elements();

        let current_path_label = Rc::new(Label::new(
            Rect::new(10, 10, 100, 20),
            &format!("Current directory: {}", current_path).to_string(),
            Color::white(),
        ));
        let label_clone = Rc::clone(&current_path_label);
        window.add_component(label_clone);

        let back_button = Rc::new(Button::new(
            Rect::new(10, current_path_label.frame().max_y(), 100, 24),
            "<-- Go Back",
        ));
        let back_button_clone = Rc::clone(&back_button);
        let controller_clone_for_back_button = Rc::clone(&controller_rc);
        window.add_component(back_button_clone);
        back_button.on_left_click(move |_b| {
            {
                let c4 = Rc::clone(&controller_clone_for_back_button);
                let borrowed_controller = c4.borrow();
                let mut current_path = borrowed_controller.current_path.borrow_mut();
                let last_slash_idx = current_path.match_indices("/").last().unwrap().0;

                let back_dir;
                if last_slash_idx == 0 || current_path.len() == 0 || current_path.len() == 1 {
                    back_dir = "/".to_string();
                } else {
                    back_dir = current_path[..last_slash_idx].to_string();
                }
                *current_path = back_dir.clone();
            }

            let c3 = Rc::clone(&controller_clone_for_back_button);
            FileBrowser::layout(c3);
        });

        let mut draw_position = Point::new(10, back_button.frame().max_y() + 20);
        let button_height = 24;

        for entry in dir_contents
            .body()
            .entries
            .iter()
            .filter_map(|e| e.as_ref())
        {
            let entry_name = str_from_u8_nul_utf8_unchecked(&entry.name);
            let char_width = 8;
            let button_width = ((entry_name.len() * char_width) + (2 * char_width)) as isize;

            let button = Rc::new(Button::new(
                Rect::new(
                    draw_position.x,
                    draw_position.y,
                    button_width,
                    button_height,
                ),
                entry_name,
            ));
            let button_clone = Rc::clone(&button);

            window.add_component(button_clone);

            let controller_clone = Rc::clone(&controller_rc);
            if entry.is_directory {
                button.on_left_click(move |b| {
                    let c3 = Rc::clone(&controller_clone);
                    let c4 = Rc::clone(&controller_clone);
                    {
                        let borrowed_controller = c3.borrow();
                        let mut current_path = borrowed_controller.current_path.borrow_mut();
                        let dir_separator = match current_path.as_str() {
                            "/" => "",
                            _ => "/",
                        };
                        current_path
                            .push_str(&format!("{}{}", dir_separator, &b.label).to_string());
                    }
                    FileBrowser::layout(c4);
                });
            } else {
                button.on_left_click(move |b| {
                    printf!("{:?} is not a directory!\n", b.label);
                });
            }

            draw_position.y += button_height + 20;
            if draw_position.y > 800 {
                break;
            }
        }

        window.draw();
        window.commit();
        */
    }

    fn layout2(controller_rc: Rc<RefCell<FileBrowser>>) {
        /*
        let window = &controller_rc.borrow().window;

        let view = Rc::new(View::new(Color::red(), |v, superview_size| {
            Rect::from_parts(Point::zero(), superview_size)
        }));
        let view_clone = Rc::clone(&view);

        let mut cursor = Point::new(10, 10);
        for i in 0..1 {
            let button = Rc::new(Button::new(
                Rect::from_parts(cursor, Size::new(100, 40)),
                &format!("Button {}", i),
            ));
            view.add_component(button);
            cursor.y += 50;
        }

        window.add_component(view_clone);
        window.draw();
        window.commit();
        */
    }
}

fn select_current_path_view_height(superview_size: Size) -> isize {
    let min_height = 100;
    let divisor_for_top_view = 8;
    cmp::max(min_height, superview_size.height / divisor_for_top_view)
}

struct CurrentPathView {
    view: View,
    current_path_label: RefCell<Rc<Label>>,
    pub current_path: RefCell<String>,
}

impl CurrentPathView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(sizer: F) -> Self {
        let view = View::new(Color::light_gray(), sizer);

        let initial_path = "/";
        let current_path_label = Rc::new(Label::new(
            Rect::new(10, 10, 200, 16),
            &format!("Current path: {}", initial_path),
            Color::new(30, 30, 30),
        ));
        let current_path_label_clone = Rc::clone(&current_path_label);
        view.add_component(current_path_label_clone);

        let back_button = Rc::new(Button::new(
            Rect::from_parts(
                Point::new(10, current_path_label.frame().max_y()),
                Size::new(90, 34),
            ),
            "Go Back",
        ));
        let back_button_clone = Rc::clone(&back_button);
        view.add_component(back_button_clone);

        CurrentPathView {
            view: view,
            current_path_label: RefCell::new(Rc::clone(&current_path_label)),
            current_path: RefCell::new(initial_path.to_string()),
        }
    }

    pub fn append_path_component(&self, path: &str) {
        self.current_path.replace_with(|old| {
            old.push_str(&format!("/{}", path));
            old.to_string()
        });
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

    fn get_slice(&self) -> LayerSlice {
        self.view.get_slice()
    }
}

impl Drawable for CurrentPathView {
    fn frame(&self) -> Rect {
        self.view.frame()
    }

    fn draw(&self, onto: &mut LayerSlice) {
        Bordered::draw(self, onto)
    }
}

impl Bordered for CurrentPathView {
    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut LayerSlice) {
        self.view.draw_inner_content(outer_frame, onto);
    }
}

impl UIElement for CurrentPathView {
    fn handle_mouse_entered(&self, onto: &mut LayerSlice) {
        self.view.handle_mouse_entered(onto)
    }

    fn handle_mouse_exited(&self, onto: &mut LayerSlice) {
        self.view.handle_mouse_exited(onto)
    }

    fn handle_mouse_moved(&self, mouse_point: Point, onto: &mut LayerSlice) {
        self.view.handle_mouse_moved(mouse_point, onto)
    }

    fn handle_left_click(&self) {
        self.view.handle_left_click()
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size)
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}

struct DirectoryContentsView {
    view: View,
    path_to_button_map: BTreeMap<String, Rc<Button>>,
}

impl DirectoryContentsView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(path: &str, sizer: F) -> Self {
        let view = View::new(Color::new(170, 170, 170), sizer);

        let fs_server = "com.axle.file_manager2";
        // TODO(PT): Should return the normalized path (ie strip extra slashes and normalize ../)
        amc_message_send(fs_server, FileManagerReadDirectory::new(path));
        let dir_contents: AmcMessage<FileManagerDirectoryContents> =
            amc_message_await(Some(fs_server));

        let mut cursor = Point::new(10, 10);
        let button_height = 30;

        let mut path_to_button_map = BTreeMap::new();

        for entry in dir_contents
            .body()
            .entries
            .iter()
            .filter_map(|e| e.as_ref())
        {
            let entry_name = str_from_u8_nul_utf8_unchecked(&entry.name);

            let button_width = 100;
            let button = Rc::new(Button::new(
                Rect::from_parts(cursor, Size::new(button_width, button_height)),
                entry_name,
            ));
            let button_clone = Rc::clone(&button);
            view.add_component(button_clone);

            path_to_button_map.insert(entry_name.to_string(), button);

            cursor.y += button_height + 20;
            if cursor.y > 500 {
                break;
            }
        }

        DirectoryContentsView {
            view: view,
            path_to_button_map,
        }
    }
}

impl Drawable for DirectoryContentsView {
    fn frame(&self) -> Rect {
        self.view.frame()
    }

    fn draw(&self, onto: &mut LayerSlice) {
        Bordered::draw(self, onto)
    }
}

impl Bordered for DirectoryContentsView {
    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut LayerSlice) {
        self.view.draw_inner_content(outer_frame, onto);
    }
}

impl UIElement for DirectoryContentsView {
    fn handle_mouse_entered(&self, onto: &mut LayerSlice) {
        self.view.handle_mouse_entered(onto)
    }

    fn handle_mouse_exited(&self, onto: &mut LayerSlice) {
        self.view.handle_mouse_exited(onto)
    }

    fn handle_mouse_moved(&self, mouse_point: Point, onto: &mut LayerSlice) {
        self.view.handle_mouse_moved(mouse_point, onto)
    }

    fn handle_left_click(&self) {
        self.view.handle_left_click()
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

    fn get_slice(&self) -> LayerSlice {
        self.view.get_slice()
    }
}

struct FileBrowser2 {
    pub window: Rc<AwmWindow>,
    pub current_path_view: RefCell<Rc<CurrentPathView>>,
    pub directory_contents_view: RefCell<Option<Rc<DirectoryContentsView>>>,
}

impl FileBrowser2 {
    fn new(window: Rc<AwmWindow>) -> Rc<Self> {
        let window_clone = Rc::clone(&window);
        let current_path_view =
            RefCell::new(Rc::new(CurrentPathView::new(move |v, superview_size| {
                Rect::from_parts(
                    Point::zero(),
                    Size::new(
                        superview_size.width,
                        select_current_path_view_height(superview_size),
                    ),
                )
            })));
        let current_path_view_clone = Rc::clone(&current_path_view.borrow());
        let window_clone = Rc::clone(&window);
        let window_clone2 = Rc::clone(&window);

        window_clone.add_component(current_path_view_clone);

        let browser = Rc::new(FileBrowser2 {
            window: window,
            current_path_view: current_path_view,
            directory_contents_view: RefCell::new(None),
        });

        let browser_clone = Rc::clone(&browser);
        let dir_contents_view = FileBrowser2::create_directory_contents_view(browser_clone);
        //let dir_contents_view_clone = Rc::clone(&dir_contents_view);
        window_clone2.add_component(dir_contents_view);

        browser
    }

    //fn create_directory_contents_view(&self) -> Rc<DirectoryContentsView> {
    // TODO(PT): Change to self: Rc
    fn create_directory_contents_view(browser: Rc<FileBrowser2>) -> Rc<DirectoryContentsView> {
        let directory_contents_view = Rc::new(DirectoryContentsView::new(
            &browser.current_path_view.borrow().current_path.borrow(),
            move |v, superview_size| {
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

        for (path, button) in &directory_contents_view.path_to_button_map {
            let path_copy = path.to_string();
            let browser_clone = Rc::clone(&browser);

            let window_clone = Rc::clone(&browser.window);
            button.on_left_click(move |b| {
                printf!("Button with path {:?} clicked!\n", path_copy);

                let browser_clone = Rc::clone(&browser_clone);
                let browser_clone2 = Rc::clone(&browser_clone);

                // Fetch the contents of the new directory and add it to the view hierarchy
                FileBrowser2::browse_by_appending_path_component(browser_clone, &path_copy);

                // Redraw the status bar since the current path has updated
                let path_view = &**browser_clone2.current_path_view.borrow();
                let mut slice = path_view.get_slice();
                Bordered::draw(path_view, &mut slice);

                // Redraw the contents view as we've got new directory contents to display
                let contents_view_container = browser_clone2.directory_contents_view.borrow();
                let contents_view = &**contents_view_container.as_ref().unwrap();
                let mut slice2 = contents_view.get_slice();
                Bordered::draw(contents_view, &mut slice2);

                window_clone.commit();
            });
        }

        let directory_contents_view_clone = Rc::clone(&directory_contents_view);
        let old_directory_contents_view = browser
            .directory_contents_view
            .replace_with(|old| Some(directory_contents_view_clone));

        directory_contents_view
    }

    fn browse_by_appending_path_component(browser: Rc<FileBrowser2>, path: &str) {
        // Remove the old view
        {
            let browser_clone = Rc::clone(&browser);
            let directory_contents_field_attr = &*browser_clone.directory_contents_view.borrow();
            if let Some(old_directory_contents_field) = directory_contents_field_attr {
                let old_directory_contents_field_clone = Rc::clone(&old_directory_contents_field);
                browser
                    .window
                    .remove_element(old_directory_contents_field_clone);
            }
        }

        let browser_clone = Rc::clone(&browser);

        // Append the path component in the status bar
        printf!("Getting current path view\n");
        let current_path_view = browser_clone.current_path_view.borrow();
        current_path_view.append_path_component(path);
        // TODO(PT): How to only mark the label as needing redraw?
        //current_path_view.current_path_label.nee

        // Read the directory and create a new directory contents view
        let directory_contents_view = FileBrowser2::create_directory_contents_view(browser);
        let directory_contents_view_clone = Rc::clone(&directory_contents_view);
        let directory_contents_view_clone2 = Rc::clone(&directory_contents_view);
        *browser_clone.directory_contents_view.borrow_mut() = Some(directory_contents_view_clone);

        let window = Rc::clone(&browser_clone.window);
        window.add_component(directory_contents_view_clone2);
    }

    fn layout(controller_rc: Rc<RefCell<FileBrowser2>>) {
        let controller = controller_rc.borrow();
        let window = &controller_rc.borrow().window;

        // TODO(PT): Add a main content view to Window?
        /*
        let current_path_view = Rc::new(View::new(Color::blue(), |v, superview_size| {

        }));
        let mut current_path_view_cell = &mut *controller.current_path_view.borrow_mut();
        *current_path_view_cell = Some(Rc::clone(&current_path_view));
        let current_path_view_clone = Rc::clone(&current_path_view);
        window.add_component(current_path_view_clone);
        */

        /*
        let current_path_view = Rc::new(View::new(Color::light_gray(), |v, superview_view|{
            Rect::from_parts(Point::zero(),
        });


        let view = Rc::new(View::new(Color::red(), |v, superview_size| {
            Rect::from_parts(Point::zero(), superview_size)
        }));
        let view_clone = Rc::clone(&view);

        let mut cursor = Point::new(10, 10);
        for i in 0..1 {
            let button = Rc::new(Button::new(
                Rect::from_parts(cursor, Size::new(100, 40)),
                &format!("Button {}", i),
            ));
            view.add_component(button);
            cursor.y += 50;
        }
        */

        window.draw();
        window.commit();
    }
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service("com.axle.fs2_client");

    // Just solved a bug where it appeared that the Rust code would not draw past
    // certain bounds
    // In the end, this is due to awm's window-open animation.
    // awm will only blit the visible portion of the framebuffer,
    // which is smaller than the requested viewport while the window is first opening.
    // rawm doesn't repaint upon resizing, so this appeared as though it couldn't draw
    // to certain places.

    let window = Rc::new(AwmWindow::new(Size::new(500, 800)));
    let file_browser = Rc::new(RefCell::new(FileBrowser2::new(Rc::clone(&window))));
    //FileBrowser2::layout(Rc::clone(&file_browser));

    //let window = &file_browser.borrow().window;

    {
        /*
        let mut layer = &mut window.layer.borrow_mut();

        let slice = layer.get_slice(Rect::new(100, 100, 200, 200));
        slice.fill(Color::white());
        slice.fill_rect(Rect::new(10, 10, 20, 400), Color::red());
        */
    }
    window.draw();
    window.commit();
    window.enter_event_loop();
    0
}
