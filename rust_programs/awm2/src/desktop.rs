use crate::effects::draw_radial_gradient;

use crate::println;
use agx_definitions::{
    Color, Layer, LikeLayerSlice, Point, Rect, RectInsets, SingleFramebufferLayer, Size,
    StrokeThickness,
};
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::format;
use alloc::rc::Rc;
use alloc::string::{String, ToString};
use alloc::vec;
use alloc::vec::Vec;
use awm_messages::{
    AwmCreateWindow, AwmMouseEntered, AwmMouseExited, AwmMouseLeftClickEnded,
    AwmMouseLeftClickStarted, AwmMouseMoved, AwmMouseScrolled, AwmWindowPartialRedraw,
    AwmWindowResized, AwmWindowUpdateTitle,
};
use axle_rt::core_commands::AmcSharedMemoryCreateRequest;
use core::cell::RefCell;
use core::cmp::{max, min};
use core::fmt::{Display, Formatter};
use mouse_driver_messages::MousePacket;

use file_manager_messages::str_from_u8_nul_utf8_unchecked;
use kb_driver_messages::{KeyEventType, KeyboardPacket};
use lazy_static::lazy_static;
use rand::prelude::*;

#[cfg(target_os = "axle")]
pub extern crate libc;
#[cfg(target_os = "axle")]
mod conditional_imports {
    pub use awm_messages::AwmCreateWindowResponse;
    pub use axle_rt::amc_message_send;
}
#[cfg(not(target_os = "axle"))]
mod conditional_imports {}

use crate::desktop::conditional_imports::*;
use crate::utils::{get_timestamp, random_color, random_color_with_rng};
use crate::window::Window;

