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
use awm_messages::AwmCreateWindow;
use axle_rt::core_commands::AmcSharedMemoryCreateRequest;
use core::cell::RefCell;
use core::cmp::{max, min};
use core::fmt::{Display, Formatter};
use mouse_driver_messages::MousePacket;

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

struct DesktopElementId(usize);

/// A persistent UI element on the desktop that occludes other elements
/// Roughly: a window, a desktop shortcut, etc
trait DesktopElement {
    fn frame(&self) -> Rect;
    fn drawable_rects(&self) -> Vec<Rect>;
    fn set_drawable_rects(&self, drawable_rects: Vec<Rect>);
    fn get_slice(&self) -> Box<dyn LikeLayerSlice>;
}

pub struct Window {
    pub frame: Rect,
    drawable_rects: RefCell<Vec<Rect>>,
    pub owner_service: String,
    pub layer: RefCell<SingleFramebufferLayer>,
}

impl Window {
    fn new(owner_service: &str, frame: Rect, window_layer: SingleFramebufferLayer) -> Self {
        Self {
            frame,
            drawable_rects: RefCell::new(vec![]),
            owner_service: owner_service.to_string(),
            layer: RefCell::new(window_layer),
        }
    }
}

impl Display for Window {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "<Window \"{}\" @ {}>", self.owner_service, self.frame)
    }
}

impl DesktopElement for Window {
    fn frame(&self) -> Rect {
        self.frame
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
            .get_slice(Rect::with_size(self.frame.size))
    }
}

struct MouseState {
    pos: Point,
    desktop_size: Size,
    size: Size,
}

impl MouseState {
    fn new(pos: Point, desktop_size: Size) -> Self {
        Self {
            pos,
            size: Size::new(14, 14),
            desktop_size,
        }
    }

    fn handle_update(&mut self, packet: &MousePacket) {
        self.pos.x += packet.rel_x as isize;
        self.pos.y += packet.rel_y as isize;

        // Bind mouse to screen dimensions
        self.pos.x = max(self.pos.x, 0);
        self.pos.y = max(self.pos.y, 0);
        self.pos.x = min(self.pos.x, self.desktop_size.width - 4);
        self.pos.y = min(self.pos.y, self.desktop_size.height - 10);
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
    elements_to_composite: Vec<Rc<dyn DesktopElement>>,
    // Every desktop element the compositor knows about
    elements: Vec<Rc<dyn DesktopElement>>,

    extra_draws: RefCell<Vec<(Rc<dyn DesktopElement>, Rect)>>,
}

impl CompositorState {
    fn new() -> Self {
        Self {
            rects_to_fully_redraw: vec![],
            elements_to_composite: vec![],
            elements: vec![],
            extra_draws: RefCell::new(vec![]),
        }
    }

    fn queue_full_redraw(&mut self, in_rect: Rect) {
        self.rects_to_fully_redraw.push(in_rect)
    }

    fn queue_composite(&mut self, element: Rc<dyn DesktopElement>) {
        self.elements_to_composite.push(element)
    }

    fn track_element(&mut self, element: Rc<dyn DesktopElement>) {
        self.elements.push(element)
    }

