use crate::effects::draw_radial_gradient;

use crate::println;
use agx_definitions::{
    Color, Layer, LikeLayerSlice, Point, Rect, RectInsets, SingleFramebufferLayer, Size,
    StrokeThickness,
};
use alloc::boxed::Box;
use alloc::format;
use alloc::rc::Rc;
use alloc::string::{String, ToString};
use alloc::vec;
use alloc::vec::Vec;
use awm_messages::{AwmCreateWindow, AwmWindowResized, AwmWindowUpdateTitle};
use axle_rt::core_commands::AmcSharedMemoryCreateRequest;
use core::cell::RefCell;
use core::cmp::{max, min};
use core::fmt::{Display, Formatter};
use mouse_driver_messages::MousePacket;

use file_manager_messages::str_from_u8_nul_utf8_unchecked;
use kb_driver_messages::{KeyEventType, KeyboardPacket};
use lazy_static::lazy_static;
use rand::rngs::SmallRng;
use rand::RngCore;
use rand::{Rng, SeedableRng};

#[cfg(target_os = "axle")]
pub extern crate libc;
#[cfg(target_os = "axle")]
mod conditional_imports {
    pub use awm_messages::AwmCreateWindowResponse;
    pub use axle_rt::amc_message_send;
}
#[cfg(not(target_os = "axle"))]
mod conditional_imports {
    pub use std::time::{SystemTime, UNIX_EPOCH};
}

use crate::desktop::conditional_imports::*;

fn random_color() -> Color {
    #[cfg(target_os = "axle")]
    let seed = unsafe { libc::ms_since_boot() } as u64;
    #[cfg(not(target_os = "axle"))]
    let seed = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_millis() as u64;

    let mut rng = SmallRng::seed_from_u64(seed);
    Color::new(rng.gen(), rng.gen(), rng.gen())
}

/// A persistent UI element on the desktop that occludes other elements
/// Roughly: a window, a desktop shortcut, etc
trait DesktopElement {
    fn frame(&self) -> Rect;
    fn name(&self) -> String;
    fn drawable_rects(&self) -> Vec<Rect>;
    fn set_drawable_rects(&self, drawable_rects: Vec<Rect>);
    fn get_slice(&self) -> Box<dyn LikeLayerSlice>;
}

pub struct Window {
    pub frame: RefCell<Rect>,
    drawable_rects: RefCell<Vec<Rect>>,
    pub owner_service: String,
    layer: RefCell<SingleFramebufferLayer>,
    pub content_layer: RefCell<SingleFramebufferLayer>,
    title: RefCell<Option<String>>,
}

impl Window {
    const TITLE_BAR_HEIGHT: usize = 30;

    fn new(owner_service: &str, frame: Rect, window_layer: SingleFramebufferLayer) -> Self {
        let total_size = Self::total_size_for_content_size(window_layer.size());
        Self {
            frame: RefCell::new(frame),
            drawable_rects: RefCell::new(vec![]),
            owner_service: owner_service.to_string(),
            layer: RefCell::new(SingleFramebufferLayer::new(total_size)),
            content_layer: RefCell::new(window_layer),
            title: RefCell::new(None),
        }
    }

    fn set_frame(&self, frame: Rect) {
        *self.frame.borrow_mut() = frame;
    }

    fn set_title(&self, new_title: &str) {
        *self.title.borrow_mut() = Some(new_title.to_string())
    }

    fn is_point_within_resize_inset(&self, local_point: Point) -> bool {
        let grabber_inset = 8;
        let content_frame_past_inset = self
            .content_frame()
            .inset_by_insets(RectInsets::uniform(grabber_inset));
        !content_frame_past_inset.contains(local_point)
    }

    fn title_bar_frame(&self) -> Rect {
        Rect::with_size(Size::new(
            self.frame().width(),
            Self::TITLE_BAR_HEIGHT as isize,
        ))
    }

    fn is_point_within_title_bar(&self, local_point: Point) -> bool {
        self.title_bar_frame()
            .replace_origin(Point::zero())
            .contains(local_point)
    }

    fn content_frame(&self) -> Rect {
        Rect::from_parts(
            Point::new(0, Self::TITLE_BAR_HEIGHT as isize),
            Size::new(
                self.frame().width(),
                self.frame().height() - (Self::TITLE_BAR_HEIGHT as isize),
            ),
        )
    }

    fn redraw_title_bar(&self) -> Rect {
        let title_bar_frame = self.title_bar_frame();
        let title_bar_slice = self.layer.borrow_mut().get_slice(title_bar_frame);
        title_bar_slice.fill(Color::white());

        // Draw the window title
        let font_size = Size::new(8, 12);
        let maybe_window_title = self.title.borrow();
        let window_title = maybe_window_title.as_ref().unwrap_or(&self.owner_service);
        //println!("Found title {window_title}");
        let title_len = window_title.len();
        let mut cursor = title_bar_frame.midpoint()
            - Point::new(
                (((font_size.width * (title_len as isize)) as f64) / 2.0) as isize,
                (((font_size.height as f64) / 2.0) - 1.0) as isize,
            );
        let title_text_color = Color::new(50, 50, 50);
        for ch in window_title.chars() {
            title_bar_slice.draw_char(ch, cursor, title_text_color, font_size);
            cursor.x += font_size.width;
        }

        title_bar_frame.replace_origin(self.frame.borrow().origin)
    }

    pub fn render_remote_layer(&self) {
        let src = self.content_layer.borrow_mut().get_full_slice();
        let mut dst = self.layer.borrow_mut().get_slice(Rect::from_parts(
            Point::new(0, Self::TITLE_BAR_HEIGHT as isize),
            src.frame().size,
        ));
        dst.blit2(&src);
    }

    fn total_size_for_content_size(content_size: Size) -> Size {
        Size::new(
            content_size.width,
            content_size.height + Self::TITLE_BAR_HEIGHT as isize,
        )
    }
}

impl Display for Window {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "<Window \"{}\" @ {}>",
            self.owner_service,
            self.frame.borrow()
        )
    }
}

impl DesktopElement for Window {
    fn frame(&self) -> Rect {
        *self.frame.borrow()
    }

    fn name(&self) -> String {
        self.owner_service.to_string()
    }

    fn drawable_rects(&self) -> Vec<Rect> {
        self.drawable_rects.borrow().clone()
    }

    fn set_drawable_rects(&self, drawable_rects: Vec<Rect>) {
        *self.drawable_rects.borrow_mut() = drawable_rects
    }

    fn get_slice(&self) -> Box<dyn LikeLayerSlice> {
        self.layer
            .borrow_mut()
            .get_slice(Rect::with_size(self.frame().size))
    }
}

