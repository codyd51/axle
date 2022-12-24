use crate::desktop::{Desktop, RenderStrategy};
use agx_definitions::{LikeLayerSlice, Point, Rect, SingleFramebufferLayer, Size};
use alloc::rc::Rc;
use awm_messages::AwmCreateWindow;
use libgui::PixelLayer;
use rand::prelude::SmallRng;
use rand::{Rng, SeedableRng};
use std::fs::OpenOptions;
use std::io::Write;
use std::time::{SystemTime, UNIX_EPOCH};
use std::{env, error, fs};
use winit::event::MouseButton;
use winit::event::WindowEvent;
use winit::event::{ElementState, Event, VirtualKeyCode};
use winit::event_loop::{ControlFlow, EventLoop};

pub fn main() -> Result<(), Box<dyn error::Error>> {
    /*
    for i in 0..100 {
        replay_capture();
    }
    return Ok(());
    */

    let event_loop = EventLoop::new();
    let desktop_size = Size::new(1920, 1080);
    let layer = Rc::new(Box::new(PixelLayer::new(
        "Hosted awm",
        &event_loop,
        desktop_size,
    )));

    let layer_as_trait_object = Rc::new(layer.get_slice(Rect::with_size(desktop_size)));
    let mut desktop = Desktop::new(Rc::clone(&layer_as_trait_object));
    desktop.draw_background();
    // Start off by drawing a blank canvas consisting of the desktop background
    desktop.blit_background();
    desktop.commit_entire_buffer_to_video_memory();

    //let mut capture_file = None;
    let mut capture_file = Some(
        OpenOptions::new()
            .write(true)
            //.append(true)
            .create(true)
            .open(capture_file_path())
            .unwrap(),
    );
    println!("capture_file {:?}", &capture_file.as_ref());

    let seed = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_millis() as u64;
    let mut rng = SmallRng::seed_from_u64(seed);
    for i in 0..2 {
        let window_size = Size::new(
            rng.gen_range(200..desktop_size.width),
            rng.gen_range(200..desktop_size.height - 30),
        );
        /*
        let window_origin = Point::new(
            rng.gen_range(0..desktop_size.width - window_size.width),
            rng.gen_range(0..desktop_size.height - window_size.height),
        );
        */
        desktop.spawn_window(
            &format!("w{i}"),
            &AwmCreateWindow::new(window_size),
            //Some(window_origin),
            None,
            true,
        );
    }

    let scale_factor = 2;
    let mut last_cursor_pos = None;
    let mut is_left_click_down = false;
    let get_mouse_status_byte = |left_click_down| -> i8 {
        let mut out = 0;
        if left_click_down {
            out |= 1 << 0;
        }
        out
    };

    if let Some(capture_file) = &mut capture_file {
        writeln!(capture_file, "[Size]").unwrap();
        writeln!(
            capture_file,
            "{}, {}",
            desktop_size.width, desktop_size.height
        )
        .unwrap();

        writeln!(capture_file, "[Windows]").unwrap();
        // Iterate backwards to reflect the order windows were spawned (as windows are appended to the front)
        for window in desktop.windows.iter().rev() {
            let frame = *window.frame.borrow();
            writeln!(
                capture_file,
                "{}, {}, {}, {}",
                frame.min_x(),
                frame.min_y(),
                frame.size.width,
                frame.size.height - 30
            )
            .unwrap();
        }
    }

    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Poll;
        match event {
            Event::MainEventsCleared => {
                //for _ in (0..1024 * 32) {
                desktop.step_animations();
                desktop.draw_frame();
                //}
                let pixel_buffer = layer.pixel_buffer.borrow_mut();
                pixel_buffer.render().unwrap();
            }
            Event::WindowEvent { event, .. } => {
                match event {
                    WindowEvent::MouseInput { state, button, .. } => {
                        println!("MouseInput {state:?}, button {button:?}");
                        match state {
                            ElementState::Pressed => match button {
                                MouseButton::Left => {
                                    is_left_click_down = true;
                                    if let Some(capture_file) = &mut capture_file {
                                        writeln!(capture_file, "[MouseDown]").unwrap();
                                    }
                                    desktop.handle_mouse_absolute_update(
                                        None,
                                        None,
                                        get_mouse_status_byte(is_left_click_down),
                                    );
                                }
                                _ => (),
                            },
                            ElementState::Released => match button {
                                MouseButton::Left => {
                                    is_left_click_down = false;
                                    if let Some(capture_file) = &mut capture_file {
                                        writeln!(capture_file, "[MouseUp]").unwrap();
                                    }
                                    desktop.handle_mouse_absolute_update(
                                        None,
                                        None,
                                        get_mouse_status_byte(is_left_click_down),
                                    );
                                }
                                _ => (),
                            },
                            _ => (),
                        }
                    }
                    WindowEvent::CursorMoved { position, .. } => {
                        let mouse_pos = Point::new(
                            (position.x as isize) / scale_factor,
                            (position.y as isize) / scale_factor,
                        );
                        if let None = last_cursor_pos {
                            desktop.set_cursor_pos(mouse_pos);
                            last_cursor_pos = Some(mouse_pos);
                            if let Some(capture_file) = &mut capture_file {
                                writeln!(capture_file, "[SetMousePos]").unwrap();
                                writeln!(capture_file, "{}, {}", mouse_pos.x, mouse_pos.y).unwrap();
                            }
                        }
                        //let rel_x = mouse_pos.x - last_cursor_pos.unwrap().x;
                        //let rel_y = mouse_pos.y - last_cursor_pos.unwrap().y;
                        //println!("Synthetic mouse event {rel_x:?}, {rel_y:?}");
                        desktop.handle_mouse_absolute_update(
                            Some(mouse_pos),
                            None,
                            get_mouse_status_byte(is_left_click_down),
                        );

                        last_cursor_pos = Some(mouse_pos);
                        if let Some(capture_file) = &mut capture_file {
                            writeln!(
                                capture_file,
                                "[MouseMoved]\n{}, {}",
                                mouse_pos.x, mouse_pos.y
                            )
                            .unwrap();
                            //writeln!(capture_file, "{} {}", rel_x as i8, rel_y as i8).unwrap();
                            //writeln!(capture_file, "{}, {}", mouse_pos.x, mouse_pos.y);
                        }
                    }
                    WindowEvent::CursorLeft { .. } => {
                        last_cursor_pos = None;
                    }
                    WindowEvent::KeyboardInput { input, .. } => {
                        if let Some(key_code) = input.virtual_keycode {
                            match key_code {
                                VirtualKeyCode::A => {
                                    //w1.render_remote_layer();
                                    /*
                                    desktop.handle_window_updated_title(
                                        "Window 0",
                                        &AwmWindowUpdateTitle::new("New Title"),
                                    );

                                     */
                                }
                                //VirtualKeyCode::B => w2.render_remote_layer(),
                                //VirtualKeyCode::C => w3.render_remote_layer(),
                                VirtualKeyCode::D => desktop.test(),
                                VirtualKeyCode::E => {
                                    if input.state == ElementState::Released {
                                        match desktop.render_strategy {
                                            RenderStrategy::TreeWalk => {
                                                println!("Switching to compositing");
                                                desktop.render_strategy = RenderStrategy::Composite;
                                            }
                                            RenderStrategy::Composite => {
                                                println!("Switching to tree walking");
                                                desktop.render_strategy = RenderStrategy::TreeWalk;
                                            }
                                        }
                                    }
                                }
                                _ => (),
                            }
                        }
                    }
                    _ => {}
                }
                /*
                desktop.draw_frame();
                let pixel_buffer = layer.pixel_buffer.borrow();
                pixel_buffer.render();
                */
            }
            _ => {}
        }
    });

    Ok(())
}

