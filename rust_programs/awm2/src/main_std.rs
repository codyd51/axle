use crate::desktop::Desktop;
use agx_definitions::{Color, LikeLayerSlice, Point, Size};
use alloc::rc::Rc;
use libgui::PixelLayer;
use pixels::{Error, Pixels, SurfaceTexture};
use std::error;
use winit::event::{MouseButton, MouseScrollDelta};
use winit::event_loop::{ControlFlow, EventLoop};
use winit::window::{Window, WindowBuilder};
use winit::{
    dpi::{LogicalPosition, LogicalSize},
    event::WindowEvent,
};
use winit::{
    event::{ElementState, Event, VirtualKeyCode},
    window::Fullscreen,
};
use awm_messages::AwmCreateWindow;
use mouse_driver_messages::MousePacket;

pub fn main() -> Result<(), Box<dyn error::Error>> {
    let event_loop = EventLoop::new();
    let size = Size::new(1024, 768);
    let mut layer = Rc::new(PixelLayer::new("Hosted awm", &event_loop, size));
    //layer.fill(Color::green());

    //let video_memory_layer = setup_awm_framebuffer();
    let mut desktop = Desktop::new(Rc::clone(&layer) as Rc<dyn LikeLayerSlice>);
    desktop.draw_background();
    // Start off by drawing a blank canvas consisting of the desktop background
    desktop.blit_background();
    desktop.commit_entire_buffer_to_video_memory();

    desktop.spawn_window(
        "Test window".to_string(),
        &AwmCreateWindow::new(Size::new(200, 300))
    );

    let scale_factor = 2;
    let mut last_cursor_pos = None;
    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Poll;
        match event {
            Event::MainEventsCleared => {
                desktop.draw_frame();
                let mut pixel_buffer = layer.pixel_buffer.borrow_mut();
                pixel_buffer.render();
            }
            Event::WindowEvent { window_id, event } => {
                match event {
                    WindowEvent::MouseInput {
                        device_id,
                        state,
                        button,
                        modifiers,
                    } => {
                        println!("MouseInput {state:?}, button {button:?}");
                        match state {
                            ElementState::Pressed => match button {
                                MouseButton::Left => {
                                    //self_clone.left_click(last_cursor_pos);
                                }
                                _ => (),
                            },
                            _ => (),
                        }
                    }
                    WindowEvent::CursorMoved {
                        device_id,
                        position,
                        modifiers,
                    } => {
                        let mouse_pos = Point::new(
                            (position.x as isize) / scale_factor,
                            (position.y as isize) / scale_factor,
                        );
                        if let None = last_cursor_pos {
                            desktop.set_cursor_pos(mouse_pos);
                            last_cursor_pos = Some(mouse_pos);
                        }
                        let rel_x = mouse_pos.x - last_cursor_pos.unwrap().x;
                        let rel_y = mouse_pos.y - last_cursor_pos.unwrap().y;
                        //println!("CursorMoved {rel_x:?}, {rel_y:?}");
                        desktop.handle_mouse_update(&MousePacket::new(rel_x as i8, rel_y as i8));

                        last_cursor_pos = Some(mouse_pos);
                    }
                    WindowEvent::CursorLeft { device_id } => {
                        last_cursor_pos = None;
                    }
                    _ => {}
                }
            }
            _ => {},
        }
    });

    Ok(())
}
