use agx_definitions::{Color, Layer, Point, SingleFramebufferLayer, Size};
use core::cmp::max;

use num_traits::Float;

fn transcolor(color1: Color, color2: Color, d: f64) -> Color {
    let mut d = f64::max(d, 0_f64);
    d = f64::min(d, 1_f64);
    Color::new(
        ((color1.b as f64) * (1.0 - d) + (color2.b as f64 * d)) as u8,
        ((color1.g as f64) * (1.0 - d) + (color2.r as f64 * d)) as u8,
        ((color1.r as f64) * (1.0 - d) + (color2.g as f64 * d)) as u8,
    )
}

fn pifdist(x1: isize, y1: isize, x2: isize, y2: isize) -> f64 {
    let x = (x1 - x2) as f64;
    let y = (y1 - y2) as f64;
    f64::sqrt((x * x) + (y * y))
}

pub fn draw_radial_gradient(
    onto: &SingleFramebufferLayer,
    gradient_size: Size,
    color1: Color,
    color2: Color,
    x1: isize,
    y1: isize,
    radius: f64,
) {
    let mut x_step = ((gradient_size.width as f64) / 200.0) as isize;
    let mut y_step = ((gradient_size.height as f64) / 200.0) as isize;
    x_step = max(x_step, 1);
    y_step = max(y_step, 1);
    // TODO(PT): Why does this overwrite bounds without the sub 5?
    for y in (0..gradient_size.height - 5).step_by(y_step as usize) {
        for x in (0..gradient_size.width).step_by(x_step as usize) {
            let color = transcolor(
                color1,
                color2,
                pifdist(x1, y1, x as isize, y as isize) / radius,
            );
            for i in 0..x_step {
                for j in 0..y_step {
                    onto.putpixel(&Point::new(x + i, y + j), color);
                }
            }
        }
    }
}
