use crate::effects::draw_radial_gradient;

use crate::println;
use agx_definitions::{
    Color, Layer, LikeLayerSlice, PixelByteLayout, Point, Rect, RectInsets, SingleFramebufferLayer,
    Size, StrokeThickness,
};
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::collections::BTreeSet;
use alloc::format;
use alloc::rc::Rc;
use alloc::string::{String, ToString};
use alloc::vec;
use alloc::vec::Vec;
use awm_messages::{AwmCreateWindow, AwmWindowPartialRedraw, AwmWindowUpdateTitle};
use core::cell::RefCell;
use core::cmp::{max, min};
use core::mem;
use core::ptr;
use mouse_driver_messages::MousePacket;

use crate::animations::{Animation, ShortcutSnapAnimationParams, WindowTransformAnimationParams};
use crate::bitmap::BitmapImage;
use crate::compositor::CompositorState;
use axle_rt::core_commands::{
    AmcRegisterServiceDiedNotif, AmcServiceDiedNotif, AmcSharedMemoryCreateRequest,
};
use dock_messages::{AwmDockTaskViewClicked, AwmDockWindowMinimizeWithInfo, AWM_DOCK_HEIGHT};
use file_manager_messages::{
    str_from_u8_nul_utf8_unchecked, ReadFile, ReadFileResponse, FILE_SERVER_SERVICE_NAME,
};
use kb_driver_messages::{KeyEventType, KeyIdentifier, KeyboardPacket};
use menu_bar_messages::AWM_MENU_BAR_HEIGHT;
use preferences_messages::PreferencesUpdated;
use rand::prelude::*;

#[cfg(target_os = "axle")]
pub extern crate libc;
#[cfg(target_os = "axle")]
mod conditional_imports {
    pub use awm_messages::AwmCreateWindowResponse;
    pub use axle_rt::{amc_message_await__u32_event, amc_message_send, AmcMessage};
}
#[cfg(not(target_os = "axle"))]
mod conditional_imports {}

use crate::desktop::conditional_imports::*;
use crate::events::{
    inform_dock_window_closed, inform_dock_window_created, inform_dock_window_title_updated,
    send_close_window_request, send_initiate_window_minimize, send_key_down_event,
    send_key_up_event, send_left_click_ended_event, send_left_click_event,
    send_mouse_dragged_event, send_mouse_entered_event, send_mouse_exited_event,
    send_mouse_moved_event, send_mouse_scrolled_event, send_window_resized_event,
};
use crate::keyboard::{KeyboardModifier, KeyboardState};
use crate::mouse::{MouseInteractionState, MouseState, MouseStateChange};
use crate::shortcuts::{DesktopShortcut, DesktopShortcutsState};
use crate::utils::{
    awm_service_is_dock, awm_service_is_menu_bar, get_timestamp, random_color,
    random_color_with_rng,
};
use crate::window::{
    SharedMemoryLayer, TitleBarButtonsHoverState, Window, WindowDecorationImages, WindowParams,
};

