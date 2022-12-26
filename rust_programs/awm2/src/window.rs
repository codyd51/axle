use crate::desktop::DesktopElement;
use agx_definitions::{
    Color, Layer, LikeLayerSlice, Point, Rect, RectInsets, SingleFramebufferLayer, Size,
    StrokeThickness,
};
use alloc::boxed::Box;
use alloc::string::String;
use alloc::string::ToString;
use alloc::vec;
use alloc::vec::Vec;
use core::cell::RefCell;
use core::fmt::{Display, Formatter};
use core::mem;

use crate::println;

#[derive(Copy, Clone, PartialEq, Debug)]
pub enum TitleBarButtonsHoverState {
    Unhovered,
    HoverClose,
    HoverMinimize,
}

#[derive(Copy, Clone, Debug)]
pub struct WindowParams {
    has_title_bar: bool,
    is_resizable: bool,
    is_draggable: bool,
}

impl WindowParams {
    pub fn new(has_title_bar: bool, is_resizable: bool, is_draggable: bool) -> Self {
        Self {
            has_title_bar,
            is_resizable,
            is_draggable,
        }
    }

    fn title_bar_height(&self) -> usize {
        match self.has_title_bar {
            true => Window::DEFAULT_TITLE_BAR_HEIGHT,
            false => 0,
        }
    }
}

impl Default for WindowParams {
    fn default() -> Self {
        WindowParams::new(true, true, true)
    }
}

pub struct SharedMemoryLayer(SingleFramebufferLayer);

impl SharedMemoryLayer {
    pub fn new(layer: SingleFramebufferLayer) -> Self {
        Self(layer)
    }

    pub fn copy_from(&self, other: &SingleFramebufferLayer) {
        self.0.copy_from(other)
    }

    pub fn get_full_slice(&mut self) -> Box<dyn LikeLayerSlice> {
        self.0.get_full_slice()
    }
}

impl Layer for SharedMemoryLayer {
    fn size(&self) -> Size {
        self.0.size()
    }

    fn bytes_per_pixel(&self) -> isize {
        self.0.bytes_per_pixel()
    }

    fn fill_rect(&self, rect: &Rect, color: Color) {
        self.0.fill_rect(rect, color)
    }

    fn putpixel(&self, loc: &Point, color: Color) {
        self.0.putpixel(loc, color)
    }

    fn get_slice(&mut self, rect: Rect) -> Box<dyn LikeLayerSlice> {
        self.0.get_slice(rect)
    }
}

impl Drop for SharedMemoryLayer {
    fn drop(&mut self) {
        #[cfg(target_os = "axle")]
        {
            // This memory is backed by a shared memory region, and isn't on the heap.
            // Don't try to free it.
            let shmem_region =
                mem::replace(&mut *self.0.framebuffer.borrow_mut(), Box::new([0; 0]));
            mem::forget(shmem_region);
        }
        #[cfg(not(target_os = "axle"))]
        {
            println!("Freeing SharedMemoryLayer because we're in a hosted target")
        }
    }
}

pub struct Window {
    id: usize,
    pub frame: RefCell<Rect>,
    drawable_rects: RefCell<Vec<Rect>>,
    pub owner_service: String,
    layer: RefCell<SingleFramebufferLayer>,
    pub content_layer: RefCell<SharedMemoryLayer>,
    title: RefCell<Option<String>>,
    title_bar_buttons_hover_state: RefCell<TitleBarButtonsHoverState>,
    params: WindowParams,
    title_bar_height: usize,
}

impl Window {
    pub const DEFAULT_TITLE_BAR_HEIGHT: usize = 30;

    pub fn new(
        id: usize,
        owner_service: &str,
        frame: Rect,
        content_layer: SharedMemoryLayer,
        params: WindowParams,
    ) -> Self {
        let total_size = Self::total_size_for_content_size(content_layer.size(), params);
        Self {
            id,
            frame: RefCell::new(frame),
            drawable_rects: RefCell::new(vec![]),
            owner_service: owner_service.to_string(),
            layer: RefCell::new(SingleFramebufferLayer::new(total_size)),
            content_layer: RefCell::new(content_layer),
            title: RefCell::new(None),
            title_bar_buttons_hover_state: RefCell::new(TitleBarButtonsHoverState::Unhovered),
            params,
            title_bar_height: params.title_bar_height(),
        }
    }

