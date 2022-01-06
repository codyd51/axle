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

use axle_rt::printf;
use axle_rt::{amc_message_await, amc_message_send, amc_register_service, AmcMessage};

use agx_definitions::{Color, Layer, Point, Rect, Size};

use file_manager_messages::{
    str_from_u8_nul_utf8_unchecked, FileManagerDirectoryContents, FileManagerReadDirectory,
};

use crate::font::draw_char;

mod font;
mod ui_elements;
mod window;
mod window_events;

use ui_elements::Button;
use window::AwmWindow;

use rand::RngCore;
use rand::SeedableRng;
use rand::{rngs::SmallRng, Rng};

use crate::ui_elements::ClickableElement;
use crate::ui_elements::UIElement;

struct BrowseState {
    current_directory: Option<String>,
    window: &'static mut AwmWindow,
}

impl BrowseState {
    pub fn new(window: &'static mut AwmWindow) -> BrowseState {
        BrowseState {
            current_directory: None,
            window,
        }
    }

    fn browse_to(&'static mut self, path: &str) -> ! {
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

            let mut button = Button::new(
                Rect::new(draw_position, Size::new(button_width, button_height)),
                entry_name,
            );
            self.window.add_component(Box::new(button));

            draw_position.y += button_height + 20;
        }

        self.window.draw();
        self.window.commit();
        loop {
            self.window.await_next_event();
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

    let window_size = Size::new(800, 800);
    let mut window = AwmWindow::new(window_size);

    // Commit once so the window shows its initial contents before we start processing messages
    window.commit();

    let browser: BrowseState = BrowseState::new(&mut window);
    //browser.browse_to("/", &mut window);
    //browser = BrowseState::browse_to(&mut self, path)

    loop {
        // Update the window with the events we've queued
        //window.await_next_event();
        /*
        unsafe {
            libc::usleep(800);
        }
        */
    }

    0
}