fn capture_file_path() -> String {
    env::current_dir()
        .unwrap()
        .join("capture.txt")
        .to_str()
        .unwrap()
        .to_string()
    /*
    .ancestors()
    .nth(2)
    .ok_or(io::Error::new(
        io::ErrorKind::NotFound,
        "Failed to find parent",
    ))?
    .join("axle-sysroot");
     */
}

fn parse_isize_vec(line: &str) -> Vec<isize> {
    //println!("parse_isize_vec {line}");
    let components: Vec<&str> = line.split(", ").collect();
    components
        .into_iter()
        .map(|component| isize::from_str_radix(component, 10).unwrap())
        .collect()
}

fn parse_point(line: &str) -> Point {
    let parts = parse_isize_vec(line);
    Point::new(parts[0], parts[1])
}

fn parse_size(line: &str) -> Size {
    //println!("parse_size {line}");
    let components: Vec<&str> = line.split(", ").collect();
    Size::new(
        isize::from_str_radix(components[0], 10).unwrap(),
        isize::from_str_radix(components[1], 10).unwrap(),
    )
}

fn parse_size_space(line: &str) -> Size {
    println!("parse_size_space {line}");
    let components: Vec<&str> = line.split(" ").collect();
    Size::new(
        isize::from_str_radix(components[0], 10).unwrap(),
        isize::from_str_radix(components[1], 10).unwrap(),
    )
}

fn get_mouse_status_byte(left_click_down: bool) -> i8 {
    let mut out = 0;
    if left_click_down {
        out |= 1 << 0;
    }
    out
}