    pub fn set_frame(&self, frame: Rect) {
        *self.frame.borrow_mut() = frame;
    }

    pub fn set_title(&self, new_title: &str) {
        *self.title.borrow_mut() = Some(new_title.to_string())
    }

    pub fn set_title_bar_buttons_hover_state(&self, state: TitleBarButtonsHoverState) {
        *self.title_bar_buttons_hover_state.borrow_mut() = state
    }

    pub fn title_bar_buttons_hover_state(&self) -> TitleBarButtonsHoverState {
        *self.title_bar_buttons_hover_state.borrow()
    }

    pub fn is_point_within_resize_inset(&self, local_point: Point) -> bool {
        if !self.params.is_resizable {
            return false;
        }

        let grabber_inset = 8;
        let content_frame_past_inset = self
            .content_frame()
            .inset_by_insets(RectInsets::uniform(grabber_inset));
        !content_frame_past_inset.contains(local_point)
    }

    fn title_bar_frame(&self) -> Rect {
        Rect::with_size(Size::new(
            self.frame().width(),
            self.title_bar_height as isize,
        ))
    }

    pub fn close_button_frame(&self) -> Rect {
        let icon_height = ((self.title_bar_height as f64) * 0.8) as isize;
        let icon_size = Size::new(16, 16);
        Rect::from_parts(
            Point::new((icon_height as f64 * 1.25) as isize, 5),
            icon_size,
        )
    }

    pub fn is_point_within_title_bar(&self, local_point: Point) -> bool {
        if !self.params.has_title_bar {
            return false;
        }

        self.title_bar_frame()
            .replace_origin(Point::zero())
            .contains(local_point)
    }

    pub fn is_point_within_close_button(&self, local_point: Point) -> bool {
        if !self.params.has_title_bar {
            return false;
        }

        self.close_button_frame().contains(local_point)
    }

    pub fn content_frame(&self) -> Rect {
        Rect::from_parts(
            Point::new(0, self.title_bar_height as isize),
            Size::new(
                self.frame().width(),
                self.frame().height() - (self.title_bar_height as isize),
            ),
        )
    }

    pub fn redraw_title_bar(&self) -> Rect {
        if !self.params.has_title_bar {
            return Rect::with_origin(self.frame.borrow().origin);
        }
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

        self.redraw_close_button();

        title_bar_frame.replace_origin(self.frame.borrow().origin)
    }

    pub fn redraw_close_button(&self) -> Rect {
        let title_bar_frame = self.title_bar_frame();
        let title_bar_slice = self.layer.borrow_mut().get_slice(title_bar_frame);

        let close_button_frame = self.close_button_frame();
        if *self.title_bar_buttons_hover_state.borrow() == TitleBarButtonsHoverState::HoverClose {
            title_bar_slice.fill_rect(close_button_frame, Color::red(), StrokeThickness::Filled);
        } else {
            title_bar_slice.fill_rect(
                close_button_frame,
                Color::new(255, 200, 200),
                StrokeThickness::Filled,
            );
        }
        title_bar_slice.fill_rect(
            close_button_frame,
            Color::black(),
            StrokeThickness::Width(1),
        );
        close_button_frame
    }

    pub fn render_remote_layer(&self) {
        let src = self.content_layer.borrow_mut().get_full_slice();
        let dst = self.layer.borrow_mut().get_slice(Rect::from_parts(
            Point::new(0, self.title_bar_height as isize),
            src.frame().size,
        ));
        dst.blit2(&src);
    }

    pub fn total_size_for_content_size(content_size: Size, params: WindowParams) -> Size {
        Size::new(
            content_size.width,
            content_size.height + params.title_bar_height() as isize,
        )
    }

    pub fn content_size_for_total_size(total_size: Size, params: WindowParams) -> Size {
        Size::new(
            total_size.width,
            total_size.height - params.title_bar_height() as isize,
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
    fn id(&self) -> usize {
        self.id
    }

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