#[derive(Debug, Ord, PartialOrd, Copy, Clone, Eq, PartialEq)]
pub enum DesktopElementZIndexCategory {
    FloatingWindow,
    Window,
    DesktopView,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum MouseInteractionCallbackResult {
    NoRedrawNecessary,
    RedrawRequested,
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
    fn z_index_category(&self) -> DesktopElementZIndexCategory;
    fn handle_mouse_entered(&self) -> MouseInteractionCallbackResult {
        MouseInteractionCallbackResult::NoRedrawNecessary
    }
    fn handle_mouse_exited(&self) -> MouseInteractionCallbackResult {
        MouseInteractionCallbackResult::NoRedrawNecessary
    }
    fn handle_mouse_moved(&self, _mouse_pos: Point) -> MouseInteractionCallbackResult {
        MouseInteractionCallbackResult::NoRedrawNecessary
    }
    fn handle_left_click_began(&self, _mouse_pos: Point) -> MouseInteractionCallbackResult {
        MouseInteractionCallbackResult::NoRedrawNecessary
    }
    fn handle_left_click_ended(&self, _mouse_pos: Point) -> MouseInteractionCallbackResult {
        MouseInteractionCallbackResult::NoRedrawNecessary
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
    keyboard_state: KeyboardState,
    compositor_state: CompositorState,
    pub render_strategy: RenderStrategy,
    rng: SmallRng,
    pub background_gradient_inner_color: Color,
    pub background_gradient_outer_color: Color,
    next_desktop_element_id: usize,
    windows_to_render_remote_layers_this_cycle: Vec<Rc<Window>>,
    frame_render_logs: Vec<String>,
    ongoing_animations: Vec<Animation>,
    window_images: WindowDecorationImages,
    desktop_shortcuts_state: DesktopShortcutsState,
    desktop_shortcut_image: BitmapImage,
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
        let desktop_frame = Rect::with_size(video_memory_layer.frame().size);

        let window_images = WindowDecorationImages::new(
            BitmapImage::read_bmp_from_path("/images/titlebar7.bmp"),
            BitmapImage::read_bmp_from_path("/images/titlebar_x_unfilled2.bmp"),
            BitmapImage::read_bmp_from_path("/images/titlebar_x_filled2.bmp"),
            BitmapImage::read_bmp_from_path("/images/titlebar_minimize_unfilled.bmp"),
            BitmapImage::read_bmp_from_path("/images/titlebar_minimize_filled.bmp"),
        );

        let desktop_shortcuts_state = DesktopShortcutsState::new(desktop_frame.size);

        Self {
            desktop_frame,
            video_memory_layer,
            screen_buffer_layer,
            desktop_background_layer,
            windows: vec![],
            compositor_state: CompositorState::new(desktop_frame),
            render_strategy: RenderStrategy::Composite,
            mouse_state: MouseState::new(initial_mouse_pos, desktop_frame.size),
            mouse_interaction_state: MouseInteractionState::BackgroundHover,
            keyboard_state: KeyboardState::new(),
            rng,
            background_gradient_inner_color,
            background_gradient_outer_color,
            next_desktop_element_id: 0,
            windows_to_render_remote_layers_this_cycle: vec![],
            frame_render_logs: vec![],
            ongoing_animations: vec![],
            window_images,
            desktop_shortcuts_state,
            desktop_shortcut_image: BitmapImage::read_bmp_from_path("/images/executable_icon.bmp"),
        }
    }

    pub fn load_shortcuts(&mut self) {
        #[cfg(target_os = "axle")]
        {
            let file_read_request = ReadFile::new("/config/desktop_shortcuts.txt");
            amc_message_send(FILE_SERVER_SERVICE_NAME, file_read_request);
            let file_data_msg: AmcMessage<ReadFileResponse> =
                amc_message_await__u32_event(FILE_SERVER_SERVICE_NAME);
            let file_data_body = file_data_msg.body();
            let file_bytes = unsafe {
                let bmp_data_slice = ptr::slice_from_raw_parts(
                    (&file_data_body.data) as *const u8,
                    file_data_body.len,
                );
                let bmp_data: &[u8] = &*(bmp_data_slice as *const [u8]);
                bmp_data.to_vec()
            };
            let file_data = String::from_utf8(file_bytes).unwrap();
            for line in file_data.split("\n") {
                let components: Vec<&str> = line.split(", ").collect();
                if components.len() != 2 && components.len() != 4 {
                    println!("Ignoring line with unknown format: \"{line}\"");
                    continue;
                }
                let path = components[0];
                let title = components[1];
                let id = self.next_desktop_element_id();
                let shortcut = if components.len() == 4 {
                    let (x, y) = (components[2], components[3]);
                    let coordinates = (
                        isize::from_str_radix(x, 10).unwrap(),
                        isize::from_str_radix(y, 10).unwrap(),
                    );
                    self.desktop_shortcuts_state.add_shortcut_by_coordinates(
                        &mut self.desktop_background_layer,
                        self.background_gradient_outer_color,
                        id,
                        &self.desktop_shortcut_image,
                        path,
                        title,
                        coordinates,
                    )
                } else {
                    self.desktop_shortcuts_state.add_shortcut_to_next_free_slot(
                        &mut self.desktop_background_layer,
                        self.background_gradient_outer_color,
                        id,
                        &self.desktop_shortcut_image,
                        path,
                        title,
                    )
                };
                self.compositor_state
                    .track_element(Rc::clone(&shortcut) as Rc<dyn DesktopElement>);
                self.recompute_drawable_regions_in_rect(shortcut.frame());
            }
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
        let mouse_color = {
            match self.mouse_interaction_state {
                MouseInteractionState::BackgroundHover
                | MouseInteractionState::WindowHover(_)
                | MouseInteractionState::MouseDragWithinWindow(_) => Color::green(),
                MouseInteractionState::HintingWindowDrag(_) => Color::new(121, 160, 217),
                MouseInteractionState::HintingWindowResize(_) => Color::new(212, 119, 201),
                MouseInteractionState::PerformingWindowDrag(_) => Color::new(30, 65, 217),
                MouseInteractionState::PerformingWindowResize(_) => Color::new(207, 25, 185),
                MouseInteractionState::ShortcutHover(_) => Color::new(140, 140, 140),
                MouseInteractionState::ShortcutDrag(_) => Color::new(100, 100, 100),
            }
        };

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
        let full_redraw_rects = self.compositor_state.rects_to_fully_redraw.borrow().clone();
        for full_redraw_rect in full_redraw_rects.iter() {
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
        let _mouse_rect = self.draw_mouse();
        self.compositor_state.extra_draws.borrow_mut().clear();
        self.compositor_state
            .rects_to_fully_redraw
            .borrow_mut()
            .drain(..);
        self.compositor_state
            .elements_to_composite
            .borrow_mut()
            .clear();

        Self::copy_rect(
            &mut *self.screen_buffer_layer.get_full_slice(),
            &mut *self
                .video_memory_layer
                .get_slice(Rect::with_size(self.desktop_frame.size)),
            self.desktop_frame,
        );
    }

    pub fn draw_frame_composited(&mut self) {
        let _start = get_timestamp();

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
        for desktop_element_id in self
            .compositor_state
            .elements_to_composite
            .borrow_mut()
            // PT: BTreeSet doesn't support drain(..)
            .drain_filter(|_| true)
        {
            let desktop_element = self
                .compositor_state
                .elements_by_id
                .get(&desktop_element_id)
                .unwrap();
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
        for background_copy_rect in self.compositor_state.extra_background_draws.iter() {
            logs.push(format!("\t{background_copy_rect}"));
            //println!("Drawing background rect {background_copy_rect}");
            Self::copy_rect(
                &mut *self.desktop_background_layer.get_slice(self.desktop_frame),
                &mut *self.screen_buffer_layer.get_slice(self.desktop_frame),
                *background_copy_rect,
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

        for background_copy_rect in self.compositor_state.extra_background_draws.drain(..) {
            Self::copy_rect(buffer, vmem, background_copy_rect);
        }

        for full_redraw_rect in self
            .compositor_state
            .rects_to_fully_redraw
            .borrow_mut()
            .drain(..)
        {
            Self::copy_rect(buffer, vmem, full_redraw_rect);
        }

        Self::copy_rect(buffer, vmem, mouse_rect);

        /*
        let end = get_timestamp();
        logs.push(format!("Finished frame in {}ms", end - start));
        if end - start >= 10 {
            for l in logs.into_iter() {
                println!("\t{l}");
            }
        }
        */
    }

    fn copy_rect(src: &mut dyn LikeLayerSlice, dst: &mut dyn LikeLayerSlice, rect: Rect) {
        let src_slice = src.get_slice(rect);
        let dst_slice = dst.get_slice(rect);
        dst_slice.blit2(&src_slice);
    }

    fn recompute_drawable_regions_in_rect(&mut self, rect: Rect) {
        //println!("recompute_drawable_regions_in_rect({rect})");
        // Start off by sorting the view hierarchy
        let mut elements_sorted_by_z_index = vec![];
        // Floating windows should always display above anything else
        for elem in self.compositor_state.elements.iter() {
            if elem.z_index_category() == DesktopElementZIndexCategory::FloatingWindow {
                elements_sorted_by_z_index.push(Rc::clone(&elem));
            }
        }
        // Then we should display regular windows, which are sorted by Z-order in the `windows` field
        for win in self.windows.iter() {
            if win.z_index_category() == DesktopElementZIndexCategory::Window {
                elements_sorted_by_z_index.push(Rc::clone(&win) as Rc<dyn DesktopElement>);
            }
        }
        // Lastly, any other generic desktop views show up behind windows
        for elem in self.compositor_state.elements.iter() {
            if elem.z_index_category() == DesktopElementZIndexCategory::DesktopView {
                elements_sorted_by_z_index.push(Rc::clone(&elem));
            }
        }

        // Iterate backwards (from the furthest back to the foremost)
        for elem_idx in (0..elements_sorted_by_z_index.len()).rev() {
            //println!("\tProcessing idx #{elem_idx}, window {} (a split has {} elems, b split has {} elems)", elem.name(), a.len(), b.len());
            let elem = &elements_sorted_by_z_index[elem_idx];
            if !rect.intersects_with(elem.frame()) {
                //println!("\t\tDoes not intersect with provided rect, skipping");
                continue;
            }

            let (occluding_elems, _elem_and_lower) = elements_sorted_by_z_index.split_at(elem_idx);

            // Ensure drawable regions are never offscreen
            elem.set_drawable_rects(vec![self.desktop_frame.constrain(elem.frame())]);

            for occluding_elem in occluding_elems.iter().rev() {
                if !elem.frame().intersects_with(occluding_elem.frame()) {
                    continue;
                }
                //println!("\t\tOccluding {} by view with frame {}", elem.frame(), occluding_elem.frame());
                // Keep rects that don't intersect with the occluding elem
                let mut new_drawable_rects: Vec<Rect> = elem
                    .drawable_rects()
                    .iter()
                    .filter_map(|r| {
                        // If it does not intersect with the occluding element, we want to keep it
                        if !r.intersects_with(occluding_elem.frame()) {
                            Some(*r)
                        } else {
                            None
                        }
                    })
                    .collect();
                for rect in elem.drawable_rects() {
                    let mut visible_portions = rect.area_excluding_rect(occluding_elem.frame());
                    new_drawable_rects.append(&mut visible_portions);
                }
                //println!("\t\tSetting drawable_rects to {:?}", new_drawable_rects);
                elem.set_drawable_rects(new_drawable_rects);
            }

            if elem.drawable_rects().len() > 0 {
                //println!("\tQueueing composite for {}", window.name());
                self.compositor_state
                    .queue_composite(Rc::clone(elem) as Rc<dyn DesktopElement>);
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
        animated: bool,
    ) -> Rc<Window> {
        let source = source.to_string();
        println!("Creating window of size {:?} for {}", request.size, source);

        let content_size = Size::from(&request.size);

        let window_params = if awm_service_is_dock(&source) {
            WindowParams::new(
                false,
                false,
                false,
                DesktopElementZIndexCategory::FloatingWindow,
            )
        } else if awm_service_is_menu_bar(&source) {
            WindowParams::new(
                false,
                false,
                false,
                DesktopElementZIndexCategory::FloatingWindow,
            )
        } else {
            WindowParams::default()
        };

        // Generally, windows are larger than the content view to account for the title bar
        let window_size = Window::total_size_for_content_size(content_size, window_params);

        let new_window_origin = origin.unwrap_or({
            // Place the window in the center of the screen
            let res = self.desktop_frame.size;
            Point::new(
                (res.width / 2) - (window_size.width / 2),
                (res.height / 2) - (window_size.height / 2),
            )
        });

        let desktop_size = self.desktop_frame.size;
        let max_content_view_size =
            Window::content_size_for_total_size(desktop_size, window_params);
        #[cfg(target_os = "axle")]
        let content_view_layer = {
            // Ask the kernel to inform us when the remote end dies
            // This allows us to clean up windows even if the remote didn't exit cleanly
            AmcRegisterServiceDiedNotif::send(&source);

            // Ask the kernel to set up a shared memory mapping we'll use for the framebuffer
            // The framebuffer will be the screen size to allow window resizing
            let bytes_per_pixel = self.screen_buffer_layer.bytes_per_pixel();
            let shared_memory_size =
                max_content_view_size.width * max_content_view_size.height * bytes_per_pixel;
            println!(
                "Requesting shared memory of size {shared_memory_size} {max_content_view_size:?}"
            );
            let shared_memory_response =
                AmcSharedMemoryCreateRequest::send(&source, shared_memory_size as u32);

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
            SharedMemoryLayer::new(SingleFramebufferLayer::from_framebuffer(
                unsafe { Box::from_raw(framebuffer) },
                bytes_per_pixel,
                max_content_view_size,
                PixelByteLayout::RGBA,
            ))
        };
        #[cfg(not(target_os = "axle"))]
        let content_view_layer =
            SharedMemoryLayer::new(SingleFramebufferLayer::new(max_content_view_size));

        let window_frame = Rect::from_parts(new_window_origin, window_size);
        let new_window = Rc::new(Window::new(
            self.next_desktop_element_id(),
            &source,
            window_frame,
            content_view_layer,
            window_params,
            &self.window_images,
        ));
        new_window.redraw_title_bar();
        self.windows.insert(0, Rc::clone(&new_window));
        self.compositor_state
            .track_element(Rc::clone(&new_window) as Rc<dyn DesktopElement>);

        if !animated {
            self.recompute_drawable_regions_in_rect(window_frame);
        } else {
            let (initial_frame, final_frame) = if awm_service_is_dock(&source) {
                let dock_height = AWM_DOCK_HEIGHT;
                let initial_frame = Some(Rect::from_parts(
                    Point::new(0, desktop_size.height),
                    Size::new(desktop_size.width, dock_height),
                ));
                let final_frame = Rect::from_parts(
                    Point::new(0, desktop_size.height - dock_height),
                    Size::new(desktop_size.width, dock_height),
                );
                (initial_frame, final_frame)
            } else if awm_service_is_menu_bar(&source) {
                let menu_bar_height = AWM_MENU_BAR_HEIGHT;
                let initial_frame = Some(Rect::from_parts(
                    Point::new(0, -menu_bar_height),
                    Size::new(desktop_size.width, menu_bar_height),
                ));
                let final_frame = Rect::from_parts(
                    Point::new(0, 0),
                    Size::new(desktop_size.width, menu_bar_height),
                );
                (initial_frame, final_frame)
            } else {
                (None, window_frame)
            };
            self.start_animation(Animation::WindowOpen(WindowTransformAnimationParams::open(
                desktop_size,
                &new_window,
                200,
                initial_frame,
                final_frame,
            )));
        }

        // If this is a window other than the dock or menu bar, inform the dock
        if !awm_service_is_dock(&source) && !awm_service_is_menu_bar(&source) {
            inform_dock_window_created(new_window.id(), &source)
        }

        new_window
    }

    fn start_animation(&mut self, animation: Animation) {
        animation.start();
        self.ongoing_animations.push(animation);
    }

    pub fn step_animations(&mut self) {
        // Don't bother fetching a timestamp (which is a syscall) if not necessary
        if self.ongoing_animations.len() == 0 {
            return;
        }

        let now = get_timestamp();
        let mut rects_to_recompute_drawable_regions = vec![];
        for animation in self.ongoing_animations.iter() {
            let animation_damage = animation.step(now);
            for damaged_rect in animation_damage.rects_needing_composite.iter() {
                self.compositor_state.queue_full_redraw(*damaged_rect);
            }
            rects_to_recompute_drawable_regions
                .push(animation_damage.area_to_recompute_drawable_regions);
        }
        for r in rects_to_recompute_drawable_regions.drain(..) {
            self.recompute_drawable_regions_in_rect(r);
        }

        // Destroy all internal references to windows that are now fully closed
        let windows_to_drop: Vec<Rc<Window>> = self
            .ongoing_animations
            .iter()
            .map(|anim| {
                if anim.is_complete(now) {
                    if let Animation::WindowClose(params) = anim {
                        return Some(Rc::clone(&params.window));
                    }
                }
                None
            })
            .filter_map(|o| o)
            .collect();
        for w in windows_to_drop.into_iter() {
            self.drop_window(&w);
        }

        // Re-render desktop shortcuts that have landed in their new locations
        let shortcuts_to_refresh: Vec<Rc<DesktopShortcut>> = self
            .ongoing_animations
            .iter()
            .map(|anim| {
                if anim.is_complete(now) {
                    if let Animation::ShortcutSnap(params) = anim {
                        return Some(Rc::clone(&params.shortcut));
                    }
                }
                None
            })
            .filter_map(|o| o)
            .collect();
        for s in shortcuts_to_refresh.into_iter() {
            s.copy_desktop_background_slice(&mut self.desktop_background_layer);
            s.render();
            self.compositor_state
                .queue_composite(s as Rc<dyn DesktopElement>);
        }

        // Drop animations that are now complete
        self.ongoing_animations.retain(|anim| {
            if anim.is_complete(now) {
                anim.finish();
                false
            } else {
                true
            }
        });
    }

    fn window_containing_point(&self, p: Point) -> Option<Rc<Window>> {
        // Iterate from the topmost window to further back ones,
        // so if windows are overlapping the topmost window will receive it
        self.windows.iter().find_map(|window| {
            if window.frame().contains(p) {
                Some(Rc::clone(window))
            } else {
                None
            }
        })
    }

    fn desktop_element_in_category_containing_point(
        &self,
        p: Point,
        category: DesktopElementZIndexCategory,
    ) -> Option<Rc<dyn DesktopElement>> {
        self.compositor_state.elements.iter().find_map(|e| {
            if e.z_index_category() == category && e.frame().contains(p) {
                Some(Rc::clone(e))
            } else {
                None
            }
        })
    }

    fn shortcut_containing_point(&self, p: Point) -> Option<Rc<DesktopShortcut>> {
        let shortcuts = self.desktop_shortcuts_state.shortcuts.borrow();
        shortcuts.iter().find_map(|s| {
            if s.frame().contains(p) {
                Some(Rc::clone(&s))
            } else {
                None
            }
        })
    }

    fn windows_in_z_hierarchy(&self) -> impl Iterator<Item = &Rc<Window>> {
        // Don't include floating windows in the search. For example, this
        // will never return the dock window
        self.windows
            .iter()
            .filter(|w| w.z_index_category() == DesktopElementZIndexCategory::Window)
    }

    fn bottom_window(&self) -> Option<&Rc<Window>> {
        self.windows_in_z_hierarchy().last()
    }

    fn top_window(&self) -> Option<&Rc<Window>> {
        self.windows_in_z_hierarchy().next()
    }

    fn is_topmost_window(&self, w: &Rc<Window>) -> bool {
        if let Some(top_window) = self.top_window() {
            Rc::ptr_eq(top_window, w)
        } else {
            false
        }
    }

    fn handle_left_click_began(&mut self) {
        // Allow the mouse state to change based on the movement
        self.transition_to_mouse_interaction_state(
            self.mouse_interaction_state_for_mouse_state(true),
        );
        if let Some(window_under_mouse) = self.window_containing_point(self.mouse_state.pos) {
            if !self.is_topmost_window(&window_under_mouse) {
                println!(
                    "Moving clicked window to top: {}",
                    window_under_mouse.name()
                );
                self.move_window_to_top(&window_under_mouse);
            }

            if window_under_mouse.title_bar_buttons_hover_state()
                == TitleBarButtonsHoverState::HoverClose
            {
                // Close button clicked, send a close request
                send_close_window_request(&window_under_mouse);
            } else if window_under_mouse.title_bar_buttons_hover_state()
                == TitleBarButtonsHoverState::HoverMinimize
            {
                // Minimize button clicked, send a minimize request
                println!("Minimize button clicked!");
                send_initiate_window_minimize(&window_under_mouse);
            } else {
                // Only send the click if it was within the content view
                let window_frame = window_under_mouse.frame();
                let screen_space_content_frame = window_under_mouse
                    .content_frame()
                    .add_origin(window_frame.origin);
                if screen_space_content_frame.contains(self.mouse_state.pos) {
                    send_left_click_event(&window_under_mouse, self.mouse_state.pos)
                }
            }
        }
        // Note that we're excluding the normal windows Z-index from this search, as they're handled above
        else if let Some(elem_under_mouse) = self.desktop_element_in_category_containing_point(
            self.mouse_state.pos,
            DesktopElementZIndexCategory::DesktopView,
        ) {
            if elem_under_mouse.handle_left_click_began(self.mouse_state.pos)
                == MouseInteractionCallbackResult::RedrawRequested
            {
                self.recompute_drawable_regions_in_rect(elem_under_mouse.frame());
            }
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

    fn can_transition_to_shortcut_drag(&self, shortcut_under_mouse: &Rc<DesktopShortcut>) -> bool {
        if let MouseInteractionState::ShortcutHover(s) = &self.mouse_interaction_state {
            if Rc::ptr_eq(s, shortcut_under_mouse) {
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
        // Stay in drag states while the left click is ongoing
        if let MouseInteractionState::PerformingWindowDrag(_)
        | MouseInteractionState::ShortcutDrag(_)
        | MouseInteractionState::MouseDragWithinWindow(_) = &self.mouse_interaction_state
        {
            if self.mouse_state.left_click_down {
                return self.mouse_interaction_state.clone();
            }
        }

        // Choose a state primarily based on the mouse position
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
                if self.mouse_state.left_click_down {
                    MouseInteractionState::MouseDragWithinWindow(Rc::clone(&window_under_mouse))
                } else {
                    MouseInteractionState::WindowHover(Rc::clone(&window_under_mouse))
                }
            }
        } else if let Some(shortcut_under_mouse) =
            self.shortcut_containing_point(self.mouse_state.pos)
        {
            if self.can_transition_to_shortcut_drag(&shortcut_under_mouse) {
                MouseInteractionState::ShortcutDrag(Rc::clone(&shortcut_under_mouse))
            } else {
                MouseInteractionState::ShortcutHover(Rc::clone(&shortcut_under_mouse))
            }
        } else {
            MouseInteractionState::BackgroundHover
        }
    }

    fn handle_left_click_ended(&mut self) {
        // Simple case so handle it directly

        // First, figure out what new state we should transition to, if necessary
        let maybe_new_interaction_state = match &self.mouse_interaction_state {
            MouseInteractionState::PerformingWindowDrag(win) => {
                // End window drag
                Some(MouseInteractionState::HintingWindowDrag(Rc::clone(&win)))
            }
            MouseInteractionState::PerformingWindowResize(win) => {
                // End window resize
                Some(MouseInteractionState::HintingWindowResize(Rc::clone(&win)))
            }
            MouseInteractionState::MouseDragWithinWindow(_) => {
                // The user may have dragged the mouse to outside a window, so we can't necessarily
                // say that WindowHover is appropriate. We need to compute the correct state 'from scratch'.
                Some(self.mouse_interaction_state_for_mouse_state(false))
            }
            MouseInteractionState::ShortcutDrag(shortcut) => {
                Some(MouseInteractionState::ShortcutHover(Rc::clone(&shortcut)))
            }
            _ => None,
        };

        // Next, invoke any callbacks that are needed.
        // These steps are split up to satisfy the borrow checker.
        match &self.mouse_interaction_state {
            MouseInteractionState::WindowHover(win)
            | MouseInteractionState::MouseDragWithinWindow(win) => {
                send_left_click_ended_event(win, self.mouse_state.pos);
            }
            MouseInteractionState::ShortcutHover(shortcut) => {
                if shortcut.handle_left_click_ended(self.mouse_state.pos)
                    == MouseInteractionCallbackResult::RedrawRequested
                {
                    self.recompute_drawable_regions_in_rect(shortcut.frame());
                }
            }
            MouseInteractionState::ShortcutDrag(shortcut) => {
                let shortcut_clone = Rc::clone(&shortcut);
                if shortcut.handle_left_click_ended(self.mouse_state.pos)
                    == MouseInteractionCallbackResult::RedrawRequested
                {
                    self.recompute_drawable_regions_in_rect(shortcut.frame());
                }
                // Find a slot to snap this shortcut to
                let start_frame = shortcut_clone.frame();
                let new_slot_frame = self
                    .desktop_shortcuts_state
                    .transfer_shortcut_to_nearest_slot(&shortcut_clone);
                let new_shortcut_frame = start_frame.replace_origin(
                    DesktopShortcutsState::shortcut_origin_for_slot_frame(new_slot_frame),
                );
                self.start_animation(Animation::ShortcutSnap(ShortcutSnapAnimationParams::new(
                    &shortcut_clone,
                    new_shortcut_frame,
                    100,
                )));
            }
            _ => {}
        }

        // Set the new state we decided above, if necessary
        if let Some(new_interaction_state) = maybe_new_interaction_state {
            self.transition_to_mouse_interaction_state(new_interaction_state);
        }
    }

    /// Handles informing about mouse enter/exit if necessary given the state we're transferring to/from
    fn transition_to_mouse_interaction_state(&mut self, new_state: MouseInteractionState) {
        if self.mouse_interaction_state == new_state {
            return;
        }
        match &self.mouse_interaction_state {
            MouseInteractionState::BackgroundHover => {}
            MouseInteractionState::WindowHover(w)
            | MouseInteractionState::HintingWindowDrag(w)
            | MouseInteractionState::HintingWindowResize(w)
            | MouseInteractionState::PerformingWindowDrag(w)
            | MouseInteractionState::PerformingWindowResize(w)
            | MouseInteractionState::MouseDragWithinWindow(w) => {
                // If we're transitioning out of a window, inform it
                // Has the mouse left the window?
                let exited_window = match &new_state {
                    MouseInteractionState::BackgroundHover
                    | MouseInteractionState::ShortcutHover(_)
                    | MouseInteractionState::ShortcutDrag(_) => true,
                    MouseInteractionState::WindowHover(w2)
                    | MouseInteractionState::HintingWindowDrag(w2)
                    | MouseInteractionState::HintingWindowResize(w2)
                    | MouseInteractionState::PerformingWindowDrag(w2)
                    | MouseInteractionState::PerformingWindowResize(w2)
                    | MouseInteractionState::MouseDragWithinWindow(w2) => !Rc::ptr_eq(w, w2),
                };
                if exited_window {
                    // Ensure the previous window is informed that the mouse is no longer hovering in its title bar.
                    // We need to specify because the title bar hover state machinery can't figure out on its own,
                    // because it only looks at the mouse position, but not other windows in the Z-index.
                    // This means that if two windows overlapped with the same title-bar position, and the mouse
                    // moved into the higher one, both would otherwise retain the hovered state, which is incorrect.
                    self.transition_to_title_bar_hover_state_for_window(
                        w,
                        TitleBarButtonsHoverState::Unhovered,
                    );
                    send_mouse_exited_event(&w)
                }
            }
            MouseInteractionState::ShortcutHover(old_shortcut) => {
                // If we're transitioning out of a shortcut, inform it
                let exited_shortcut = match &new_state {
                    MouseInteractionState::ShortcutHover(new_shortcut)
                    | MouseInteractionState::ShortcutDrag(new_shortcut) => {
                        !Rc::ptr_eq(old_shortcut, new_shortcut)
                    }
                    _ => true,
                };
                if exited_shortcut {
                    if old_shortcut.handle_mouse_exited()
                        == MouseInteractionCallbackResult::RedrawRequested
                    {
                        self.recompute_drawable_regions_in_rect(old_shortcut.frame());
                    }
                }
            }
            MouseInteractionState::ShortcutDrag(old_shortcut) => {
                println!("Shortcut drag ended for \"{}\"", old_shortcut.name());
            }
        };

        // If we're transitioning into the title bar, send a mouse exited event
        if let MouseInteractionState::WindowHover(w)
        | MouseInteractionState::HintingWindowResize(w) = &self.mouse_interaction_state
        {
            if let MouseInteractionState::HintingWindowDrag(w2) = &new_state {
                if Rc::ptr_eq(w, w2) {
                    //self.transition_title_bar_hover_state_for_window(w);
                    println!("Sending mouse exited event!");
                    send_mouse_exited_event(&w)
                }
            }
        }

        // If we're transitioning into a window, inform it
        if let MouseInteractionState::WindowHover(new_win)
        | MouseInteractionState::HintingWindowDrag(new_win)
        | MouseInteractionState::HintingWindowResize(new_win)
        | MouseInteractionState::PerformingWindowDrag(new_win)
        | MouseInteractionState::PerformingWindowResize(new_win)
        | MouseInteractionState::MouseDragWithinWindow(new_win) = &new_state
        {
            let did_enter_new_window = match &self.mouse_interaction_state {
                MouseInteractionState::BackgroundHover
                | MouseInteractionState::ShortcutHover(_)
                | MouseInteractionState::ShortcutDrag(_) => true,
                MouseInteractionState::WindowHover(old_win)
                | MouseInteractionState::HintingWindowDrag(old_win)
                | MouseInteractionState::HintingWindowResize(old_win)
                | MouseInteractionState::PerformingWindowDrag(old_win)
                | MouseInteractionState::PerformingWindowResize(old_win)
                | MouseInteractionState::MouseDragWithinWindow(old_win) => {
                    // Is the new state not about the same window?
                    !Rc::ptr_eq(new_win, old_win)
                }
            };
            if did_enter_new_window {
                send_mouse_entered_event(&new_win)
            }
        }

        // If we're transitioning into a shortcut, inform it
        if let MouseInteractionState::ShortcutHover(new_shortcut)
        | MouseInteractionState::ShortcutDrag(new_shortcut) = &new_state
        {
            let did_enter_shortcut = match &self.mouse_interaction_state {
                MouseInteractionState::ShortcutHover(old_shortcut)
                | MouseInteractionState::ShortcutDrag(old_shortcut) => {
                    // Is the new shortcut the same as the old shortcut?
                    !Rc::ptr_eq(new_shortcut, old_shortcut)
                }
                _ => true,
            };
            if did_enter_shortcut {
                if new_shortcut.handle_mouse_entered()
                    == MouseInteractionCallbackResult::RedrawRequested
                {
                    self.recompute_drawable_regions_in_rect(new_shortcut.frame());
                }
            }
        } else if let MouseInteractionState::ShortcutDrag(shortcut) = &new_state {
            println!("Started shortcut drag for \"{}\"", shortcut.name());
        }

        self.mouse_interaction_state = new_state;
    }

    fn title_bar_hover_state_for_window(&self, window: &Rc<Window>) -> TitleBarButtonsHoverState {
        let mouse_within_window = window.frame().translate_point(self.mouse_state.pos);
        if window.is_point_within_close_button(mouse_within_window) {
            TitleBarButtonsHoverState::HoverClose
        } else if window.is_point_within_minimize_button(mouse_within_window) {
            TitleBarButtonsHoverState::HoverMinimize
        } else if window.is_point_within_title_bar(mouse_within_window) {
            TitleBarButtonsHoverState::HoverBackground
        } else {
            TitleBarButtonsHoverState::Unhovered
        }
    }

    fn transition_title_bar_hover_state_for_window(&self, window: &Rc<Window>) {
        let new_state = self.title_bar_hover_state_for_window(window);
        self.transition_to_title_bar_hover_state_for_window(window, new_state);
    }

    fn transition_to_title_bar_hover_state_for_window(
        &self,
        window: &Rc<Window>,
        new_state: TitleBarButtonsHoverState,
    ) {
        let should_redraw_title_bar = new_state != window.title_bar_buttons_hover_state();
        window.set_title_bar_buttons_hover_state(new_state);
        if should_redraw_title_bar {
            let title_bar_frame = window.redraw_title_bar();
            self.compositor_state.queue_full_redraw(title_bar_frame);
        }
    }

    fn handle_mouse_moved(&mut self, _new_pos: Point, rel_shift: Point) {
        if let MouseInteractionState::PerformingWindowDrag(dragged_window) =
            &self.mouse_interaction_state
        {
            // We're in the middle of dragging a window
            let prev_frame = dragged_window.frame();
            // Bind the window to the area that windows can be placed within
            let new_frame = self.bind_rect_to_windows_region(
                dragged_window
                    .frame()
                    .replace_origin(dragged_window.frame().origin + rel_shift),
            );

            dragged_window.set_frame(new_frame);
            self.queue_compositor_updates_for_old_and_new_element_frame(prev_frame, new_frame);
            return;
        } else if let MouseInteractionState::MouseDragWithinWindow(window_owning_drag) =
            &self.mouse_interaction_state
        {
            // We're performing a drag within the content view of a window
            send_mouse_dragged_event(&window_owning_drag, self.mouse_state.pos);
            return;
        } else if let MouseInteractionState::PerformingWindowResize(resized_window) =
            &self.mouse_interaction_state
        {
            // We're in the middle of resizing a window
            let old_frame = resized_window.frame();
            let mut new_frame = self.bind_rect_to_windows_region(
                old_frame.replace_size(old_frame.size + Size::new(rel_shift.x, rel_shift.y)),
            );
            // Don't let the window get too small
            new_frame.size.width = max(new_frame.size.width, 200);
            new_frame.size.height = max(new_frame.size.height, 200);
            resized_window.set_frame(new_frame);
            //println!("Set window to {new_frame}");
            send_window_resized_event(&resized_window);

            resized_window.redraw_title_bar();
            self.queue_compositor_updates_for_old_and_new_element_frame(old_frame, new_frame);
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
            self.transition_title_bar_hover_state_for_window(window_under_mouse);
            send_mouse_moved_event(window_under_mouse, self.mouse_state.pos);
        } else if let MouseInteractionState::HintingWindowDrag(window) =
            &self.mouse_interaction_state
        {
            self.transition_title_bar_hover_state_for_window(window);
        } else if let MouseInteractionState::ShortcutHover(shortcut) = &self.mouse_interaction_state
        {
            if shortcut.handle_mouse_moved(self.mouse_state.pos)
                == MouseInteractionCallbackResult::RedrawRequested
            {
                self.recompute_drawable_regions_in_rect(shortcut.frame());
            }
        } else if let MouseInteractionState::ShortcutDrag(shortcut) = &self.mouse_interaction_state
        {
            // Update the position of the shortcut based on the mouse movement
            let prev_frame = shortcut.frame();
            // Bind the shortcut to the screen size
            let new_frame = self.bind_rect_to_screen_size(
                shortcut
                    .frame()
                    .replace_origin(shortcut.frame().origin + rel_shift),
            );
            shortcut.set_frame(new_frame);
            shortcut.copy_desktop_background_slice(&mut self.desktop_background_layer);
            self.queue_compositor_updates_for_old_and_new_element_frame(prev_frame, new_frame);
        }
    }

    fn queue_compositor_updates_for_old_and_new_element_frame(
        &mut self,
        old_elem_frame: Rect,
        new_elem_frame: Rect,
    ) {
        let update_rect = old_elem_frame.union(new_elem_frame);
        self.recompute_drawable_regions_in_rect(update_rect);
        if new_elem_frame.intersects_with(old_elem_frame) {
            // PT: area_excluding_rect() can only give sane results when the input rects overlap
            for diff_rect in old_elem_frame.area_excluding_rect(new_elem_frame) {
                self.compositor_state.queue_full_redraw(diff_rect);
            }
        } else {
            self.compositor_state.queue_full_redraw(update_rect);
        }
    }

    fn handle_mouse_scrolled(&mut self, delta_z: i8) {
        if let MouseInteractionState::WindowHover(hover_window) = &self.mouse_interaction_state {
            // Scroll within a window, inform the window
            send_mouse_scrolled_event(hover_window, self.mouse_state.pos, delta_z);
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

    fn bind_rect_to_windows_region(&self, r: Rect) -> Rect {
        let mut out = r;
        let desktop_size = self.desktop_frame.size;
        out.origin.x = max(r.origin.x, 0_isize);
        out.origin.y = max(r.origin.y, AWM_MENU_BAR_HEIGHT);
        if out.max_x() > desktop_size.width {
            let overhang = out.max_x() - desktop_size.width;
            if out.origin.x >= overhang {
                out.origin.x -= overhang;
            } else {
                out.size.width -= overhang;
            }
        }
        if out.max_y() > desktop_size.height - AWM_DOCK_HEIGHT {
            let overhang = out.max_y() - (desktop_size.height - AWM_DOCK_HEIGHT);
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
        self.recompute_drawable_regions_in_rect(window.frame());
    }

    pub fn handle_keyboard_event(&mut self, packet: &KeyboardPacket) {
        // Update held modifiers state
        if let Ok(key_identifier) = KeyIdentifier::try_from(packet.key) {
            let modifier_key = match key_identifier {
                KeyIdentifier::ShiftLeft | KeyIdentifier::ShiftRight => {
                    Some(KeyboardModifier::Shift)
                }
                KeyIdentifier::ControlLeft => Some(KeyboardModifier::Control),
                KeyIdentifier::CommandLeft => Some(KeyboardModifier::Command),
                KeyIdentifier::OptionLeft => Some(KeyboardModifier::Option),
                _ => None,
            };
            if let Some(modifier_key) = modifier_key {
                match packet.event_type {
                    KeyEventType::Pressed => {
                        //println!("Inserting {modifier_key:?}");
                        self.keyboard_state.pressed_modifiers.insert(modifier_key);
                    }
                    KeyEventType::Released => {
                        //println!("Removing {modifier_key:?}");
                        self.keyboard_state.pressed_modifiers.remove(&modifier_key);
                    }
                }
            }
        }
        // Is this a chord?
        if packet.event_type == KeyEventType::Pressed && self.keyboard_state.is_control_held() {
            //println!("Control is held, checking {}\n", packet.key);
            match packet.key {
                const { '\t' as u32 } => {
                    // Ctrl+Tab switches windows by rotating the Z-order
                    println!("Found Ctrl+Tab\n");
                    if let Some(bottom_window) = self.bottom_window() {
                        self.move_window_to_top(&Rc::clone(bottom_window));
                    }
                }
                const { 'w' as u32 } => {
                    // Ctrl+W closes the topmost window
                    println!("Found Ctrl+W\n");
                    if let Some(top_window) = self.top_window() {
                        send_close_window_request(&Rc::clone(top_window));
                    }
                }
                const { 'a' as u32 } => {
                    // Ctrl+W closes the topmost window
                    println!("Found Ctrl+W\n");
                    panic!("Intentional panic to force a red rectangle of death");
                }
                _ => {}
            }
        }

        // Send the event to the topmost window
        if let Some(top_window) = self.top_window() {
            match packet.event_type {
                KeyEventType::Pressed => send_key_down_event(&Rc::clone(top_window), packet.key),
                KeyEventType::Released => send_key_up_event(&Rc::clone(top_window), packet.key),
            }
            //
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
            //println!("Ignoring extra draw request for {}", window.name());
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
        self.compositor_state.queue_extra_draw(
            Rc::clone(&window) as Rc<dyn DesktopElement>,
            title_bar_frame,
        );

        // Inform the dock
        inform_dock_window_title_updated(window.id(), new_title);
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

    pub fn has_ongoing_animations(&self) -> bool {
        self.ongoing_animations.len() > 0
    }

    pub fn handle_preferences_updated(&mut self, msg: &PreferencesUpdated) {
        self.background_gradient_outer_color = Color::from(msg.to);
        self.background_gradient_inner_color = Color::from(msg.from);
        self.draw_background();

        // Redraw desktop shortcuts so they render using the new background
        self.desktop_shortcuts_state.update_background(
            &mut self.desktop_background_layer,
            self.background_gradient_outer_color,
        );

        self.compositor_state.queue_full_redraw(self.desktop_frame);
    }

    fn window_with_id(&self, window_id: usize) -> Option<Rc<Window>> {
        for w in self.windows.iter() {
            if w.id() == window_id {
                return Some(Rc::clone(w));
            }
        }
        None
    }

    pub fn handle_dock_task_view_clicked(&mut self, msg: &AwmDockTaskViewClicked) {
        let window = self.window_with_id(msg.window_id as usize).unwrap();
        self.move_window_to_top(&window);

        // Unminimize if necessary
        if window.is_minimized() {
            self.start_animation(Animation::WindowUnminimize(
                WindowTransformAnimationParams::unminimize(&window, 200),
            ));
        }
    }

    pub fn handle_window_close(&mut self, window_owner: &str) {
        let window = self.window_for_owner(window_owner);
        self.start_animation(Animation::WindowClose(
            WindowTransformAnimationParams::close(self.desktop_frame.size, &window, 200),
        ));
        // Inform the dock immediately, as it's snappier than waiting for the animation to finish
        inform_dock_window_closed(window.id());
    }

    fn drop_window(&mut self, window: &Rc<Window>) {
        println!("drop_window({})", window.name());
        let window_as_desktop_elem = Rc::clone(&window) as Rc<dyn DesktopElement>;
        // Remove the window from the desktop tree
        self.windows.retain(|w| !Rc::ptr_eq(&window, w));
        self.transition_to_mouse_interaction_state(
            self.mouse_interaction_state_for_mouse_state(false),
        );
        self.windows_to_render_remote_layers_this_cycle
            .retain(|w| !Rc::ptr_eq(&window, w));
        // Remove the window from the compositor tree
        self.compositor_state
            .elements
            .retain(|elem| !Rc::ptr_eq(&window_as_desktop_elem, elem));
        self.compositor_state
            .elements_by_id
            .retain(|elem_id, elem| window.id() != elem.id());
        self.compositor_state
            .elements_to_composite
            .borrow_mut()
            .retain(|&elem_id| window.id() != elem_id);

        let window_frame = window.frame();
        self.recompute_drawable_regions_in_rect(window_frame);
        self.compositor_state.queue_full_redraw(window_frame);
    }

    pub fn handle_minimize_window_to_dock(&mut self, info: &AwmDockWindowMinimizeWithInfo) {
        let window = self.window_with_id(info.window_id as _).unwrap();
        let task_view_frame = Rect::from(info.task_view_frame);
        let dest_frame = Rect::from_parts(
            Point::new(task_view_frame.min_x(), self.desktop_frame.height()),
            task_view_frame.size,
        );
        self.start_animation(Animation::WindowMinimize(
            WindowTransformAnimationParams::minimize(&window, 200, dest_frame),
        ));
    }

    pub fn handle_amc_service_died_notif(&mut self, notif: &AmcServiceDiedNotif) {
        let dead_window_owner = str_from_u8_nul_utf8_unchecked(&notif.dead_service);
        println!("Core informed awm that {dead_window_owner} has died");
        self.handle_window_close(dead_window_owner);
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
        desktop.spawn_window(
            name,
            &AwmCreateWindow::new(frame.size),
            Some(frame.origin),
            false,
        )
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
                            frame.height() - Window::DEFAULT_TITLE_BAR_HEIGHT as isize,
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

        /*
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
        */

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

        assert_window_layouts_matches_drawable_rects(
            vec![Rect::new(0, 998, 1000, 32)],
            vec![vec![Rect::new(0, 998, 1000, 2)]],
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
        let extra_draws_by_elem_name: BTreeMap<String, Vec<Rect>> =
            BTreeMap::from_iter(desktop.compositor_state.extra_draws.borrow().iter().map(
                |(e_id, rects)| {
                    let elem = desktop.compositor_state.elements_by_id.get(e_id).unwrap();
                    (elem.name(), rects.iter().map(|r| *r).collect())
                },
            ));
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