#[derive(Debug)]
enum MouseStateChange {
    LeftClickBegan,
    LeftClickEnded,
    Moved(Point, Point),
}

struct MouseState {
    pos: Point,
    desktop_size: Size,
    size: Size,
    left_click_down: bool,
}

impl MouseState {
    fn new(pos: Point, desktop_size: Size) -> Self {
        Self {
            pos,
            size: Size::new(14, 14),
            desktop_size,
            left_click_down: false,
        }
    }

    fn compute_state_changes(
        &mut self,
        new_pos: Option<Point>,
        status: i8,
    ) -> Vec<MouseStateChange> {
        let mut out = vec![];

        if let Some(new_pos) = new_pos {
            let old_pos = self.pos;

            self.pos = new_pos;

            // Bind mouse to screen dimensions
            self.pos.x = max(self.pos.x, 0);
            self.pos.y = max(self.pos.y, 0);
            self.pos.x = min(self.pos.x, self.desktop_size.width - 4);
            self.pos.y = min(self.pos.y, self.desktop_size.height - 10);

            let delta = new_pos - old_pos;
            if delta.x != 0 || delta.y != 0 {
                out.push(MouseStateChange::Moved(self.pos, delta));
            }
        }

        // Is the left button clicked?
        if status & (1 << 0) != 0 {
            // Were we already tracking a left click?
            if !self.left_click_down {
                self.left_click_down = true;
                out.push(MouseStateChange::LeftClickBegan);
            }
        } else {
            // Did we just release a left click?
            if self.left_click_down {
                self.left_click_down = false;
                out.push(MouseStateChange::LeftClickEnded);
            }
        }

        out
    }

    fn frame(&self) -> Rect {
        Rect::from_parts(self.pos, self.size)
    }
}

struct CompositorState {
    /// While compositing the frame, awm will determine what individual elements
    /// must be redrawn to composite these rectangles.
    /// These may include portions of windows, the desktop background, etc.
    rects_to_fully_redraw: Vec<Rect>,
    /// Entire elements that must be composited on the next frame
    elements_to_composite: RefCell<Vec<Rc<dyn DesktopElement>>>,
    // Every desktop element the compositor knows about
    elements: Vec<Rc<dyn DesktopElement>>,

    extra_draws: RefCell<Vec<(Rc<dyn DesktopElement>, Rect)>>,
    extra_background_draws: Vec<Rect>,
}

impl CompositorState {
    fn new() -> Self {
        Self {
            rects_to_fully_redraw: vec![],
            elements_to_composite: RefCell::new(vec![]),
            elements: vec![],
            extra_draws: RefCell::new(vec![]),
            extra_background_draws: vec![],
        }
    }

    fn queue_full_redraw(&mut self, in_rect: Rect) {
        self.rects_to_fully_redraw.push(in_rect)
    }

    fn queue_composite(&self, element: Rc<dyn DesktopElement>) {
        self.elements_to_composite.borrow_mut().push(element)
    }

    fn track_element(&mut self, element: Rc<dyn DesktopElement>) {
        self.elements.push(element)
    }

    fn queue_extra_draw(&self, element: Rc<dyn DesktopElement>, r: Rect) {
        self.extra_draws.borrow_mut().push((element, r));
    }

    fn queue_extra_background_draw(&mut self, r: Rect) {
        self.extra_background_draws.push(r);
    }
}

enum MouseInteractionState {
    BackgroundHover,
    WindowHover(Rc<Window>),
    HintingWindowDrag(Rc<Window>),
    HintingWindowResize(Rc<Window>),
    PerformingWindowDrag(Rc<Window>),
    PerformingWindowResize(Rc<Window>),
}

pub enum RenderStrategy {
    TreeWalk,
    Composite,
}

pub struct Desktop {
    desktop_frame: Rect,
    // The final video memory
    video_memory_layer: Rc<Box<dyn LikeLayerSlice>>,
    screen_buffer_layer: Box<SingleFramebufferLayer>,
    desktop_background_layer: Box<SingleFramebufferLayer>,
    // Index 0 is the foremost window
    pub windows: Vec<Rc<Window>>,
    mouse_state: MouseState,
    mouse_interaction_state: MouseInteractionState,
    compositor_state: CompositorState,
    pub render_strategy: RenderStrategy,
    rng: SmallRng,
}

impl Desktop {
    pub fn new(video_memory_layer: Rc<Box<dyn LikeLayerSlice>>) -> Self {
        let desktop_frame = Rect::with_size(video_memory_layer.frame().size);
        video_memory_layer.fill_rect(desktop_frame, Color::yellow(), StrokeThickness::Filled);

        let desktop_background_layer = Box::new(SingleFramebufferLayer::new(desktop_frame.size));
        let screen_buffer_layer = Box::new(SingleFramebufferLayer::new(desktop_frame.size));

        // Start the mouse in the middle of the screen
        let initial_mouse_pos = Point::new(desktop_frame.mid_x(), desktop_frame.mid_y());

        #[cfg(target_os = "axle")]
        let seed = unsafe { libc::ms_since_boot() } as u64;
        #[cfg(not(target_os = "axle"))]
        let seed = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis() as u64;

        Self {
            desktop_frame: Rect::with_size(video_memory_layer.frame().size),
            video_memory_layer,
            screen_buffer_layer,
            desktop_background_layer,
            windows: vec![],
            mouse_state: MouseState::new(initial_mouse_pos, desktop_frame.size),
            compositor_state: CompositorState::new(),
            render_strategy: RenderStrategy::Composite,
            mouse_interaction_state: MouseInteractionState::BackgroundHover,
            rng: SmallRng::seed_from_u64(seed),
        }
    }

    pub fn draw_background(&self) {
        let gradient_outer = Color::new(255, 120, 120);
        let gradient_inner = Color::new(200, 120, 120);
        draw_radial_gradient(
            &self.desktop_background_layer,
            self.desktop_background_layer.size(),
            gradient_inner,
            gradient_outer,
            (self.desktop_background_layer.width() as f64 / 2.0) as isize,
            (self.desktop_background_layer.height() as f64 / 2.0) as isize,
            self.desktop_background_layer.height() as f64 * 0.65,
        );
    }

