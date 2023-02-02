use agx_definitions::{
    Color, Drawable, LikeLayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
    StrokeThickness,
};
use pixels::{Pixels, SurfaceTexture};
use std::rc::{Rc, Weak};
use std::{env, error, fs, io};
use ttf_renderer::parse;
use winit::dpi::LogicalSize;
use winit::event::Event;
use winit::event_loop::{ControlFlow, EventLoop};
use winit::window::WindowBuilder;

pub fn main() -> Result<(), Box<dyn error::Error>> {
    let event_loop = EventLoop::new();

    let window_size = Size::new(600, 600);
    let window = {
        let size = LogicalSize::new(window_size.width as f64, window_size.height as f64);
        WindowBuilder::new()
            .with_title("TTF Viewer")
            .with_inner_size(size)
            .with_min_inner_size(size)
            .with_visible(true)
            .with_resizable(false)
            //.with_position(LogicalPosition::new(100, 100))
            .build(&event_loop)
            .unwrap()
    };

    let mut pixels = {
        let window_size = window.inner_size();
        let surface_texture = SurfaceTexture::new(window_size.width, window_size.height, &window);
        Pixels::new(
            window_size.width as _,
            window_size.height as _,
            surface_texture,
        )
        .unwrap()
    };
    pixels.render().unwrap();

    let font_data = std::fs::read("/Users/philliptennen/Downloads/TestFont.ttf").unwrap();
    parse(&font_data);

    /*
    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Poll;

        if let Event::RedrawRequested(_) = event {
            let frame = pixels.get_frame_mut();
            for pixel in frame.chunks_exact_mut(4) {
                pixel.copy_from_slice(&[0x48, 0xb2, 0xe8, 0xff]);
            }
            pixels.render();
        }
    });
    */
    Ok(())
}
