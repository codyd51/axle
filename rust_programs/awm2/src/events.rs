use crate::desktop::DesktopElement;
use crate::window::Window;
use agx_definitions::Point;
use alloc::rc::Rc;
use awm_messages::{
    AwmCloseWindow, AwmKeyDown, AwmKeyUp, AwmMouseEntered, AwmMouseExited, AwmMouseLeftClickEnded,
    AwmMouseLeftClickStarted, AwmMouseMoved, AwmMouseScrolled, AwmWindowResized,
};
use dock_messages::{
    AwmDockWindowClosed, AwmDockWindowCreatedEvent, AwmDockWindowMinimizeRequestedEvent,
    AwmDockWindowTitleUpdatedEvent, AWM_DOCK_SERVICE_NAME,
};

#[cfg(target_os = "axle")]
mod conditional_imports {
    pub use axle_rt::amc_message_send;
}
#[cfg(not(target_os = "axle"))]
mod conditional_imports {}

use crate::events::conditional_imports::*;

pub fn send_left_click_event(window: &Rc<Window>, mouse_pos: Point) {
    let mouse_within_window = window.frame().translate_point(mouse_pos);
    let mouse_within_content_view = window.content_frame().translate_point(mouse_within_window);
    #[cfg(target_os = "axle")]
    {
        amc_message_send(
            &window.owner_service,
            AwmMouseLeftClickStarted::new(mouse_within_content_view),
        );
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!(
            "send_left_click_event({}, {mouse_within_content_view})",
            window.name()
        )
    }
}

pub fn send_left_click_ended_event(window: &Rc<Window>, mouse_pos: Point) {
    let mouse_within_window = window.frame().translate_point(mouse_pos);
    let mouse_within_content_view = window.content_frame().translate_point(mouse_within_window);
    #[cfg(target_os = "axle")]
    {
        amc_message_send(
            &window.owner_service,
            AwmMouseLeftClickEnded::new(mouse_within_content_view),
        );
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!(
            "send_left_click_ended_event({}, {mouse_within_content_view})",
            window.name()
        )
    }
}

pub fn send_mouse_entered_event(window: &Rc<Window>) {
    #[cfg(target_os = "axle")]
    {
        amc_message_send(&window.owner_service, AwmMouseEntered::new())
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("send_mouse_entered_event({})", window.name())
    }
}

pub fn send_mouse_exited_event(window: &Rc<Window>) {
    #[cfg(target_os = "axle")]
    {
        amc_message_send(&window.owner_service, AwmMouseExited::new())
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("send_mouse_exited_event({})", window.name())
    }
}

pub fn send_mouse_moved_event(window: &Rc<Window>, mouse_pos: Point) {
    let mouse_within_window = window.frame().translate_point(mouse_pos);
    let mouse_within_content_view = window.content_frame().translate_point(mouse_within_window);
    #[cfg(target_os = "axle")]
    {
        amc_message_send(
            &window.owner_service,
            AwmMouseMoved::new(mouse_within_content_view),
        )
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!(
            "send_mouse_moved_event({}, {mouse_within_content_view})",
            window.name()
        )
    }
}

pub fn send_mouse_scrolled_event(window: &Rc<Window>, mouse_pos: Point, delta_z: i8) {
    #[cfg(target_os = "axle")]
    {
        let mouse_within_window = window.frame().translate_point(mouse_pos);
        let mouse_within_content_view = window.content_frame().translate_point(mouse_within_window);
        let mouse_scrolled_msg = AwmMouseScrolled::new(mouse_within_content_view, delta_z);
        amc_message_send(&window.owner_service, mouse_scrolled_msg);
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("Mouse scrolled within window {} {delta_z}", window.name());
    }
}

pub fn send_window_resized_event(window: &Rc<Window>) {
    #[cfg(target_os = "axle")]
    {
        amc_message_send(
            &window.owner_service,
            AwmWindowResized::new(window.content_frame().size),
        );
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("send_window_resized_event({})", window.name())
    }
}

pub fn send_close_window_request(window: &Rc<Window>) {
    #[cfg(target_os = "axle")]
    {
        amc_message_send(&window.owner_service, AwmCloseWindow::new());
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("send_close_window_request({})", window.name())
    }
}

pub fn send_key_down_event(window: &Rc<Window>, key: u32) {
    #[cfg(target_os = "axle")]
    {
        amc_message_send(&window.owner_service, AwmKeyDown::new(key));
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("send_key_down_event({}, {key})", window.name())
    }
}

pub fn send_key_up_event(window: &Rc<Window>, key: u32) {
    #[cfg(target_os = "axle")]
    {
        amc_message_send(&window.owner_service, AwmKeyUp::new(key));
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("send_key_up_event({}, {key})", window.name())
    }
}

pub fn send_initiate_window_minimize(window: &Rc<Window>) {
    #[cfg(target_os = "axle")]
    {
        amc_message_send(
            AWM_DOCK_SERVICE_NAME,
            AwmDockWindowMinimizeRequestedEvent::new(window.id()),
        );
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("initiate window minimize({})", window.name())
    }
}

pub fn inform_dock_window_closed(window_id: usize) {
    #[cfg(target_os = "axle")]
    {
        amc_message_send(
            AWM_DOCK_SERVICE_NAME,
            AwmDockWindowClosed::new(window_id as u32),
        );
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("inform_dock_window_closed({window_id})")
    }
}

pub fn inform_dock_window_title_updated(window_id: usize, new_title: &str) {
    #[cfg(target_os = "axle")]
    {
        let dock_notification = AwmDockWindowTitleUpdatedEvent::new(window_id, new_title);
        amc_message_send(AWM_DOCK_SERVICE_NAME, dock_notification);
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("inform_dock_window_title_updated({window_id}, {new_title})")
    }
}

pub fn inform_dock_window_created(window_id: usize, owner_service: &str) {
    #[cfg(target_os = "axle")]
    {
        let msg = AwmDockWindowCreatedEvent::new(window_id, &owner_service);
        amc_message_send(AWM_DOCK_SERVICE_NAME, msg);
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("inform_dock_window_created({window_id})");
    }
}
