use crate::bitmap::BitmapImage;
use crate::desktop::{Desktop, DesktopElement, DesktopElementZIndexCategory};
use crate::println;
use agx_definitions::{
    Color, Layer, LikeLayerSlice, Point, Rect, SingleFramebufferLayer, Size, StrokeThickness,
};
use alloc::boxed::Box;
use alloc::rc::Rc;
use alloc::string::String;
use alloc::string::ToString;
use alloc::vec;
use alloc::vec::Vec;
use core::cell::RefCell;

#[derive(Debug)]
pub struct DesktopShortcut {
    id: usize,
    frame: Rect,
    drawable_rects: RefCell<Vec<Rect>>,
    layer: RefCell<SingleFramebufferLayer>,
    icon: BitmapImage,
    path: String,
    title: String,
    first_click_start_time: Option<usize>,
}

impl DesktopShortcut {
    fn new(id: usize, icon: &BitmapImage, origin: Point, path: &str, title: &str) -> Self {
        let frame = Rect::from_parts(origin, Self::size());
        Self {
            id,
            frame,
            drawable_rects: RefCell::new(vec![]),
            layer: RefCell::new(SingleFramebufferLayer::new(frame.size)),
            icon: icon.clone(),
            path: path.to_string(),
            title: title.to_string(),
            first_click_start_time: None,
        }
    }

    fn size() -> Size {
        Size::new(100, 65)
    }

    fn is_in_soft_click(&self) -> bool {
        self.first_click_start_time.is_some()
    }

    fn render(&self, desktop_background: &mut Box<SingleFramebufferLayer>) {
        let slice = self.layer.borrow_mut().get_full_slice();

        // Start off by rendering the background of the shortcut background layer's content
        // If we're not in a soft-click, this will just display the desktop background
        if self.is_in_soft_click() {
            slice.fill(Color::new(127, 127, 255));
        } else {
            let background_slice = desktop_background.get_slice(self.frame);
            slice.blit2(&background_slice);
        }

        /*
        slice.fill_rect(
            Rect::with_size(slice.frame().size),
            Color::dark_gray(),
            StrokeThickness::Width(2),
        );
        */

        let image_size = self.frame.size - Size::new(28, 16);
        let icon_margin = Size::new(self.frame.width() - image_size.width, 4);
        let label_height = self.frame.height() - image_size.height;

        // Render the shortcut icon
        self.icon
            .render(&self.layer.borrow_mut().get_slice(Rect::from_parts(
                Point::new(
                    (icon_margin.width as f64 / 2.0) as isize,
                    (icon_margin.height as f64 / 2.0) as isize,
                ),
                image_size,
            )));

        // Render the title label
        let label_mid = Point::new(
            (self.frame.width() as f64 / 2.0) as isize,
            (self.frame.height() as f64 - (label_height as f64 / 2.0)) as isize,
        );
        let font_size = Size::new(8, 10);
        let title_len = self.title.len();
        let label_origin = Point::new(
            label_mid.x - (((font_size.width * title_len as isize) as f64 / 2.0) as isize),
            label_mid.y - ((font_size.height as f64 / 2.0) as isize),
        );
        let text_color = Color::new(50, 50, 50);

        let mut cursor = label_origin;
        for ch in self.title.chars() {
            slice.draw_char(ch, cursor, text_color, font_size);
            cursor.x += font_size.width;
        }

        // If the background gradient is too dark, set the shortcuts text color to white so it's always visible.
        // Per ITU-R BT.709
        /*
        Color outer_bg = background_gradient_outer_color();
        double luma = (0.2126 * outer_bg.val[0]) + (0.7152 * outer_bg.val[1]) + (0.0722 * outer_bg.val[2]);
        if (luma < 64) {
            text_color = color_make(205, 205, 205);
        }
        */
    }
}

impl DesktopElement for DesktopShortcut {
    fn id(&self) -> usize {
        self.id
    }

    fn frame(&self) -> Rect {
        self.frame
    }

    fn name(&self) -> String {
        self.title.clone()
    }

    fn drawable_rects(&self) -> Vec<Rect> {
        self.drawable_rects.borrow().clone()
    }

    fn set_drawable_rects(&self, drawable_rects: Vec<Rect>) {
        *self.drawable_rects.borrow_mut() = drawable_rects
    }

    fn get_slice(&self) -> Box<dyn LikeLayerSlice> {
        self.layer.borrow_mut().get_full_slice()
    }

    fn z_index_category(&self) -> DesktopElementZIndexCategory {
        DesktopElementZIndexCategory::DesktopView
    }
}

#[derive(Debug)]
struct DesktopShortcutGridSlot {
    frame: Rect,
    occupant: RefCell<Option<Rc<DesktopShortcut>>>,
}

impl DesktopShortcutGridSlot {
    fn new(origin: Point) -> Self {
        Self {
            frame: Rect::from_parts(origin, Self::size()),
            occupant: RefCell::new(None),
        }
    }

    fn size() -> Size {
        DesktopShortcut::size() + Size::new(16, 30)
    }

    fn set_occupant(&self, shortcut: Option<Rc<DesktopShortcut>>) {
        *self.occupant.borrow_mut() = shortcut;
    }
}

pub struct DesktopShortcutsState {
    slots: Vec<DesktopShortcutGridSlot>,
}

impl DesktopShortcutsState {
    pub fn new(desktop_size: Size) -> Self {
        let mut slots = vec![];
        let grid_slot_size = DesktopShortcutGridSlot::size();
        // Iterate by columns so when searching for a free space linearly we fill in columns first
        for x in
            (0..(desktop_size.width - grid_slot_size.width)).step_by(grid_slot_size.width as usize)
        {
            for y in (0..(desktop_size.height - grid_slot_size.height))
                .step_by(grid_slot_size.height as usize)
            {
                slots.push(DesktopShortcutGridSlot::new(Point::new(x, y)));
            }
        }
        Self { slots }
    }

    pub fn add_shortcut(
        &self,
        background_layer: &mut Box<SingleFramebufferLayer>,
        id: usize,
        icon: &BitmapImage,
        path: &str,
        title: &str,
    ) -> Rc<DesktopShortcut> {
        // Find the next empty grid slot
        if let Some(found_slot) = self.slots.iter().find(|s| s.occupant.borrow().is_none()) {
            println!("Found empty grid slot {found_slot:?}");
            let shortcut_size = DesktopShortcut::size();
            let shortcut_origin = Point::new(
                found_slot.frame.mid_x() - ((shortcut_size.width as f64 / 2.0) as isize),
                found_slot.frame.mid_y() - ((shortcut_size.height as f64 / 2.0) as isize),
            );
            let new_shortcut =
                Rc::new(DesktopShortcut::new(id, icon, shortcut_origin, path, title));
            found_slot.set_occupant(Some(Rc::clone(&new_shortcut)));
            new_shortcut.render(background_layer);
            return new_shortcut;
        } else {
            panic!("Failed to find a free slot to place another desktop shortcut");
        }
    }
}