    fn queue_extra_draw(&self, element: Rc<dyn DesktopElement>, r: Rect) {
        self.extra_draws.borrow_mut().push((element, r));
    }
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
    compositor_state: CompositorState,
}

impl Desktop {
    pub fn new(video_memory_layer: Rc<Box<dyn LikeLayerSlice>>) -> Self {
        let desktop_frame = Rect::with_size(video_memory_layer.frame().size);
        video_memory_layer.fill_rect(desktop_frame, Color::yellow(), StrokeThickness::Filled);

        let desktop_background_layer = Box::new(SingleFramebufferLayer::new(desktop_frame.size));
        let screen_buffer_layer = Box::new(SingleFramebufferLayer::new(desktop_frame.size));

        // Start the mouse in the middle of the screen
        let initial_mouse_pos = Point::new(desktop_frame.mid_x(), desktop_frame.mid_y());

        Self {
            desktop_frame: Rect::with_size(video_memory_layer.frame().size),
            video_memory_layer,
            screen_buffer_layer,
            desktop_background_layer,
            windows: vec![],
            mouse_state: MouseState::new(initial_mouse_pos, desktop_frame.size),
            compositor_state: CompositorState::new(),
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
        let mouse_color = Color::green();
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
        for full_redraw_rect in self.compositor_state.rects_to_fully_redraw.clone().iter() {
            //println!("\tProcessing full redraw rect {full_redraw_rect}");
            // For now, just composite the desktop background here
            Self::copy_rect(
                &mut *self.desktop_background_layer.get_slice(self.desktop_frame),
                &mut *self.screen_buffer_layer.get_slice(self.desktop_frame),
                *full_redraw_rect,
            );
            // Keep track of what we've redrawn using desktop elements
            // This will hold what we still need to draw (eventually with the desktop background)
            // let undrawn_areas = vec![full_redraw_rect];

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
                        break 'outer;
                    } else {
                        // This element needs to redraw the intersection of its visible rect and the update rect
                        let intersection = visible_region
                            .area_overlapping_with(*full_redraw_rect)
                            .unwrap();
                        self.compositor_state
                            .queue_extra_draw(Rc::clone(&elem), intersection);
                        // And subtract the area of the rect from the region to update
                        //unobscured_region = update_occlusions(unobscured_region, intersection);
                    }
                }
            }
        }
    }

    pub fn draw_frame(&mut self) {
        self.compute_extra_draws_from_total_update_rects();

        // Composite each desktop element that needs to be composited this frame
        for desktop_element in self.compositor_state.elements_to_composite.drain(..) {
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
                    Color::red(),
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

        // Finally, draw the mouse cursor
        let mouse_rect = self.draw_mouse();

        // Now blit the screen buffer to the backing video memory
        // Follow the same steps as above to only copy what's changed
        // And empty queues as we go
        // We don't need to walk each individual view - we can rely on the logic above to have drawn it to the screen buffer
        self.compositor_state.extra_draws.borrow_mut().drain(..);
        for full_redraw_rect in self.compositor_state.rects_to_fully_redraw.drain(..) {
            Self::copy_rect(
                &mut *self.screen_buffer_layer.get_slice(self.desktop_frame),
                &mut *self.video_memory_layer.get_slice(self.desktop_frame),
                full_redraw_rect,
            );
        }

        Self::copy_rect(
            &mut *self.screen_buffer_layer.get_slice(self.desktop_frame),
            &mut *self.video_memory_layer.get_slice(self.desktop_frame),
            mouse_rect,
        );
    }

    fn copy_rect(src: &mut dyn LikeLayerSlice, dst: &mut dyn LikeLayerSlice, rect: Rect) {
        let src_slice = src.get_slice(rect);
        let dst_slice = dst.get_slice(rect);
        dst_slice.blit2(&src_slice);
    }

    fn recompute_drawable_regions_in_rect(&mut self, rect: Rect) {
        //println!("recompute_drawable_regions_in_rect({rect})");
        // Iterate backwards (from the furthest back to the foremost)
        for elem_idx in (0..self.windows.len()).rev() {
            let (a, b) = self.windows.split_at(elem_idx);
            let elem = &b[0];
            if !rect.intersects_with(elem.frame()) {
                continue;
            }

            elem.set_drawable_rects(vec![elem.frame()]);

            for (occluding_elem_idx, occluding_elem) in a.iter().enumerate().rev() {
                //println!("\toccluding_elem_idx {occluding_elem_idx}");
                if !elem.frame.intersects_with(occluding_elem.frame) {
                    continue;
                }
                //println!("\tOccluding {} by view with frame {}", elem.frame(), occluding_elem.frame());
                let mut new_drawable_rects = vec![];
                for rect in elem.drawable_rects() {
                    let mut visible_portions = rect.area_excluding_rect(occluding_elem.frame());
                    new_drawable_rects.append(&mut visible_portions);
                }
                elem.set_drawable_rects(new_drawable_rects);
            }

            if elem.drawable_rects().len() > 0 {
                self.compositor_state
                    .queue_composite(Rc::clone(elem) as Rc<dyn DesktopElement>);
            }
            /*
            println!("\tNew visible rects for {elem}");
            for drawable_rect in elem.drawable_rects.iter() {
                println!("\t\t{drawable_rect}")
            }
            */
        }
    }

    pub fn spawn_window(
        &mut self,
        source: String,
        request: &AwmCreateWindow,
        origin: Option<Point>,
    ) -> Rc<Window> {
        println!("Creating window of size {:?} for {}", request.size, source);

        let window_size = Size::from(&request.size);
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
                AmcSharedMemoryCreateRequest::send(&source, shared_memory_size as u32);

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
        self.windows.insert(0, Rc::clone(&new_window));
        self.compositor_state
            .track_element(Rc::clone(&new_window) as Rc<dyn DesktopElement>);
        self.recompute_drawable_regions_in_rect(window_frame);

        new_window
    }

    pub fn handle_mouse_update(&mut self, packet: &MousePacket) {
        let old_mouse_frame = self.mouse_state.frame();
        // Previous mouse position should be redrawn
        /*
        self.compositor_state
            .queue_full_redraw(self.mouse_state.frame());
         */

        self.mouse_state.handle_update(packet);
        // Don't bother queueing the new mouse position to redraw
        // For simplicity, the compositor will always draw the mouse over each frame
        let new_mouse_frame = self.mouse_state.frame();

        let total_update_rect = old_mouse_frame.union(new_mouse_frame);
        //println!("old {old_mouse_frame} total_update {total_update_rect}");
        self.compositor_state.queue_full_redraw(total_update_rect);
    }

    pub fn set_cursor_pos(&mut self, pos: Point) {
        self.mouse_state.pos = pos
    }

    pub fn handle_window_requested_redraw(&mut self, window_owner: &str) {
        // Find the window
        let window = {
            // PT: Assumes a single window per client
            self.windows
                .iter()
                .find(|w| w.owner_service == window_owner)
                .expect(format!("Failed to find window for {}", window_owner).as_str())
        };
        self.compositor_state
            .queue_composite(Rc::clone(window) as Rc<dyn DesktopElement>)
    }
}