    fn draw_mouse(&mut self) -> Rect {
        let mut mouse_color = {
            match self.mouse_interaction_state {
                MouseInteractionState::BackgroundHover | MouseInteractionState::WindowHover(_) => {
                    Color::green()
                }
                MouseInteractionState::HintingWindowDrag(_) => Color::new(121, 160, 217),
                MouseInteractionState::HintingWindowResize(_) => Color::new(212, 119, 201),
                MouseInteractionState::PerformingWindowDrag(_) => Color::new(30, 65, 217),
                MouseInteractionState::PerformingWindowResize(_) => Color::new(207, 25, 185),
            }
        };
        if cfg!(not(target_os = "axle")) {
            //println!("Swapping order");
            //mouse_color = mouse_color.swap_order();
        }

        let mouse_rect = self.mouse_state.frame();
        let onto = self.screen_buffer_layer.get_slice(mouse_rect);
        onto.fill(Color::black());
        onto.fill_rect(
            Rect::with_size(mouse_rect.size).apply_insets(RectInsets::new(2, 2, 2, 2)),
            mouse_color,
            StrokeThickness::Filled,
        );
        mouse_rect
    }

    pub fn blit_background(&mut self) {
        self.screen_buffer_layer
            .copy_from(&self.desktop_background_layer);
    }

    /// Very expensive and should be used sparingly
    pub fn commit_entire_buffer_to_video_memory(&mut self) {
        Self::copy_rect(
            &mut *self.screen_buffer_layer.get_slice(self.desktop_frame),
            &mut *self.video_memory_layer.get_slice(self.desktop_frame),
            self.desktop_frame,
        );
    }

