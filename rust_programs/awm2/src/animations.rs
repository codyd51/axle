use crate::desktop::{Desktop, DesktopElement};
use crate::println;
use crate::shortcuts::DesktopShortcut;
use crate::utils::get_timestamp;
use crate::window::Window;
use agx_definitions::{Point, Rect, Size};
use alloc::rc::Rc;
use alloc::vec;
use alloc::vec::Vec;

fn lerp(a: f64, b: f64, percent: f64) -> f64 {
    a + (percent * (b - a))
}

fn interpolate_frame(from: Rect, to: Rect, percent: f64) -> Rect {
    Rect::from_parts(
        Point::new(
            lerp(from.min_x() as f64, to.min_x() as f64, percent) as isize,
            lerp(from.min_y() as f64, to.min_y() as f64, percent) as isize,
        ),
        Size::new(
            lerp(from.width() as f64, to.width() as f64, percent) as isize,
            lerp(from.height() as f64, to.height() as f64, percent) as isize,
        ),
    )
}

pub struct WindowTransformAnimationParams {
    start_time: usize,
    end_time: usize,
    pub window: Rc<Window>,
    pub duration_ms: usize,
    pub frame_from: Rect,
    pub frame_to: Rect,
}

impl WindowTransformAnimationParams {
    fn new(window: &Rc<Window>, duration_ms: usize, frame_from: Rect, frame_to: Rect) -> Self {
        let start_time = get_timestamp() as usize;
        Self {
            start_time,
            end_time: start_time + duration_ms,
            window: Rc::clone(window),
            duration_ms,
            frame_from,
            frame_to,
        }
    }

    pub fn open(
        desktop_size: Size,
        window: &Rc<Window>,
        duration_ms: usize,
        frame_from: Option<Rect>,
        frame_to: Rect,
    ) -> Self {
        let from_size = Size::new(desktop_size.width / 10, desktop_size.height / 10);
        let frame_from = frame_from.unwrap_or(Rect::from_parts(
            Point::new(
                ((desktop_size.width as f64 / 2.0) - (from_size.width as f64 / 2.0)) as isize,
                desktop_size.height - from_size.height,
            ),
            from_size,
        ));
        Self::new(&window, duration_ms, frame_from, frame_to)
    }

    pub fn close(desktop_size: Size, window: &Rc<Window>, duration_ms: usize) -> Self {
        let final_size = Size::new(
            (desktop_size.width as f64 / 10.0) as isize,
            (desktop_size.height as f64 / 10.0) as isize,
        );
        let final_frame = Rect::from_parts(
            Point::new(
                ((desktop_size.width as f64 / 2.0) - (final_size.width as f64 / 2.0)) as isize,
                desktop_size.height - final_size.height,
            ),
            final_size,
        );
        Self::new(&window, duration_ms, window.frame(), final_frame)
    }

    pub fn minimize(window: &Rc<Window>, duration_ms: usize, frame_to: Rect) -> Self {
        Self::new(&window, duration_ms, window.frame(), frame_to)
    }

    pub fn unminimize(window: &Rc<Window>, duration_ms: usize) -> Self {
        Self::new(
            &window,
            duration_ms,
            window.frame(),
            window.unminimized_frame().unwrap(),
        )
    }
}

pub struct ShortcutSnapAnimationParams {
    start_time: usize,
    end_time: usize,
    pub shortcut: Rc<DesktopShortcut>,
    duration_ms: usize,
    frame_from: Rect,
    frame_to: Rect,
}

impl ShortcutSnapAnimationParams {
    pub fn new(shortcut: &Rc<DesktopShortcut>, frame_to: Rect, duration_ms: usize) -> Self {
        let start_time = get_timestamp() as usize;
        Self {
            start_time,
            end_time: start_time + duration_ms,
            shortcut: Rc::clone(shortcut),
            duration_ms,
            frame_from: shortcut.frame(),
            frame_to,
        }
    }
}

pub struct AnimationDamage {
    pub area_to_recompute_drawable_regions: Rect,
    pub rects_needing_composite: Vec<Rect>,
}

impl AnimationDamage {
    pub fn new(
        area_to_recompute_drawable_regions: Rect,
        rects_needing_composite: Vec<Rect>,
    ) -> Self {
        Self {
            area_to_recompute_drawable_regions,
            rects_needing_composite,
        }
    }
}

pub enum Animation {
    WindowOpen(WindowTransformAnimationParams),
    WindowClose(WindowTransformAnimationParams),
    WindowMinimize(WindowTransformAnimationParams),
    WindowUnminimize(WindowTransformAnimationParams),
    ShortcutSnap(ShortcutSnapAnimationParams),
}

impl Animation {
    pub fn start(&self) {
        match self {
            Animation::WindowOpen(params)
            | Animation::WindowClose(params)
            | Animation::WindowUnminimize(params) => {
                params.window.set_frame(params.frame_from);
            }
            Animation::WindowMinimize(params) => {
                params.window.set_frame(params.frame_from);
                params.window.set_unminimized_frame(Some(params.frame_from));
            }
            Animation::ShortcutSnap(params) => {
                params.shortcut.set_frame(params.frame_from);
            }
        }
    }

    pub fn step(&self, now: u64) -> AnimationDamage {
        match self {
            Animation::WindowOpen(params)
            | Animation::WindowClose(params)
            | Animation::WindowMinimize(params)
            | Animation::WindowUnminimize(params) => {
                let update_region = {
                    let mut window_frame = params.window.frame.borrow_mut();
                    let old_frame = *window_frame;
                    let elapsed = now - (params.start_time as u64);
                    let percent = f64::min(1.0, elapsed as f64 / params.duration_ms as f64);
                    let new_frame = interpolate_frame(params.frame_from, params.frame_to, percent);
                    *window_frame = new_frame;
                    old_frame.union(new_frame)
                };
                params.window.redraw_title_bar();
                AnimationDamage::new(update_region, vec![update_region])
            }
            Animation::ShortcutSnap(params) => {
                let update_region = {
                    let old_frame = params.shortcut.frame();
                    let elapsed = now - (params.start_time as u64);
                    let percent = f64::min(1.0, elapsed as f64 / params.duration_ms as f64);
                    let new_frame = interpolate_frame(params.frame_from, params.frame_to, percent);
                    params.shortcut.set_frame(new_frame);
                    old_frame.union(new_frame)
                };
                AnimationDamage::new(update_region, vec![update_region])
            }
        }
    }

    pub fn is_complete(&self, now: u64) -> bool {
        match self {
            Animation::WindowOpen(params)
            | Animation::WindowClose(params)
            | Animation::WindowMinimize(params)
            | Animation::WindowUnminimize(params) => now as usize >= params.end_time,
            Animation::ShortcutSnap(params) => now as usize >= params.end_time,
        }
    }

    pub fn finish(&self) {
        match self {
            Animation::WindowOpen(_)
            | Animation::WindowClose(_)
            | Animation::WindowMinimize(_)
            | Animation::ShortcutSnap(_) => {}
            Animation::WindowUnminimize(params) => params.window.set_unminimized_frame(None),
        }
    }
}