fn send_left_click_event(window: &Rc<Window>, mouse_pos: Point) {
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

fn send_left_click_ended_event(window: &Rc<Window>, mouse_pos: Point) {
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

fn send_mouse_entered_event(window: &Rc<Window>) {
    #[cfg(target_os = "axle")]
    {
        amc_message_send(&window.owner_service, AwmMouseEntered::new())
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("send_mouse_entered_event({})", window.name())
    }
}

fn send_mouse_exited_event(window: &Rc<Window>) {
    #[cfg(target_os = "axle")]
    {
        amc_message_send(&window.owner_service, AwmMouseExited::new())
    }
    #[cfg(not(target_os = "axle"))]
    {
        println!("send_mouse_exited_event({})", window.name())
    }
}

fn send_mouse_moved_event(window: &Rc<Window>, mouse_pos: Point) {
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

fn send_window_resized_event(window: &Rc<Window>) {
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

/// A persistent UI element on the desktop that occludes other elements
/// Roughly: a window, a desktop shortcut, etc
pub trait DesktopElement {
    fn id(&self) -> usize;
    fn frame(&self) -> Rect;
    fn name(&self) -> String;
    fn drawable_rects(&self) -> Vec<Rect>;
    fn set_drawable_rects(&self, drawable_rects: Vec<Rect>);
    fn get_slice(&self) -> Box<dyn LikeLayerSlice>;
}

#[derive(Debug)]
enum MouseStateChange {
    LeftClickBegan,
    LeftClickEnded,
    Moved(Point, Point),
    Scrolled(i8),
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
        delta_z: Option<i8>,
        status: i8,
    ) -> Vec<MouseStateChange> {
        let mut out = vec![];

        if let Some(delta_z) = delta_z {
            if delta_z != 0 {
                out.push(MouseStateChange::Scrolled(delta_z))
            }
        }

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
    elements_by_id: BTreeMap<usize, Rc<dyn DesktopElement>>,

    extra_draws: RefCell<BTreeMap<usize, Vec<Rect>>>,
    extra_background_draws: Vec<Rect>,
}

impl CompositorState {
    fn new() -> Self {
        Self {
            rects_to_fully_redraw: vec![],
            elements_to_composite: RefCell::new(vec![]),
            elements: vec![],
            elements_by_id: BTreeMap::new(),
            extra_draws: RefCell::new(BTreeMap::new()),
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
        self.elements.push(Rc::clone(&element));
        self.elements_by_id.insert(element.id(), element);
    }

    fn queue_extra_draw(&self, element: Rc<dyn DesktopElement>, r: Rect) {
        let element_id = element.id();
        let mut extra_draws = self.extra_draws.borrow_mut();
        if !extra_draws.contains_key(&element_id) {
            extra_draws.insert(element_id, vec![]);
        }
        let mut extra_draws_for_element = extra_draws.get_mut(&element_id).unwrap();
        extra_draws_for_element.push(r)
    }

    fn queue_extra_background_draw(&mut self, r: Rect) {
        self.extra_background_draws.push(r);
    }

    fn merge_extra_draws(&self) -> BTreeMap<usize, Vec<Rect>> {
        let mut out = BTreeMap::new();
        for (elem_id, extra_draws) in self.extra_draws.borrow().iter() {
            let mut rects = extra_draws.clone();

            // Sort by X origin
            rects.sort_by(|a, b| {
                if a.origin.x < b.origin.x {
                    core::cmp::Ordering::Less
                } else if a.origin.x > b.origin.x {
                    core::cmp::Ordering::Greater
                } else {
                    core::cmp::Ordering::Equal
                }
            });

            'begin: loop {
                let mut merged_anything = false;
                //let mut unmerged_rects = rects.clone();

                let rects_clone = rects.clone();
                'outer: for (i, r1) in rects_clone.iter().enumerate() {
                    for (j, r2) in rects_clone[i + 1..].iter().enumerate() {
                        if r1.max_x() == r2.min_x()
                            && r1.min_y() == r2.min_y()
                            && r1.max_y() == r2.max_y()
                        {
                            //println!("Merging {r1} and {r2}");
                            //merged_rects.push(r1.union(*r2));
                            merged_anything = true;
                            // r1 and r2 have been merged
                            rects.retain(|r| r != r1 && r != r2);
                            // TODO(PT): Is the sort order still correct?
                            rects.insert(i, r1.union(*r2));
                            continue 'begin;
                        }
                    }
                }

                // TODO(PT): Are we losing rects?

                if !merged_anything {
                    break;
                }

                //rects = merged_rects.clone();
            }
            out.insert(*elem_id, rects);
        }

        out
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

impl PartialEq for MouseInteractionState {
    fn eq(&self, other: &Self) -> bool {
        match self {
            MouseInteractionState::BackgroundHover => match other {
                MouseInteractionState::BackgroundHover => true,
                _ => false,
            },
            MouseInteractionState::WindowHover(w1) => match other {
                MouseInteractionState::WindowHover(w2) => Rc::ptr_eq(w1, w2),
                _ => false,
            },
            MouseInteractionState::HintingWindowDrag(w1) => match other {
                MouseInteractionState::HintingWindowDrag(w2) => Rc::ptr_eq(w1, w2),
                _ => false,
            },
            MouseInteractionState::HintingWindowResize(w1) => match other {
                MouseInteractionState::HintingWindowResize(w2) => Rc::ptr_eq(w1, w2),
                _ => false,
            },
            MouseInteractionState::PerformingWindowDrag(w1) => match other {
                MouseInteractionState::PerformingWindowDrag(w2) => Rc::ptr_eq(w1, w2),
                _ => false,
            },
            MouseInteractionState::PerformingWindowResize(w1) => match other {
                MouseInteractionState::PerformingWindowResize(w2) => Rc::ptr_eq(w1, w2),
                _ => false,
            },
        }
    }
}

pub enum RenderStrategy {
    TreeWalk,
    Composite,
}

pub struct Desktop {
    pub desktop_frame: Rect,
    // The final video memory
    pub video_memory_layer: Rc<Box<dyn LikeLayerSlice>>,
    screen_buffer_layer: Box<SingleFramebufferLayer>,
    desktop_background_layer: Box<SingleFramebufferLayer>,
    // Index 0 is the foremost window
    pub windows: Vec<Rc<Window>>,
    mouse_state: MouseState,
    mouse_interaction_state: MouseInteractionState,
    compositor_state: CompositorState,
    pub render_strategy: RenderStrategy,
    rng: SmallRng,
    pub background_gradient_inner_color: Color,
    pub background_gradient_outer_color: Color,
    next_desktop_element_id: usize,
    windows_to_render_remote_layers_this_cycle: Vec<Rc<Window>>,
    frame_render_logs: Vec<String>,
}

impl Desktop {
    pub fn new(video_memory_layer: Rc<Box<dyn LikeLayerSlice>>) -> Self {
        let desktop_frame = Rect::with_size(video_memory_layer.frame().size);

        let desktop_background_layer = Box::new(SingleFramebufferLayer::new(desktop_frame.size));
        let screen_buffer_layer = Box::new(SingleFramebufferLayer::new(desktop_frame.size));

        // Start the mouse in the middle of the screen
        let initial_mouse_pos = Point::new(desktop_frame.mid_x(), desktop_frame.mid_y());

        let mut rng = SmallRng::seed_from_u64(get_timestamp());
        let background_gradient_inner_color = random_color_with_rng(&mut rng);
        let background_gradient_outer_color = random_color_with_rng(&mut rng);
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
            rng,
            background_gradient_inner_color,
            background_gradient_outer_color,
            next_desktop_element_id: 0,
            windows_to_render_remote_layers_this_cycle: vec![],
            frame_render_logs: vec![],
        }
    }

    pub fn draw_background(&self) {
        draw_radial_gradient(
            &self.desktop_background_layer,
            self.desktop_background_layer.size(),
            self.background_gradient_inner_color,
            self.background_gradient_outer_color,
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

    fn append_log(&mut self, log: String) {
        self.frame_render_logs.push(log)
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
        self.compositor_state.extra_draws.borrow_mut().clear();
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
        let start = get_timestamp();

        let mut logs: Vec<String> = self.frame_render_logs.drain(..).collect();
        // First, fetch the remote framebuffers for windows that requested it
        logs.push(format!("Fetching framebufs for windows:"));
        for window_to_fetch in self.windows_to_render_remote_layers_this_cycle.drain(..) {
            logs.push(format!("\t{}", window_to_fetch.name()));
            window_to_fetch.render_remote_layer();
        }

        self.compute_extra_draws_from_total_update_rects();

        logs.push(format!("Elements to composite:"));
        // Composite each desktop element that needs to be composited this frame
        for desktop_element in self
            .compositor_state
            .elements_to_composite
            .borrow_mut()
            .drain(..)
        {
            logs.push(format!("\t{}", desktop_element.name()));
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
        logs.push(format!("Extra draws:"));
        for (desktop_element_id, screen_rects) in
            self.compositor_state.merge_extra_draws().into_iter()
        {
            let desktop_element = self
                .compositor_state
                .elements_by_id
                .get(&desktop_element_id)
                .unwrap();
            for screen_rect in screen_rects.iter() {
                logs.push(format!("\t{}, {}", desktop_element.name(), screen_rect));
                let layer = desktop_element.get_slice();
                let local_origin = screen_rect.origin - desktop_element.frame().origin;
                let local_rect = Rect::from_parts(local_origin, screen_rect.size);
                let src_slice = layer.get_slice(local_rect);
                let dst_slice = self.screen_buffer_layer.get_slice(*screen_rect);
                dst_slice.blit2(&src_slice);
            }
        }

        // Copy the bits of the background that we decided we needed to redraw
        logs.push(format!("Extra background draws:"));
        for background_copy_rect in self.compositor_state.extra_background_draws.drain(..) {
            logs.push(format!("\t{background_copy_rect}"));
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
        for (_, extra_draw_rects) in self.compositor_state.extra_draws.borrow().iter() {
            for screen_rect in extra_draw_rects.iter() {
                Self::copy_rect(buffer, vmem, *screen_rect);
            }
        }
        self.compositor_state.extra_draws.borrow_mut().clear();

        for full_redraw_rect in self.compositor_state.rects_to_fully_redraw.drain(..) {
            Self::copy_rect(buffer, vmem, full_redraw_rect);
        }

        Self::copy_rect(buffer, vmem, mouse_rect);

        let end = get_timestamp();
        logs.push(format!("Finished frame in {}ms", end - start));
        if end - start >= 10 {
            for l in logs.into_iter() {
                println!("\t{l}");
            }
        }
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

    pub fn next_desktop_element_id(&mut self) -> usize {
        let ret = self.next_desktop_element_id;
        self.next_desktop_element_id += 1;
        ret
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
        let max_content_view_size = Window::content_size_for_total_size(desktop_size);
        #[cfg(target_os = "axle")]
        let content_view_layer = {
            // Ask the kernel to set up a shared memory mapping we'll use for the framebuffer
            // The framebuffer will be the screen size to allow window resizing
            let bytes_per_pixel = self.screen_buffer_layer.bytes_per_pixel();
            let shared_memory_size =
                max_content_view_size.width * max_content_view_size.height * bytes_per_pixel;
            println!(
                "Requesting shared memory of size {shared_memory_size} {max_content_view_size:?}"
            );
            let shared_memory_response =
                AmcSharedMemoryCreateRequest::send(&source.to_string(), shared_memory_size as u32);

            let framebuffer_slice = core::ptr::slice_from_raw_parts_mut(
                shared_memory_response.local_buffer_start as *mut libc::c_void,
                shared_memory_size as usize,
            );
            let framebuffer: &mut [u8] = unsafe { &mut *(framebuffer_slice as *mut [u8]) };
            let window_created_msg = AwmCreateWindowResponse::new(
                max_content_view_size,
                bytes_per_pixel as u32,
                shared_memory_response.remote_buffer_start,
            );
            println!("Sending response to {source} {source:?}: {window_created_msg:?}");
            amc_message_send(&source, window_created_msg);
            SingleFramebufferLayer::from_framebuffer(
                unsafe { Box::from_raw(framebuffer) },
                bytes_per_pixel,
                max_content_view_size,
            )
        };
        #[cfg(not(target_os = "axle"))]
        let content_view_layer = SingleFramebufferLayer::new(max_content_view_size);

        let window_frame = Rect::from_parts(new_window_origin, window_size);
        let new_window = Rc::new(Window::new(
            self.next_desktop_element_id(),
            &source,
            window_frame,
            content_view_layer,
        ));
        new_window.redraw_title_bar();
        self.windows.insert(0, Rc::clone(&new_window));
        self.compositor_state
            .track_element(Rc::clone(&new_window) as Rc<dyn DesktopElement>);
        self.recompute_drawable_regions_in_rect(window_frame);

        // TODO(PT): Testing
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
        // Allow the mouse state to change based on the movement
        self.transition_to_mouse_interaction_state(
            self.mouse_interaction_state_for_mouse_state(true),
        );
        if let Some(window_under_mouse) = self.window_containing_point(self.mouse_state.pos) {
            if !Rc::ptr_eq(&self.windows[0], &window_under_mouse) {
                println!(
                    "Moving clicked window to top: {}",
                    window_under_mouse.name()
                );
                self.move_window_to_top(&window_under_mouse);
                self.recompute_drawable_regions_in_rect(window_under_mouse.frame());
            }
            send_left_click_event(&window_under_mouse, self.mouse_state.pos)
        }
    }

    fn can_transition_to_window_drag(&self, window_under_mouse: &Rc<Window>) -> bool {
        if let MouseInteractionState::HintingWindowDrag(win) = &self.mouse_interaction_state {
            if Rc::ptr_eq(window_under_mouse, win) {
                if self.mouse_state.left_click_down {
                    return true;
                }
            }
        }
        false
    }

    fn can_transition_to_window_resize(&self, window_under_mouse: &Rc<Window>) -> bool {
        if let MouseInteractionState::HintingWindowResize(win) = &self.mouse_interaction_state {
            if Rc::ptr_eq(window_under_mouse, win) {
                if self.mouse_state.left_click_down {
                    return true;
                }
            }
        }
        false
    }

    fn mouse_interaction_state_for_mouse_state(
        &self,
        allow_transition_from_hint_to_perform: bool,
    ) -> MouseInteractionState {
        if let Some(window_under_mouse) = self.window_containing_point(self.mouse_state.pos) {
            let mouse_within_window = window_under_mouse
                .frame()
                .translate_point(self.mouse_state.pos);
            if window_under_mouse.is_point_within_title_bar(mouse_within_window) {
                if allow_transition_from_hint_to_perform
                    && self.can_transition_to_window_drag(&window_under_mouse)
                {
                    MouseInteractionState::PerformingWindowDrag(Rc::clone(&window_under_mouse))
                } else {
                    MouseInteractionState::HintingWindowDrag(Rc::clone(&window_under_mouse))
                }
            } else if window_under_mouse.is_point_within_resize_inset(mouse_within_window) {
                if allow_transition_from_hint_to_perform
                    && self.can_transition_to_window_resize(&window_under_mouse)
                {
                    MouseInteractionState::PerformingWindowResize(Rc::clone(&window_under_mouse))
                } else {
                    MouseInteractionState::HintingWindowResize(Rc::clone(&window_under_mouse))
                }
            } else {
                MouseInteractionState::WindowHover(Rc::clone(&window_under_mouse))
            }
        } else {
            MouseInteractionState::BackgroundHover
        }
    }

    fn handle_left_click_ended(&mut self) {
        // Simple case so handle it directly
        match &self.mouse_interaction_state {
            MouseInteractionState::PerformingWindowDrag(win) => {
                // End window drag
                self.mouse_interaction_state =
                    MouseInteractionState::HintingWindowDrag(Rc::clone(&win))
            }
            MouseInteractionState::PerformingWindowResize(win) => {
                // End window resize
                self.mouse_interaction_state =
                    MouseInteractionState::HintingWindowResize(Rc::clone(&win))
            }
            MouseInteractionState::WindowHover(win) => {
                // End left click
                send_left_click_ended_event(win, self.mouse_state.pos)
            }
            _ => {}
        }
    }

    /// Handles informing about mouse enter/exit if necessary given the state we're transferring to/from
    fn transition_to_mouse_interaction_state(&mut self, new_state: MouseInteractionState) {
        if self.mouse_interaction_state == new_state {
            return;
        }
        // If we're transitioning out of a window, inform it
        match &self.mouse_interaction_state {
            MouseInteractionState::BackgroundHover => {}
            MouseInteractionState::WindowHover(w)
            | MouseInteractionState::HintingWindowDrag(w)
            | MouseInteractionState::HintingWindowResize(w)
            | MouseInteractionState::PerformingWindowDrag(w)
            | MouseInteractionState::PerformingWindowResize(w) => {
                // Has the mouse left the window?
                let exited_window = match &new_state {
                    MouseInteractionState::BackgroundHover => true,
                    MouseInteractionState::WindowHover(w2)
                    | MouseInteractionState::HintingWindowDrag(w2)
                    | MouseInteractionState::HintingWindowResize(w2)
                    | MouseInteractionState::PerformingWindowDrag(w2)
                    | MouseInteractionState::PerformingWindowResize(w2) => !Rc::ptr_eq(w, w2),
                };
                if exited_window {
                    //println!("Informing window {} about mouse exit", w.name());
                    send_mouse_exited_event(&w)
                }
            }
        };

        // If we're transitioning into of a window, inform it
        if let MouseInteractionState::WindowHover(new_win)
        | MouseInteractionState::HintingWindowDrag(new_win)
        | MouseInteractionState::HintingWindowResize(new_win)
        | MouseInteractionState::PerformingWindowDrag(new_win)
        | MouseInteractionState::PerformingWindowResize(new_win) = &new_state
        {
            let entered_window = match &self.mouse_interaction_state {
                MouseInteractionState::BackgroundHover => true,
                MouseInteractionState::WindowHover(old_win)
                | MouseInteractionState::HintingWindowDrag(old_win)
                | MouseInteractionState::HintingWindowResize(old_win)
                | MouseInteractionState::PerformingWindowDrag(old_win)
                | MouseInteractionState::PerformingWindowResize(old_win) => {
                    // Is the new state not about the same window?
                    !Rc::ptr_eq(new_win, old_win)
                }
            };
            if entered_window {
                send_mouse_entered_event(&new_win)
            }
        }
        self.mouse_interaction_state = new_state;
    }

    fn handle_mouse_moved(&mut self, new_pos: Point, rel_shift: Point) {
        if let MouseInteractionState::PerformingWindowDrag(dragged_window) =
            &self.mouse_interaction_state
        {
            // We're in the middle of dragging a window
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
            // We're in the middle of resizing a window
            let old_frame = resized_window.frame();
            let mut new_frame = self.bind_rect_to_screen_size(
                old_frame.replace_size(old_frame.size + Size::new(rel_shift.x, rel_shift.y)),
            );
            // Don't let the window get too small
            new_frame.size.width = max(new_frame.size.width, 200);
            new_frame.size.height = max(new_frame.size.height, 200);
            resized_window.set_frame(new_frame);
            //println!("Set window to {new_frame}");
            send_window_resized_event(&resized_window);

            resized_window.redraw_title_bar();
            let update_rect = old_frame.union(new_frame);
            self.recompute_drawable_regions_in_rect(update_rect);
            for diff_rect in old_frame.area_excluding_rect(new_frame) {
                self.compositor_state.queue_full_redraw(diff_rect);
            }

            return;
        }

        // Allow the mouse state to change based on the movement
        // But don't allow the mouse state to transition from resize/drag hints to active resize/drag from mouse movement alone
        self.transition_to_mouse_interaction_state(
            self.mouse_interaction_state_for_mouse_state(false),
        );

        if let MouseInteractionState::WindowHover(window_under_mouse) =
            &self.mouse_interaction_state
        {
            send_mouse_moved_event(window_under_mouse, self.mouse_state.pos);
        }
    }

    fn handle_mouse_scrolled(&mut self, delta_z: i8) {
        if let MouseInteractionState::WindowHover(hover_window) = &self.mouse_interaction_state {
            // Scroll within a window, inform the window
            let mouse_within_window = hover_window.frame().translate_point(self.mouse_state.pos);
            let mouse_within_content_view = hover_window
                .content_frame()
                .translate_point(mouse_within_window);
            /*
            println!(
                "Mouse scrolled within hover window {} {delta_z}",
                hover_window.name()
            );
            */
            #[cfg(target_os = "axle")]
            {
                let mouse_scrolled_msg = AwmMouseScrolled::new(mouse_within_content_view, delta_z);
                amc_message_send(&hover_window.owner_service, mouse_scrolled_msg);
            }
        } else {
            //println!("scroll outside window");
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
            MouseStateChange::Scrolled(delta_z) => self.handle_mouse_scrolled(delta_z),
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
        let state_changes = self.mouse_state.compute_state_changes(
            Some(new_pos),
            Some(packet.rel_z),
            packet.status,
        );
        self.handle_mouse_state_changes(old_mouse_pos, state_changes)
    }

    pub fn handle_mouse_absolute_update(
        &mut self,
        new_mouse_pos: Option<Point>,
        delta_z: Option<i8>,
        status_byte: i8,
    ) {
        let old_mouse_pos = self.mouse_state.frame();
        let state_changes =
            self.mouse_state
                .compute_state_changes(new_mouse_pos, delta_z, status_byte);
        self.handle_mouse_state_changes(old_mouse_pos, state_changes)
    }

    fn bind_rect_to_screen_size(&self, r: Rect) -> Rect {
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
        if !self
            .windows_to_render_remote_layers_this_cycle
            .iter()
            .any(|w| Rc::ptr_eq(w, &window))
        {
            self.windows_to_render_remote_layers_this_cycle
                .push(Rc::clone(&window));
        } else {
            println!("Ignoring extra draw request for {}", window.name());
        }
        self.compositor_state
            .queue_composite(Rc::clone(&window) as Rc<dyn DesktopElement>)
    }

    pub fn handle_window_requested_partial_redraw(
        &mut self,
        window_owner: &str,
        update: &AwmWindowPartialRedraw,
    ) {
        println!(
            "Got partial redraw from {window_owner} with {} rects:",
            update.rect_count
        );
        let rects: Vec<Rect> = update.rects[..update.rect_count as usize]
            .iter()
            .map(|r_u32| Rect::from(*r_u32))
            .collect();
        for r in rects.iter() {
            println!("\t{r}")
        }
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
    use std::collections::BTreeMap;
    use std::fs;
    use std::fs::OpenOptions;
    use std::iter::zip;
    //use test::Bencher;
    use winit::event::Event;
    //extern crate test;
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
        expected_extra_draws: &BTreeMap<String, Vec<Rect>>,
        expected_extra_background_draws: &Vec<Rect>,
    ) {
        for elem in desktop.compositor_state.elements.iter() {
            println!("Drawables of {}:", elem.name());
            for r in elem.drawable_rects().iter() {
                println!("\t{r}");
            }
        }

        println!("Extra draws:");
        for (elem_id, extra_draw_rects) in desktop.compositor_state.extra_draws.borrow().iter() {
            let elem = desktop
                .compositor_state
                .elements_by_id
                .get(elem_id)
                .unwrap();
            for extra_draw_rect in extra_draw_rects.iter() {
                println!("Extra draw for {}: {extra_draw_rect}", elem.name());
            }
        }
        println!("Extra background draws:");
        for extra_background_draw_rect in desktop.compositor_state.extra_background_draws.iter() {
            println!("Extra background draw: {extra_background_draw_rect}");
        }
        let extra_draws_by_elem_name: BTreeMap<String, Vec<Rect>> = desktop
            .compositor_state
            .extra_draws
            .borrow()
            .iter()
            .map(|(e_id, rects)| {
                let elem = desktop.compositor_state.elements_by_id.get(e_id).unwrap();
                (elem.name(), rects.clone())
            })
            .collect();
        assert_eq!(&extra_draws_by_elem_name, expected_extra_draws);
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
            &BTreeMap::from([(window.name(), vec![Rect::new(390, 200, 10, 10)])]),
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
            &BTreeMap::from([
                (
                    "w0".to_string(),
                    vec![Rect::new(150, 110, 30, 90), Rect::new(180, 150, 20, 50)],
                ),
                ("w1".to_string(), vec![Rect::new(180, 110, 70, 40)]),
            ]),
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
            &BTreeMap::from([("w0".to_string(), vec![Rect::new(150, 150, 20, 20)])]),
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
            &BTreeMap::from([
                ("w0".to_string(), vec![Rect::new(0, 50, 50, 50)]),
                ("w1".to_string(), vec![Rect::new(50, 50, 25, 35)]),
            ]),
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

    /*
    #[bench]
    fn test_capture(b: &mut Bencher) {
        b.iter(|| {
            replay_capture();
        });
    }
    */

    #[test]
    fn test_merge_extra_draws() {
        // Given a few adjacent extra-draw rectangles
        let (mut desktop, windows) = spawn_windows_with_frames(vec![Rect::new(700, 800, 500, 80)]);
        let window_as_desktop_elem = Rc::clone(&windows[0]);
        for extra_draw_rect in vec![
            Rect::new(710, 801, 125, 53),
            Rect::new(835, 801, 250, 53),
            Rect::new(1085, 801, 125, 53),
        ]
        .into_iter()
        {
            desktop.compositor_state.queue_extra_draw(
                Rc::clone(&window_as_desktop_elem) as Rc<dyn DesktopElement>,
                extra_draw_rect,
            );
        }
        for (i, rects) in desktop.compositor_state.merge_extra_draws() {
            for r in rects.iter() {
                println!("{r}");
            }
        }

        assert_eq!(
            desktop.compositor_state.merge_extra_draws(),
            BTreeMap::from([(
                window_as_desktop_elem.id(),
                vec![Rect::new(710, 801, 500, 53)]
            )])
        );
    }
}