    fn compute_extra_draws_from_total_update_rects(&mut self) {
        // Compute what to draw for the rects in which we need to do a full desktop walk
        //println!("compute_extra_draws_from_total_update_rects()");
        for full_redraw_rect in self.compositor_state.rects_to_fully_redraw.clone().iter() {
            //println!("\tProcessing full redraw rect {full_redraw_rect}");
            // Keep track of what we've redrawn using desktop elements
            // This will hold what we still need to draw (eventually with the desktop background)
            let mut undrawn_areas = vec![*full_redraw_rect];

            // Handle the parts of the dirty region that are obscured by desktop views
            'outer: for elem in self.compositor_state.elements.iter() {
                if !elem.frame().intersects_with(*full_redraw_rect) {
                    continue;
                }

                for visible_region in elem.drawable_rects().iter() {
                    if !visible_region.intersects_with(*full_redraw_rect) {
                        continue;
                    }
                    if visible_region.encloses(*full_redraw_rect) {
                        // The entire rect should be redrawn from this window
                        self.compositor_state
                            .queue_extra_draw(Rc::clone(&elem), *full_redraw_rect);
                        // And subtract the area of the rect from the region to update
                        //unobscured_region = update_occlusions(unobscured_region, r);
                        undrawn_areas = Self::update_occlusions(undrawn_areas, *full_redraw_rect);
                        //println!("Fully enclosed in {}", elem.name());
                        break 'outer;
                    } else {
                        // This element needs to redraw the intersection of its visible rect and the update rect
                        let intersection = visible_region
                            .area_overlapping_with(*full_redraw_rect)
                            .unwrap();
                        self.compositor_state
                            .queue_extra_draw(Rc::clone(&elem), intersection);

                        /*
                        println!(
                            "Partially enclosed in {}'s drawable rect of {}",
                            elem.name(),
                            visible_region
                        );
                        */
                        //println!("\tIntersection {intersection}");
                        // And subtract the area of the rect from the region to update
                        //println!("Updating unobscured area from {undrawn_areas:?}");
                        undrawn_areas = Self::update_occlusions(undrawn_areas, intersection);
                        //println!("\tUpdated to {undrawn_areas:?}");
                    }
                }
            }

            for undrawn_area in undrawn_areas.iter() {
                //println!("\tQueuing extra background draw {undrawn_area}");
                self.compositor_state
                    .queue_extra_background_draw(*undrawn_area)
            }
        }
    }

    fn update_occlusions(free_areas: Vec<Rect>, exclude: Rect) -> Vec<Rect> {
        let mut out = vec![];

        for free_area in free_areas.iter() {
            if !free_area.intersects_with(exclude) {
                out.push(*free_area);
                continue;
            }

            let mut occlusions = free_area.area_excluding_rect(exclude);
            out.append(&mut occlusions);
        }
        out
    }

    pub fn draw_frame(&mut self) {
        match self.render_strategy {
            RenderStrategy::TreeWalk => {
                self.draw_frame_simple();
            }
            RenderStrategy::Composite => {
                self.draw_frame_composited();
            }
        }
    }

    pub fn draw_frame_simple(&mut self) {
        Self::copy_rect(
            &mut *self.desktop_background_layer.get_full_slice(),
            &mut *self
                .screen_buffer_layer
                .get_slice(Rect::with_size(self.desktop_frame.size)),
            self.desktop_frame,
        );
        for elem in self.compositor_state.elements.iter() {
            let src = elem.get_slice();
            let dst = self.screen_buffer_layer.get_slice(elem.frame());
            dst.blit2(&src);
        }
        let mouse_rect = self.draw_mouse();
        self.compositor_state.extra_draws.borrow_mut().drain(..);
        self.compositor_state.rects_to_fully_redraw.drain(..);
        self.compositor_state
            .elements_to_composite
            .borrow_mut()
            .drain(..);

        Self::copy_rect(
            &mut *self.screen_buffer_layer.get_full_slice(),
            &mut *self
                .video_memory_layer
                .get_slice(Rect::with_size(self.desktop_frame.size)),
            self.desktop_frame,
        );
    }

    pub fn draw_frame_composited(&mut self) {
        self.compute_extra_draws_from_total_update_rects();

        // Composite each desktop element that needs to be composited this frame
        for desktop_element in self
            .compositor_state
            .elements_to_composite
            .borrow_mut()
            .drain(..)
        {
            let layer = desktop_element.get_slice();
            let drawable_rects = desktop_element.drawable_rects();
            for drawable_rect in drawable_rects.iter() {
                let origin_in_element_coordinate_space =
                    drawable_rect.origin - desktop_element.frame().origin;
                let drawable_rect_slice = layer.get_slice(Rect::from_parts(
                    origin_in_element_coordinate_space,
                    drawable_rect.size,
                ));
                let dst_slice = self.video_memory_layer.get_slice(*drawable_rect);
                dst_slice.blit2(&drawable_rect_slice);
                /*
                self.video_memory_layer.fill_rect(
                    *drawable_rect,
                    random_color(),
                    StrokeThickness::Width(2),
                );
                */
            }
        }

        // Composite specific pieces of desktop elements that were invalidated this frame
        for (desktop_element, screen_rect) in self.compositor_state.extra_draws.borrow_mut().iter()
        {
            let layer = desktop_element.get_slice();
            let local_origin = screen_rect.origin - desktop_element.frame().origin;
            let local_rect = Rect::from_parts(local_origin, screen_rect.size);
            let src_slice = layer.get_slice(local_rect);
            let dst_slice = self.screen_buffer_layer.get_slice(*screen_rect);
            dst_slice.blit2(&src_slice);
        }

        // Copy the bits of the background that we decided we needed to redraw
        for background_copy_rect in self.compositor_state.extra_background_draws.drain(..) {
            //println!("Drawing background rect {background_copy_rect}");
            Self::copy_rect(
                &mut *self.desktop_background_layer.get_slice(self.desktop_frame),
                &mut *self.screen_buffer_layer.get_slice(self.desktop_frame),
                background_copy_rect,
            );
        }

        // Finally, draw the mouse cursor
        let mouse_rect = self.draw_mouse();

        // Now blit the screen buffer to the backing video memory
        // Follow the same steps as above to only copy what's changed
        // And empty queues as we go
        let buffer = &mut *self.screen_buffer_layer.get_slice(self.desktop_frame);
        let vmem = &mut *self.video_memory_layer.get_slice(self.desktop_frame);

        // We don't need to walk each individual view - we can rely on the logic above to have drawn it to the screen buffer
        //self.compositor_state.extra_draws.borrow_mut().drain(..);
        for (_, extra_draw) in self.compositor_state.extra_draws.borrow_mut().drain(..) {
            Self::copy_rect(buffer, vmem, extra_draw);
        }

        for full_redraw_rect in self.compositor_state.rects_to_fully_redraw.drain(..) {
            Self::copy_rect(buffer, vmem, full_redraw_rect);
        }

        Self::copy_rect(buffer, vmem, mouse_rect);
    }

    fn copy_rect(src: &mut dyn LikeLayerSlice, dst: &mut dyn LikeLayerSlice, rect: Rect) {
        let src_slice = src.get_slice(rect);
        let dst_slice = dst.get_slice(rect);
        dst_slice.blit2(&src_slice);
    }

    fn recompute_drawable_regions_in_rect(&mut self, rect: Rect) {
        //println!("recompute_drawable_regions_in_rect({rect})");
        // Iterate backwards (from the furthest back to the foremost)
        for window_idx in (0..self.windows.len()).rev() {
            //println!("\tProcessing idx #{elem_idx}, window {} (a split has {} elems, b split has {} elems)", elem.name(), a.len(), b.len());
            let window = &self.windows[window_idx];
            if !rect.intersects_with(window.frame()) {
                //println!("\t\tDoes not intersect with provided rect, skipping");
                continue;
            }

            let (occluding_windows, _window_and_lower) = self.windows.split_at(window_idx);

            window.set_drawable_rects(vec![window.frame()]);

            for occluding_window in occluding_windows.iter().rev() {
                if !window.frame().intersects_with(occluding_window.frame()) {
                    continue;
                }
                //println!("\t\tOccluding {} by view with frame {}", elem.frame(), occluding_elem.frame());
                // Keep rects that don't intersect with the occluding elem
                let mut new_drawable_rects: Vec<Rect> = window
                    .drawable_rects()
                    .iter()
                    .filter_map(|r| {
                        // If it does not intersect with the occluding element, we want to keep it
                        if !r.intersects_with(occluding_window.frame()) {
                            Some(*r)
                        } else {
                            None
                        }
                    })
                    .collect();
                for rect in window.drawable_rects() {
                    let mut visible_portions = rect.area_excluding_rect(occluding_window.frame());
                    new_drawable_rects.append(&mut visible_portions);
                }
                //println!("\t\tSetting drawable_rects to {:?}", new_drawable_rects);
                window.set_drawable_rects(new_drawable_rects);
            }

            if window.drawable_rects().len() > 0 {
                //println!("\tQueueing composite for {}", window.name());
                self.compositor_state
                    .queue_composite(Rc::clone(window) as Rc<dyn DesktopElement>);
            }
        }
    }

    pub fn spawn_window(
        &mut self,
        source: &str,
        request: &AwmCreateWindow,
        origin: Option<Point>,
    ) -> Rc<Window> {
        let source = source.to_string();
        println!("Creating window of size {:?} for {}", request.size, source);

        let content_size = Size::from(&request.size);
        // The window is larger than the content view to account for decorations
        let window_size = Window::total_size_for_content_size(content_size);
        let new_window_origin = origin.unwrap_or({
            // Place the window in the center of the screen
            let res = self.desktop_frame.size;
            Point::new(
                (res.width / 2) - (window_size.width / 2),
                (res.height / 2) - (window_size.height / 2),
            )
        });

        let desktop_size = self.desktop_frame.size;
        #[cfg(target_os = "axle")]
        let window_layer = {
            // Ask the kernel to set up a shared memory mapping we'll use for the framebuffer
            // The framebuffer will be the screen size to allow window resizing
            let bytes_per_pixel = self.screen_buffer_layer.bytes_per_pixel();
            let shared_memory_size = desktop_size.width * desktop_size.height * bytes_per_pixel;
            println!("Requesting shared memory of size {shared_memory_size} {desktop_size:?}");
            let shared_memory_response =
                AmcSharedMemoryCreateRequest::send(&source.to_string(), shared_memory_size as u32);

            let framebuffer_slice = core::ptr::slice_from_raw_parts_mut(
                shared_memory_response.local_buffer_start as *mut libc::c_void,
                shared_memory_size as usize,
            );
            let framebuffer: &mut [u8] = unsafe { &mut *(framebuffer_slice as *mut [u8]) };
            let window_created_msg = AwmCreateWindowResponse::new(
                desktop_size,
                bytes_per_pixel as u32,
                shared_memory_response.remote_buffer_start,
            );
            println!("Sending response to {source} {source:?}: {window_created_msg:?}");
            amc_message_send(&source, window_created_msg);
            SingleFramebufferLayer::from_framebuffer(
                unsafe { Box::from_raw(framebuffer) },
                bytes_per_pixel,
                desktop_size,
            )
        };
        #[cfg(not(target_os = "axle"))]
        let window_layer = SingleFramebufferLayer::new(desktop_size);

        let window_frame = Rect::from_parts(new_window_origin, window_size);
        let new_window = Rc::new(Window::new(&source, window_frame, window_layer));
        new_window.redraw_title_bar();
        self.windows.insert(0, Rc::clone(&new_window));
        self.compositor_state
            .track_element(Rc::clone(&new_window) as Rc<dyn DesktopElement>);
        self.recompute_drawable_regions_in_rect(window_frame);

        /*
        new_window.render_remote_layer();
        self.compositor_state
            .queue_composite(Rc::clone(&new_window) as Rc<dyn DesktopElement>);
        */

        new_window
    }

    fn window_containing_point(&self, p: Point) -> Option<Rc<Window>> {
        // Iterate from the topmost window to further back ones,
        // so if windows are overlapping the topmost window will receive it
        for window in self.windows.iter() {
            if window.frame().contains(p) {
                return Some(Rc::clone(window));
            }
        }
        return None;
    }

    fn handle_left_click_began(&mut self) {
        if let Some(window_under_mouse) = self.window_containing_point(self.mouse_state.pos) {
            if !Rc::ptr_eq(&self.windows[0], &window_under_mouse) {
                println!(
                    "Moving clicked window to top: {}",
                    window_under_mouse.name()
                );
                self.move_window_to_top(&window_under_mouse);
                self.recompute_drawable_regions_in_rect(window_under_mouse.frame());

                self.mouse_interaction_state =
                    MouseInteractionState::WindowHover(Rc::clone(&window_under_mouse));
                self.handle_mouse_moved(self.mouse_state.pos, Point::zero());
            }
        }

        if let MouseInteractionState::HintingWindowDrag(hover_window) =
            &self.mouse_interaction_state
        {
            self.mouse_interaction_state =
                MouseInteractionState::PerformingWindowDrag(Rc::clone(hover_window));
        } else if let MouseInteractionState::HintingWindowResize(hover_window) =
            &self.mouse_interaction_state
        {
            self.mouse_interaction_state =
                MouseInteractionState::PerformingWindowResize(Rc::clone(&hover_window));
        }
    }

    fn handle_left_click_ended(&mut self) {
        match &self.mouse_interaction_state {
            MouseInteractionState::PerformingWindowDrag(win) => {
                self.mouse_interaction_state =
                    MouseInteractionState::HintingWindowDrag(Rc::clone(&win))
            }
            MouseInteractionState::PerformingWindowResize(win) => {
                println!("Ending window resize");
                self.mouse_interaction_state =
                    MouseInteractionState::HintingWindowResize(Rc::clone(&win))
            }
            _ => {}
        }
    }

    fn handle_mouse_moved(&mut self, new_pos: Point, rel_shift: Point) {
        // Are we in the middle of dragging a window?
        if let MouseInteractionState::PerformingWindowDrag(dragged_window) =
            &self.mouse_interaction_state
        {
            let prev_frame = dragged_window.frame();
            // Bind the window to the screen size
            let new_frame = self.bind_rect_to_screen_size(
                dragged_window
                    .frame()
                    .replace_origin(dragged_window.frame().origin + rel_shift),
            );

            dragged_window.set_frame(new_frame);
            let total_update_region = prev_frame.union(new_frame);
            self.recompute_drawable_regions_in_rect(total_update_region);
            //self.compositor_state.queue_full_redraw(total_update_region);
            for diff_rect in prev_frame.area_excluding_rect(new_frame) {
                self.compositor_state.queue_full_redraw(diff_rect);
            }

            return;
        } else if let MouseInteractionState::PerformingWindowResize(resized_window) =
            &self.mouse_interaction_state
        {
            //println!("\tResizing hover window");
            let old_frame = resized_window.frame();
            let mut new_frame = self.bind_rect_to_screen_size(
                old_frame.replace_size(old_frame.size + Size::new(rel_shift.x, rel_shift.y)),
            );
            // Don't let the window get too small
            new_frame.size.width = max(new_frame.size.width, 200);
            new_frame.size.height = max(new_frame.size.height, 200);
            resized_window.set_frame(new_frame);
            //println!("Set window to {new_frame}");

            #[cfg(target_os = "axle")]
            {
                //println!("Sending response to {source} {source:?}: {window_created_msg:?}");
                amc_message_send(
                    &(resized_window.owner_service),
                    AwmWindowResized::new(resized_window.content_frame().size),
                );
            }

            resized_window.redraw_title_bar();
            let update_rect = old_frame.union(new_frame);
            self.recompute_drawable_regions_in_rect(update_rect);
            for diff_rect in old_frame.area_excluding_rect(new_frame) {
                self.compositor_state.queue_full_redraw(diff_rect);
            }

            return;
        }

        // For the rest of the possible state changes we need the position within the
        // hover window, if any
        let window_containing_mouse = match self.window_containing_point(new_pos) {
            None => {
                self.mouse_interaction_state = MouseInteractionState::BackgroundHover;
                return;
            }
            Some(w) => w,
        };
        let mouse_within_window = window_containing_mouse.frame().translate_point(new_pos);
        if let MouseInteractionState::WindowHover(hovered_window) = &self.mouse_interaction_state {
            // Check whether we should move to drag hint/resize hint states
            if hovered_window.is_point_within_title_bar(mouse_within_window) {
                self.mouse_interaction_state =
                    MouseInteractionState::HintingWindowDrag(Rc::clone(&hovered_window))
            } else if hovered_window.is_point_within_resize_inset(mouse_within_window) {
                self.mouse_interaction_state =
                    MouseInteractionState::HintingWindowResize(Rc::clone(&hovered_window))
            }
        } else if let MouseInteractionState::HintingWindowResize(hovered_window) =
            &self.mouse_interaction_state
        {
            if hovered_window.is_point_within_title_bar(mouse_within_window) {
                self.mouse_interaction_state =
                    MouseInteractionState::HintingWindowDrag(Rc::clone(&hovered_window));
            } else if !hovered_window.is_point_within_resize_inset(mouse_within_window) {
                self.mouse_interaction_state =
                    MouseInteractionState::WindowHover(Rc::clone(&hovered_window))
            }
        } else if let MouseInteractionState::HintingWindowDrag(hovered_window) =
            &self.mouse_interaction_state
        {
            if !hovered_window.is_point_within_title_bar(mouse_within_window) {
                self.mouse_interaction_state =
                    MouseInteractionState::HintingWindowResize(Rc::clone(&hovered_window));
            }
        } else {
            // Only assign a hover window (and clone) if we weren't already in this state
            if !matches!(
                self.mouse_interaction_state,
                MouseInteractionState::WindowHover(_)
            ) {
                self.mouse_interaction_state =
                    MouseInteractionState::WindowHover(Rc::clone(&window_containing_mouse))
            }
        }
    }

    fn handle_mouse_state_change(&mut self, state_change: MouseStateChange) {
        //println!("Mouse state change: {state_change:?}");
        match state_change {
            MouseStateChange::LeftClickBegan => {
                self.handle_left_click_began();
            }
            MouseStateChange::LeftClickEnded => {
                self.handle_left_click_ended();
            }
            MouseStateChange::Moved(new_pos, rel_shift) => {
                self.handle_mouse_moved(new_pos, rel_shift)
            }
        }
    }

    fn handle_mouse_state_changes(
        &mut self,
        old_mouse_frame: Rect,
        state_changes: Vec<MouseStateChange>,
    ) {
        // Don't bother queueing the new mouse position to redraw
        // For simplicity, the compositor will always draw the mouse over each frame
        let new_mouse_frame = self.mouse_state.frame();
        //
        // Previous mouse position should be redrawn
        let total_update_rect =
            self.bind_rect_to_screen_size(old_mouse_frame.union(new_mouse_frame));
        self.compositor_state.queue_full_redraw(total_update_rect);

        for state_change in state_changes.into_iter() {
            self.handle_mouse_state_change(state_change);
        }
    }

    pub fn handle_mouse_packet(&mut self, packet: &MousePacket) {
        let old_mouse_pos = self.mouse_state.frame();
        let new_pos =
            self.mouse_state.pos + Point::new(packet.rel_x as isize, packet.rel_y as isize);
        let state_changes = self
            .mouse_state
            .compute_state_changes(Some(new_pos), packet.status);
        self.handle_mouse_state_changes(old_mouse_pos, state_changes)
    }

    pub fn handle_mouse_absolute_update(&mut self, new_mouse_pos: Option<Point>, status_byte: i8) {
        let old_mouse_pos = self.mouse_state.frame();
        let state_changes = self
            .mouse_state
            .compute_state_changes(new_mouse_pos, status_byte);
        self.handle_mouse_state_changes(old_mouse_pos, state_changes)
    }

    fn bind_rect_to_screen_size(&self, r: Rect) -> Rect {
        /*
        let mut out = r;
        out.origin.x = max(r.origin.x, 0);
        out.origin.y = max(r.origin.y, 0);

        if out.max_x() > desktop_size.width {
            let overhang = out.max_x() - desktop_size.width;
            out.origin.x -= overhang;
        }
        if out.max_y() > desktop_size.height {
            let overhang = out.max_y() - desktop_size.height;
            out.origin.y -= overhang;
        }

        out
        */
        let mut out = r;
        let desktop_size = self.desktop_frame.size;
        //println!("Max of {}, {}: {}", )
        out.origin.x = max(r.origin.x, 0_isize);
        out.origin.y = max(r.origin.y, 0_isize);
        if out.max_x() > desktop_size.width {
            let overhang = out.max_x() - desktop_size.width;
            if out.origin.x >= overhang {
                out.origin.x -= overhang;
            } else {
                out.size.width -= overhang;
            }
        }
        if out.max_y() > desktop_size.height {
            let overhang = out.max_y() - desktop_size.height;
            if out.origin.y >= overhang {
                out.origin.y -= overhang;
            } else {
                out.size.height -= overhang;
            }
        }

        out
    }

    pub fn move_window_to_top(&mut self, window: &Rc<Window>) {
        let window_idx = self
            .windows
            .iter()
            .position(|w| Rc::ptr_eq(w, window))
            .unwrap();
        self.windows.remove(window_idx);
        self.windows.insert(0, Rc::clone(window));
    }

    pub fn handle_keyboard_event(&mut self, packet: &KeyboardPacket) {
        println!("Got keyboard packet {packet:?}");
        if packet.key == 97 && packet.event_type == KeyEventType::Released {
            match self.render_strategy {
                RenderStrategy::TreeWalk => {
                    println!("Switching to compositing");
                    self.render_strategy = RenderStrategy::Composite;
                }
                RenderStrategy::Composite => {
                    println!("Switching to tree walking");
                    self.render_strategy = RenderStrategy::TreeWalk;
                }
            }
        }
    }

    pub fn set_cursor_pos(&mut self, pos: Point) {
        self.mouse_state.pos = pos
    }

    fn window_for_owner(&self, window_owner: &str) -> Rc<Window> {
        // PT: Assumes a single window per client
        Rc::clone(
            self.windows
                .iter()
                .find(|w| w.owner_service == window_owner)
                .expect(format!("Failed to find window for {}", window_owner).as_str()),
        )
    }

    pub fn handle_window_requested_redraw(&mut self, window_owner: &str) {
        let window = self.window_for_owner(window_owner);
        // Render the framebuffer to the visible window layer
        window.render_remote_layer();
        self.compositor_state
            .queue_composite(Rc::clone(&window) as Rc<dyn DesktopElement>)
    }

    pub fn handle_window_updated_title(&self, window_owner: &str, update: &AwmWindowUpdateTitle) {
        let new_title = str_from_u8_nul_utf8_unchecked(&update.title);
        println!("Window for {window_owner} updated title to {new_title}");
        let window = self.window_for_owner(window_owner);
        window.set_title(new_title);
        let title_bar_frame = window.redraw_title_bar();
        println!("Queueing extra draw {title_bar_frame}");
        self.compositor_state.queue_extra_draw(
            Rc::clone(&window) as Rc<dyn DesktopElement>,
            title_bar_frame,
        );
    }

    pub fn test(&mut self) {
        let rects = vec![
            Rect::new(50, 85, 25, 65),
            Rect::new(50, 0, 25, 50),
            Rect::new(0, 100, 50, 50),
            Rect::new(0, 0, 50, 50),
        ];
        for r in rects.iter() {
            let slice = self.video_memory_layer.get_slice(*r);
            slice.fill(random_color());
        }
    }
}

