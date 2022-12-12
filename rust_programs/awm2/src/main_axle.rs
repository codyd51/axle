extern crate alloc;
extern crate libc;

use alloc::boxed::Box;
use alloc::{collections::BTreeMap, format, rc::Weak, vec, vec::Vec};
use alloc::{
    rc::Rc,
    string::{String, ToString},
};
use core::cmp::{max, min};
use core::{cell::RefCell, cmp};

use awm_messages::{AwmWindowPartialRedraw, AWM2_SERVICE_NAME};
use axle_rt::{
    amc_has_message, amc_message_await, amc_message_await_untyped, amc_message_send,
    amc_register_service, printf, println, AmcMessage,
};
use axle_rt::{ContainsEventField, ExpectsEventField};

use agx_definitions::{
    Color, Drawable, Layer, LayerSlice, LikeLayerSlice, Line, NestedLayerSlice, Point, Rect,
    RectInsets, SingleFramebufferLayer, Size, StrokeThickness,
};
use awm_messages::{
    AwmCreateWindow, AwmCreateWindowResponse, AwmDesktopTraitsRequest, AwmDesktopTraitsResponse,
    AwmWindowRedrawReady, AwmWindowUpdateTitle,
};

use kb_driver_messages::KB_DRIVER_SERVICE_NAME;
use mouse_driver_messages::{MousePacket, MOUSE_DRIVER_SERVICE_NAME};

use axle_rt::core_commands::{
    AmcAwmMapFramebuffer, AmcAwmMapFramebufferResponse, AmcSharedMemoryCreateRequest,
    AmcSharedMemoryCreateResponse, AMC_CORE_SERVICE_NAME,
};
use libgui::window_events::AwmWindowEvent;
use libgui::AwmWindow;

use crate::desktop::Desktop;

fn setup_awm_framebuffer() -> SingleFramebufferLayer {
    // Ask the kernel to map in the framebuffer and send us info about it
    amc_message_send(AMC_CORE_SERVICE_NAME, AmcAwmMapFramebuffer::new());
    let framebuffer_info_msg: AmcMessage<AmcAwmMapFramebufferResponse> =
        amc_message_await(Some(AMC_CORE_SERVICE_NAME));
    let framebuffer_info = framebuffer_info_msg.body();
    println!("Received framebuffer info: {framebuffer_info:?}");

    let bpp = framebuffer_info.bytes_per_pixel as isize;
    let screen_resolution = Size::new(
        framebuffer_info.width as isize,
        framebuffer_info.height as isize,
    );

    let framebuffer_slice = core::ptr::slice_from_raw_parts_mut(
        framebuffer_info.address as *mut libc::c_void,
        (bpp * screen_resolution.width * screen_resolution.height)
            .try_into()
            .unwrap(),
    );
    let framebuffer: &mut [u8] = unsafe { &mut *(framebuffer_slice as *mut [u8]) };
    SingleFramebufferLayer::from_framebuffer(
        unsafe { Box::from_raw(framebuffer) },
        bpp,
        screen_resolution,
    )
}

unsafe fn body_as_type_unchecked<T: ExpectsEventField + ContainsEventField>(body: &[u8]) -> &T {
    &*(body.as_ptr() as *const T)
}

pub fn main() {
    amc_register_service(AWM2_SERVICE_NAME);

    let mut video_memory_layer = setup_awm_framebuffer();
    let video_memory_slice =
        video_memory_layer.get_slice(Rect::with_size(video_memory_layer.size()));
    let mut desktop = Desktop::new(Rc::new(video_memory_slice));
    desktop.draw_background();
    // Start off by drawing a blank canvas consisting of the desktop background
    desktop.blit_background();
    desktop.commit_entire_buffer_to_video_memory();

    loop {
        /*
        if !amc_has_message(None) {
            continue;
        }
        */

        let msg_unparsed: AmcMessage<[u8]> = unsafe { amc_message_await_untyped(None).unwrap() };

        // Parse the first bytes of the message as a u32 event field
        let raw_body = msg_unparsed.body();
        let event = u32::from_ne_bytes(
            // We must slice the array to the exact size of a u32 for the conversion to succeed
            raw_body[..core::mem::size_of::<u32>()]
                .try_into()
                .expect("Failed to get 4-length array from message body"),
        );

        // Each inner call to body_as_type_unchecked is unsafe because we must be
        // sure we're casting to the right type.
        // Since we verify the type on the LHS, each usage is safe.
        //
        // Wrap the whole thing in an unsafe block to reduce
        // boilerplate in each match arm.
        //
        // Make a copy of the message source to pass around so that callers don't need to worry
        // about its validity
        let msg_source = msg_unparsed.source().to_string();
        unsafe {
            // Try to match special messages from known services first
            let consumed = match msg_source.as_str() {
                MOUSE_DRIVER_SERVICE_NAME => {
                    match event {
                        MousePacket::EXPECTED_EVENT => {
                            desktop.handle_mouse_packet(body_as_type_unchecked(raw_body))
                        }
                        _ => {
                            println!("Ignoring unknown message from mouse driver")
                        }
                    };
                    true
                }
                KB_DRIVER_SERVICE_NAME => {
                    // PT: We can't use body_as_type_unchecked because the message from the KB driver lacks
                    // an event field. Do a direct cast until the KB driver message is fixed.
                    desktop.handle_keyboard_event(unsafe { &*(raw_body.as_ptr() as *const _) });
                    true
                }
                PREFERENCES_SERVICE_NAME => {
                    match event {
                        AwmDesktopTraitsRequest::EXPECTED_EVENT => {
                            amc_message_send(
                                PREFERENCES_SERVICE_NAME,
                                AwmDesktopTraitsResponse::new(
                                    desktop.background_gradient_inner_color,
                                    desktop.background_gradient_outer_color,
                                ),
                            );
                            true
                        }
                        _ => {
                            //println!("Ignoring unknown message from preferences");
                            false
                        }
                    }
                }
            };
            if !consumed {
                // Unknown sender - probably a client wanting to interact with the window manager
                match event {
                    // Mouse driver events
                    // libgui events
                    // Keyboard events
                    AwmCreateWindow::EXPECTED_EVENT => {
                        desktop.spawn_window(&msg_source, body_as_type_unchecked(raw_body), None);
                    }
                    AwmWindowRedrawReady::EXPECTED_EVENT => {
                        //println!("Window said it was ready to redraw!");
                        desktop.handle_window_requested_redraw(msg_unparsed.source());
                    }
                    AwmWindowPartialRedraw::EXPECTED_EVENT => {
                        desktop.handle_window_requested_partial_redraw(
                            msg_unparsed.source(),
                            body_as_type_unchecked(raw_body),
                        );
                    }
                    AwmWindowUpdateTitle::EXPECTED_EVENT => {
                        desktop.handle_window_updated_title(
                            &msg_source,
                            body_as_type_unchecked(raw_body),
                        );
                    }
                    _ => {
                        println!("Awm ignoring message with unknown event type: {event}");
                    }
                }
            }
        }

        // Now that we've processed a message, draw a frame to reflect the updated state
        // Perhaps there are messages for which we can elide a draw?
        desktop.draw_frame();
    }
}
