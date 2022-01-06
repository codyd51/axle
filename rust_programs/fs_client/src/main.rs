#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate libc;

use alloc::{
    boxed::Box,
    string::{String, ToString},
};

use axle_rt::{amc_message_await, amc_message_send, amc_register_service, AmcMessage};

use agx_definitions::{Point, Rect, Size};

use file_manager_messages::{
    str_from_u8_nul_utf8_unchecked, FileManagerDirectoryContents, FileManagerReadDirectory,
};

mod font;
mod ui_elements;
mod window;
mod window_events;

use ui_elements::Button;
use window::AwmWindow;

struct BrowseState {
    current_directory: Option<String>,
}

impl BrowseState {
    pub fn new() -> BrowseState {
        BrowseState {
            current_directory: None,
        }
    }

    fn browse_to(&mut self, path: &str, window: &mut AwmWindow) {
        self.current_directory = Some(path.to_string());

        let fs_server = "com.axle.file_manager2";
        amc_message_send(fs_server, FileManagerReadDirectory::new(path));

        let dir_contents: AmcMessage<FileManagerDirectoryContents> =
            amc_message_await(Some(fs_server));

        let mut draw_position = Point::new(10, 10);
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

            let button = Button::new(
                Rect::new(draw_position, Size::new(button_width, button_height)),
                entry_name,
            );
            window.add_component(Box::new(button));

            draw_position.y += button_height + 20;
        }
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

    let window_size = Size::new(500, 700);
    let mut window = AwmWindow::new(window_size);

    let mut browser: BrowseState = BrowseState::new();
    browser.browse_to("/usr/lib", &mut window);

    // Commit once so the window shows its initial contents before we start processing messages
    window.draw();
    window.commit();

    loop {
        /*
        window.draw();
        window.commit();
        */
        // Update the window with the events we've queued
        window.await_next_event();
        /*
        unsafe {
            libc::usleep(800);
        }
        */
    }

    0
}