#[cfg(test)]
mod test {
    #![feature(test)]
    use crate::desktop::{Desktop, DesktopElement, Window};
    use agx_definitions::{LikeLayerSlice, Point, Rect, SingleFramebufferLayer, Size};
    use alloc::rc::Rc;
    use awm_messages::AwmCreateWindow;
    use image::codecs::gif::GifEncoder;
    use image::{
        save_buffer_with_format, GenericImage, GenericImageView, ImageBuffer, RgbImage, Rgba,
    };
    use libgui::PixelLayer;
    use mouse_driver_messages::MousePacket;
    use std::cell::RefCell;
    use std::fs;
    use std::fs::OpenOptions;
    use std::iter::zip;
    use test::Bencher;
    use winit::event::Event;
    extern crate test;
    use winit::event_loop::{ControlFlow, EventLoop};

    fn get_desktop_with_size(screen_size: Size) -> Desktop {
        let mut vmem = SingleFramebufferLayer::new(screen_size);
        let layer_as_trait_object = Rc::new(vmem.get_full_slice());
        Desktop::new(Rc::clone(&layer_as_trait_object))
    }

    fn get_desktop() -> Desktop {
        let screen_size = Size::new(1000, 1000);
        let mut vmem = SingleFramebufferLayer::new(screen_size);
        let layer_as_trait_object = Rc::new(vmem.get_full_slice());
        Desktop::new(Rc::clone(&layer_as_trait_object))
    }