fn replay_capture() {
    let data = Rc::new(
        fs::read_to_string(capture_file_path())
            .expect("Unable to read file")
            .clone(),
    );
    let mut line_iter = data.split('\n').into_iter().enumerate().peekable();

    assert_eq!(line_iter.next().unwrap().1, "[Size]");

    let desktop_size = parse_size(line_iter.next().unwrap().1);
    println!("Got desktop size {desktop_size}");
    let mut desktop = get_desktop_with_size(desktop_size);

    desktop.draw_background();
    // Start off by drawing a blank canvas consisting of the desktop background
    desktop.blit_background();
    desktop.commit_entire_buffer_to_video_memory();

    assert_eq!(line_iter.next().unwrap().1, "[Windows]");
    let mut window_counter = 0;
    loop {
        let peek = line_iter.peek().unwrap().1;
        if peek.starts_with('[') {
            break;
        }
        let line = line_iter.next().unwrap().1;
        let window_frame = {
            let components: Vec<&str> = line.split(", ").collect();
            Rect::from_parts(
                Point::new(
                    isize::from_str_radix(components[0], 10).unwrap(),
                    isize::from_str_radix(components[1], 10).unwrap(),
                ),
                Size::new(
                    isize::from_str_radix(components[2], 10).unwrap(),
                    isize::from_str_radix(components[3], 10).unwrap(),
                ),
            )
        };
        println!("Got window frame {window_frame}");
        desktop.spawn_window(
            &format!("win{window_counter}"),
            &AwmCreateWindow::new(window_frame.size),
            Some(window_frame.origin),
            true,
        );
        window_counter += 1;
    }
    desktop.draw_frame();

    let mut last_mouse_pos = Point::zero();
    let mut is_left_click_down = false;
    let mut event_count = 0;
    while let Some((line_num, line)) = line_iter.next() {
        if line == "[MouseMoved]" {
            /*
            //let rel_movement = parse_size_space(line_iter.next().unwrap());
            last_mouse_pos =
                last_mouse_pos + Point::new(rel_movement.width, rel_movement.height);
             */
            let new_mouse_pos = parse_point(line_iter.next().unwrap().1);
            last_mouse_pos = new_mouse_pos;
            desktop.handle_mouse_absolute_update(
                Some(new_mouse_pos),
                None,
                get_mouse_status_byte(is_left_click_down),
            );
        } else if line == "[MouseDown]" {
            is_left_click_down = true;
            desktop.handle_mouse_absolute_update(
                None,
                None,
                get_mouse_status_byte(is_left_click_down),
            );
        } else if line == "[MouseUp]" {
            is_left_click_down = false;
            desktop.handle_mouse_absolute_update(
                None,
                None,
                get_mouse_status_byte(is_left_click_down),
            );
        } else if line == "[SetMousePos]" {
            last_mouse_pos = parse_point(line_iter.next().unwrap().1);
        } else if line == "\n" || line.len() == 0 {
        } else {
            panic!("Unhandled line #{line_num}: {line}");
        }
        desktop.draw_frame();
        event_count += 1;

        /*
        let desktop_size = desktop.desktop_frame.size;
        let img: RgbImage = ImageBuffer::new(desktop_size.width as u32, desktop_size.height as u32);
        let desktop_slice = desktop
            .video_memory_layer
            .get_slice(Rect::with_size(desktop_size));
        let mut pixel_data = desktop_slice.pixel_data();
        let (prefix, pixel_data_u32, suffix) = unsafe { pixel_data.align_to_mut::<u32>() };
        // Ensure the slice was exactly u32-aligned
        assert_eq!(prefix.len(), 0);
        assert_eq!(suffix.len(), 0);
        let mut pixels: Vec<Rgba<u32>> = pixel_data_u32
            .into_iter()
            //.map(|word| Rgba([*word >> 24, *word >> 16, *word >> 8, 0xff]))
            .map(|word| Rgba([*word >> 24, *word >> 16, *word >> 8, 0xff]))
            .collect();
        save_buffer_with_format(
            format!("./test_capture2/frame{event_count}.jpg"),
            &pixel_data,
            desktop_size.width as u32,
            desktop_size.height as u32,
            image::ColorType::Rgba8,
            image::ImageFormat::Jpeg,
        )
            .unwrap();
        */

        /*
        let mut img = ImageBuffer::from_fn(
            desktop_size.width as u32,
            desktop_size.height as u32,
            |x, y| {
                let px = desktop_slice.getpixel(Point::new(x as isize, y as isize));
                Rgba([px.r, px.g, px.b, 0xff])
            },
        );
        */

        /*
        let mut pixels = vec![];
        let mut iter = pixel_data.into_iter().peekable();
        loop {
            let r = iter.next().unwrap();
            let g = iter.next().unwrap();
            let b = iter.next().unwrap();
            pixels.append(&mut [r, g, b, 0xff]);
            if iter.peek().is_none() {
                break;
            }
        }
         */

        /*
        let mut img: ImageBuffer<Rgba<u32>, Vec<_>> = ImageBuffer::from_vec(
            desktop_size.width as u32,
            desktop_size.height as u32,
            pixels,
        )
        .unwrap();
        img.save(format!("./test_capture2/frame{event_count}.png"));

         */
    }

    /*
    let mut gif_file = OpenOptions::new()
        .write(true)
        .create(true)
        .open("./test.gif")
        .unwrap();
    let out = GifEncoder::new(gif_file);
    */
}

fn get_desktop_with_size(screen_size: Size) -> Desktop {
    let mut vmem = SingleFramebufferLayer::new(screen_size);
    let layer_as_trait_object = Rc::new(vmem.get_full_slice());
    Desktop::new(Rc::clone(&layer_as_trait_object))
}
