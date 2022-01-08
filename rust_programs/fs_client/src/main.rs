#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate libc;

use alloc::format;
use alloc::{
    rc::Rc,
    string::{String, ToString},
};
use core::cell::RefCell;

use axle_rt::{amc_message_await, amc_message_send, amc_register_service, printf, AmcMessage};

use agx_definitions::{Color, Point, Rect, Size};

use file_manager_messages::{
    str_from_u8_nul_utf8_unchecked, FileManagerDirectoryContents, FileManagerReadDirectory,
};

mod font;
mod ui_elements;
mod window;
mod window_events;

use ui_elements::{Button, Label, UIElement};
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
            Rect::new(Point::new(10, 10), Size::new(100, 20)),
            &format!("Current directory: {}", current_path).to_string(),
            Color::white(),
        ));
        let label_clone = Rc::clone(&current_path_label);
        window.add_component(label_clone);

        let back_button = Rc::new(Button::new(
            Rect::new(
                Point::new(10, current_path_label.frame().max_y()),
                Size::new(100, 24),
            ),
            "<-- Go Back",
            Color::new(100, 200, 100),
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
            let button_width = (entry_name.len() * char_width) + (2 * char_width);

            let button = Rc::new(Button::new(
                Rect::new(draw_position, Size::new(button_width, button_height)),
                entry_name,
                Color::white(),
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

    let window = AwmWindow::new(Size::new(500, 800));
    let file_browser = Rc::new(RefCell::new(FileBrowser::new(window)));
    FileBrowser::layout(Rc::clone(&file_browser));

    let window = &file_browser.borrow().window;
    window.enter_event_loop();
    0
}