    fn spawn_window_easy(desktop: &mut Desktop, name: &str, frame: Rect) -> Rc<Window> {
        desktop.spawn_window(name, &AwmCreateWindow::new(frame.size), Some(frame.origin))
    }

    fn spawn_windows_with_frames(window_frames: Vec<Rect>) -> (Desktop, Vec<Rc<Window>>) {
        let mut desktop = get_desktop();

        let windows: Vec<Rc<Window>> = window_frames
            .iter()
            .enumerate()
            .map(|(i, frame)| {
                spawn_window_easy(
                    &mut desktop,
                    &format!("w{i}"),
                    Rect::from_parts(
                        frame.origin,
                        Size::new(
                            frame.width(),
                            frame.height() - Window::TITLE_BAR_HEIGHT as isize,
                        ),
                    ),
                )
            })
            .collect();
        (desktop, windows)
    }

    fn assert_window_layouts_matches_drawable_rects(
        window_frames: Vec<Rect>,
        expected_drawable_rects: Vec<Vec<Rect>>,
    ) {
        // Given some windows arranged in a desktop
        let (mut desktop, windows) = spawn_windows_with_frames(window_frames);
        desktop.draw_frame();

        let desktop_size = desktop.desktop_frame.size;
        let img: RgbImage = ImageBuffer::new(desktop_size.width as u32, desktop_size.height as u32);
        let desktop_slice = desktop
            .video_memory_layer
            .get_slice(Rect::with_size(desktop_size));
        let mut img = ImageBuffer::from_fn(
            desktop_size.width as u32,
            desktop_size.height as u32,
            |x, y| {
                let px = desktop_slice.getpixel(Point::new(x as isize, y as isize));
                Rgba([px.r, px.g, px.b, 0xff])
            },
        );
        img.save("./test_image.png");

        for (i, window) in windows.iter().enumerate() {
            println!("Window {} has drawable rects:", window.name());
            for r in window.drawable_rects().iter() {
                println!("\t{r}");
            }
            assert_eq!(window.drawable_rects(), expected_drawable_rects[i])
        }
        for (window, expected_drawable_rects) in zip(windows, expected_drawable_rects) {
            assert_eq!(window.drawable_rects(), expected_drawable_rects)
        }
    }

