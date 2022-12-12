use crate::desktop::DesktopElement;
use agx_definitions::{
    Color, Layer, LikeLayerSlice, Point, Rect, RectInsets, SingleFramebufferLayer, Size,
};
use alloc::boxed::Box;
use alloc::string::String;
use alloc::string::ToString;
use alloc::vec;
use alloc::vec::Vec;
use core::cell::RefCell;
use core::fmt::{Display, Formatter};

pub struct Window {
    id: usize,
    pub frame: RefCell<Rect>,
    drawable_rects: RefCell<Vec<Rect>>,
    pub owner_service: String,
    layer: RefCell<SingleFramebufferLayer>,
    pub content_layer: RefCell<SingleFramebufferLayer>,
    title: RefCell<Option<String>>,
}

impl Window {
    pub const TITLE_BAR_HEIGHT: usize = 30;

    pub fn new(
        id: usize,
        owner_service: &str,
        frame: Rect,
        content_layer: SingleFramebufferLayer,
    ) -> Self {
        let total_size = Self::total_size_for_content_size(content_layer.size());
        Self {
            id,
            frame: RefCell::new(frame),
            drawable_rects: RefCell::new(vec![]),
            owner_service: owner_service.to_string(),
            layer: RefCell::new(SingleFramebufferLayer::new(total_size)),
            content_layer: RefCell::new(content_layer),
            title: RefCell::new(None),
        }
    }

    pub fn set_frame(&self, frame: Rect) {
        *self.frame.borrow_mut() = frame;
    }

    pub fn set_title(&self, new_title: &str) {
        *self.title.borrow_mut() = Some(new_title.to_string())
    }

    pub fn is_point_within_resize_inset(&self, local_point: Point) -> bool {
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

    pub fn is_point_within_title_bar(&self, local_point: Point) -> bool {
        self.title_bar_frame()
            .replace_origin(Point::zero())
            .contains(local_point)
    }

    pub fn content_frame(&self) -> Rect {
        Rect::from_parts(
            Point::new(0, Self::TITLE_BAR_HEIGHT as isize),
            Size::new(
                self.frame().width(),
                self.frame().height() - (Self::TITLE_BAR_HEIGHT as isize),
            ),
        )
    }

    pub fn redraw_title_bar(&self) -> Rect {
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

    pub fn total_size_for_content_size(content_size: Size) -> Size {
        Size::new(
            content_size.width,
            content_size.height + Self::TITLE_BAR_HEIGHT as isize,
        )
    }

    pub fn content_size_for_total_size(total_size: Size) -> Size {
        Size::new(
            total_size.width,
            total_size.height - Self::TITLE_BAR_HEIGHT as isize,
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