    #[test]
    fn test_compute_drawable_regions() {
        assert_window_layouts_matches_drawable_rects(
            vec![Rect::new(0, 0, 100, 100), Rect::new(50, 0, 100, 100)],
            vec![
                vec![Rect::new(0, 0, 50, 100)],
                vec![Rect::new(50, 0, 100, 100)],
            ],
        );

        assert_window_layouts_matches_drawable_rects(
            vec![
                Rect::new(0, 0, 100, 100),
                Rect::new(50, 50, 100, 100),
                Rect::new(100, 100, 100, 100),
            ],
            vec![
                vec![Rect::new(0, 0, 50, 100), Rect::new(50, 0, 50, 50)],
                vec![Rect::new(50, 50, 50, 100), Rect::new(100, 50, 50, 50)],
                vec![Rect::new(100, 100, 100, 100)],
            ],
        );

        assert_window_layouts_matches_drawable_rects(
            vec![
                Rect::new(200, 200, 100, 130),
                Rect::new(250, 250, 100, 130),
                Rect::new(300, 300, 100, 130),
            ],
            vec![
                vec![Rect::new(200, 200, 50, 130), Rect::new(250, 200, 50, 50)],
                vec![Rect::new(250, 250, 50, 130), Rect::new(300, 250, 50, 50)],
                vec![Rect::new(300, 300, 100, 130)],
            ],
        );

        assert_window_layouts_matches_drawable_rects(
            vec![Rect::new(100, 100, 100, 100), Rect::new(180, 50, 100, 100)],
            vec![
                vec![Rect::new(100, 100, 80, 100), Rect::new(180, 150, 20, 50)],
                vec![Rect::new(180, 50, 100, 100)],
            ],
        );

        assert_window_layouts_matches_drawable_rects(
            vec![
                Rect::new(280, 190, 100, 130),
                Rect::new(250, 250, 100, 130),
                Rect::new(300, 300, 100, 130),
            ],
            vec![
                vec![Rect::new(280, 190, 70, 60), Rect::new(350, 190, 30, 110)],
                vec![Rect::new(250, 250, 50, 130), Rect::new(300, 250, 50, 50)],
                vec![Rect::new(300, 300, 100, 130)],
            ],
        );

        assert_window_layouts_matches_drawable_rects(
            vec![Rect::new(280, 190, 100, 130), Rect::new(280, 190, 100, 130)],
            vec![vec![], vec![Rect::new(280, 190, 100, 130)]],
        );
    }

    fn assert_extra_draws(
        desktop: &Desktop,
        expected_extra_draws: &Vec<(String, Rect)>,
        expected_extra_background_draws: &Vec<Rect>,
    ) {
        for elem in desktop.compositor_state.elements.iter() {
            println!("Drawables of {}:", elem.name());
            for r in elem.drawable_rects().iter() {
                println!("\t{r}");
            }
        }

        println!("Extra draws:");
        for (elem, extra_draw_rect) in desktop.compositor_state.extra_draws.borrow().iter() {
            println!("Extra draw for {}: {extra_draw_rect}", elem.name());
        }
        println!("Extra background draws:");
        for extra_background_draw_rect in desktop.compositor_state.extra_background_draws.iter() {
            println!("Extra background draw: {extra_background_draw_rect}");
        }
        let extra_draws_by_elem_name: Vec<(String, Rect)> = desktop
            .compositor_state
            .extra_draws
            .borrow()
            .iter()
            .map(|(e, r)| (e.name(), *r))
            .collect();
        assert_eq!(&extra_draws_by_elem_name, expected_extra_draws,);
        assert_eq!(
            &desktop.compositor_state.extra_background_draws,
            expected_extra_background_draws,
        );
    }

    #[test]
    fn test_compute_extra_draws_from_total_update() {
        // Given a window on a desktop
        let mut desktop = get_desktop();
        let window = spawn_window_easy(&mut desktop, "window", Rect::new(200, 200, 200, 200));
        desktop
            .compositor_state
            .queue_full_redraw(Rect::new(390, 110, 100, 100));
        desktop.compute_extra_draws_from_total_update_rects();
        assert_extra_draws(
            &desktop,
            &vec![(window.name(), Rect::new(390, 200, 10, 10))],
            &vec![Rect::new(400, 110, 90, 100), Rect::new(390, 110, 10, 90)],
        );
    }

    #[test]
    fn test_compute_extra_draws_from_total_update2() {
        let (mut desktop, windows) = spawn_windows_with_frames(vec![
            Rect::new(100, 100, 100, 100),
            Rect::new(180, 50, 100, 100),
        ]);

        desktop
            .compositor_state
            .queue_full_redraw(Rect::new(150, 110, 100, 130));
        desktop.compute_extra_draws_from_total_update_rects();
        assert_extra_draws(
            &desktop,
            &vec![
                ("w0".to_string(), Rect::new(150, 110, 30, 90)),
                ("w0".to_string(), Rect::new(180, 150, 20, 50)),
                ("w1".to_string(), Rect::new(180, 110, 70, 40)),
            ],
            &vec![
                Rect::new(200, 150, 50, 90),
                Rect::new(180, 200, 20, 40),
                Rect::new(150, 200, 30, 40),
            ],
        );
    }

    #[test]
    fn test_compute_extra_draws_from_total_update3() {
        let (mut desktop, windows) = spawn_windows_with_frames(vec![Rect::new(100, 100, 100, 100)]);

        desktop
            .compositor_state
            .queue_full_redraw(Rect::new(150, 150, 20, 20));
        desktop.compute_extra_draws_from_total_update_rects();
        assert_extra_draws(
            &desktop,
            &vec![("w0".to_string(), Rect::new(150, 150, 20, 20))],
            &vec![],
        );
    }

    #[test]
    fn test_compute_extra_draws_from_total_update4() {
        let (mut desktop, windows) =
            spawn_windows_with_frames(vec![Rect::new(0, 50, 50, 50), Rect::new(50, 50, 100, 35)]);

        desktop
            .compositor_state
            .queue_full_redraw(Rect::new(0, 0, 75, 150));
        desktop.compute_extra_draws_from_total_update_rects();
        assert_extra_draws(
            &desktop,
            &vec![
                ("w0".to_string(), Rect::new(0, 50, 50, 50)),
                ("w1".to_string(), Rect::new(50, 50, 25, 35)),
            ],
            &vec![
                Rect::new(50, 85, 25, 65),
                Rect::new(50, 0, 25, 50),
                Rect::new(0, 100, 50, 50),
                Rect::new(0, 0, 50, 50),
            ],
        );
    }

    #[test]
    fn test_bind_rect_to_screen_size() {
        // Input, expected
        let test_cases = vec![
            (Rect::new(-10, -10, 1000, 1000), Rect::new(0, 0, 990, 990)),
            (Rect::new(0, 0, 1200, 1200), Rect::new(0, 0, 1000, 1000)),
            (Rect::new(100, 100, 950, 950), Rect::new(50, 50, 950, 950)),
        ];
    }

    fn parse_isize_vec(line: &str) -> Vec<isize> {
        println!("parse_isize_vec {line}");
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
            out |= (1 << 0);
        }
        out
    }

    fn replay_capture() {
        let path = "/Users/philliptennen/Documents/develop/axle.nosync/rust_programs/capture.txt";
        let data = Rc::new(
            fs::read_to_string(path)
                .expect("Unable to read file")
                .clone(),
        );
        let data_clone = Rc::clone(&data);
        let mut line_iter = data.split('\n').into_iter().peekable();

        assert_eq!(line_iter.next().unwrap(), "[Size]");

        let desktop_size = parse_size(line_iter.next().unwrap());
        println!("Got desktop size {desktop_size}");
        let mut desktop = get_desktop_with_size(desktop_size);

        desktop.draw_background();
        // Start off by drawing a blank canvas consisting of the desktop background
        desktop.blit_background();
        desktop.commit_entire_buffer_to_video_memory();

        assert_eq!(line_iter.next().unwrap(), "[Windows]");
        let mut window_counter = 0;
        loop {
            let peek = line_iter.peek().unwrap();
            if peek.starts_with('[') {
                break;
            }
            let line = line_iter.next().unwrap();
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
            let w = desktop.spawn_window(
                &format!("win{window_counter}"),
                &AwmCreateWindow::new(window_frame.size),
                Some(window_frame.origin),
            );
            window_counter += 1;
        }
        desktop.draw_frame();

        let mut last_mouse_pos = Point::zero();
        let mut is_left_click_down = false;
        let mut event_count = 0;
        while let Some(line) = line_iter.next() {
            if line == "[MouseMoved]" {
                /*
                //let rel_movement = parse_size_space(line_iter.next().unwrap());
                last_mouse_pos =
                    last_mouse_pos + Point::new(rel_movement.width, rel_movement.height);
                 */
                let new_mouse_pos = parse_point(line_iter.next().unwrap());
                let rel_movement = new_mouse_pos - last_mouse_pos;
                last_mouse_pos = new_mouse_pos;
                desktop.handle_mouse_absolute_update(
                    Some(new_mouse_pos),
                    get_mouse_status_byte(is_left_click_down),
                );
            } else if line == "[MouseDown]" {
                is_left_click_down = true;
                desktop
                    .handle_mouse_absolute_update(None, get_mouse_status_byte(is_left_click_down));
            } else if line == "[MouseUp]" {
                is_left_click_down = false;
                desktop
                    .handle_mouse_absolute_update(None, get_mouse_status_byte(is_left_click_down));
            } else if line == "[SetMousePos]" {
                last_mouse_pos = parse_point(line_iter.next().unwrap());
            } else if line == "\n" || line.len() == 0 {
            } else {
                panic!("Unhandled line {line}");
            }
            desktop.draw_frame();
            event_count += 1;

            let desktop_size = desktop.desktop_frame.size;
            let img: RgbImage =
                ImageBuffer::new(desktop_size.width as u32, desktop_size.height as u32);
            let desktop_slice = desktop
                .video_memory_layer
                .get_slice(Rect::with_size(desktop_size));
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
            let mut pixel_data = desktop_slice.pixel_data();
            /*
            let (prefix, pixel_data_u32, suffix) = unsafe { pixel_data.align_to_mut::<u32>() };
            // Ensure the slice was exactly u32-aligned
            assert_eq!(prefix.len(), 0);
            assert_eq!(suffix.len(), 0);
            /*
            let mut pixels: Vec<Rgba<u32>> = pixel_data_u32
                .into_iter()
                //.map(|word| Rgba([*word >> 24, *word >> 16, *word >> 8, 0xff]))
                .map(|word| Rgba([*word >> 24, *word >> 16, *word >> 8, 0xff]))
                .collect();
            */

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

            let mut img: ImageBuffer<Rgba<u32>, Vec<_>> = ImageBuffer::from_vec(
                desktop_size.width as u32,
                desktop_size.height as u32,
                pixels,
            )
            .unwrap();
            img.save(format!("./test_capture/desktop{event_count}.png"));
             */
            /*
            save_buffer_with_format(
                format!("./test_capture/desktop{event_count}.jpg"),
                &pixel_data,
                desktop_size.width as u32,
                desktop_size.height as u32,
                image::ColorType::Rgba8,
                image::ImageFormat::Jpeg,
            )
            .unwrap();
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

    #[bench]
    fn test_capture(b: &mut Bencher) {
        b.iter(|| {
            replay_capture();
        });
    }
}
