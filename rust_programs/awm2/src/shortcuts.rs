use crate::bitmap::BitmapImage;
use crate::desktop::{
    Desktop, DesktopElement, DesktopElementZIndexCategory, MouseInteractionCallbackResult,
};
use crate::events::request_program_launch;
use crate::println;
use crate::utils::get_timestamp;
use agx_definitions::{
    Color, Layer, LikeLayerSlice, Point, Rect, SingleFramebufferLayer, Size, StrokeThickness,
};
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::rc::Rc;
use alloc::string::String;
use alloc::string::ToString;
use alloc::vec;
use alloc::vec::Vec;
use axle_rt::AmcMessage;
use axle_rt::{amc_message_await__u32_event, amc_message_send};
use core::cell::RefCell;
use core::ptr;
use dock_messages::AWM_DOCK_HEIGHT;
use file_manager_messages::{ReadFile, ReadFileResponse, FILE_SERVER_SERVICE_NAME};
use lazy_static::lazy_static;
use menu_bar_messages::AWM_MENU_BAR_HEIGHT;
use spin::Mutex;
use ttf_renderer::{render_char_onto, Font};

#[cfg(not(target_os = "axle"))]
use std::fs;

lazy_static! {
    static ref FONT: spin::Mutex<Font> = Mutex::new(load_font("fonts/sf_pro.ttf"));
}

pub fn load_font(path: &str) -> Font {
    println!("Loading font {path}...");
    let font_bytes = {
        #[cfg(target_os = "axle")]
        {
            let file_read_request = ReadFile::new(path);
            amc_message_send(FILE_SERVER_SERVICE_NAME, file_read_request);
            let file_data_msg: AmcMessage<ReadFileResponse> =
                amc_message_await__u32_event(FILE_SERVER_SERVICE_NAME);
            let file_data_body = file_data_msg.body();
            unsafe {
                let data_slice = ptr::slice_from_raw_parts(
                    (&file_data_body.data) as *const u8,
                    file_data_body.len,
                );
                let data: &[u8] = &*(data_slice as *const [u8]);
                data.to_vec()
            }
        }
        #[cfg(not(target_os = "axle"))]
        {
            fs::read(path).unwrap()
        }
    };
    ttf_renderer::parse(&font_bytes)
}

#[derive(Debug, Copy, Clone)]
enum ShortcutMouseInteractionState {
    Unhovered,
    Hover,
    LeftClickDown,
    /// Carries a timestamp of when the left click up happened
    LeftClickUp(usize),
}

#[derive(Debug)]
pub struct DesktopShortcut {
    id: usize,
    frame: RefCell<Rect>,
    drawable_rects: RefCell<Vec<Rect>>,
    layer: RefCell<SingleFramebufferLayer>,
    icon: BitmapImage,
    path: String,
    title: String,
    first_click_start_time: RefCell<Option<u64>>,
    interaction_state: RefCell<ShortcutMouseInteractionState>,
    desktop_background_slice: RefCell<Option<SingleFramebufferLayer>>,
    desktop_gradient_background_color: RefCell<Option<Color>>,
}

impl DesktopShortcut {
    fn new(id: usize, icon: &BitmapImage, origin: Point, path: &str, title: &str) -> Self {
        let frame = Rect::from_parts(origin, Self::size());
        Self {
            id,
            frame: RefCell::new(frame),
            drawable_rects: RefCell::new(vec![]),
            layer: RefCell::new(SingleFramebufferLayer::new(frame.size)),
            icon: icon.clone(),
            path: path.to_string(),
            title: title.to_string(),
            first_click_start_time: RefCell::new(None),
            interaction_state: RefCell::new(ShortcutMouseInteractionState::Unhovered),
            desktop_background_slice: RefCell::new(None),
            desktop_gradient_background_color: RefCell::new(None),
        }
    }

    fn size() -> Size {
        Size::new(100, 76)
    }

    fn icon_image_size() -> Size {
        Size::new(60, 42)
    }

    fn is_during_left_click_down(&self) -> bool {
        matches!(
            self.mouse_interaction_state(),
            ShortcutMouseInteractionState::LeftClickDown
        )
    }

    fn is_in_soft_click(&self) -> bool {
        self.first_click_start_time.borrow().is_some()
    }

    fn set_first_click_start_time(&self, timestamp: u64) {
        *self.first_click_start_time.borrow_mut() = Some(timestamp)
    }

    fn first_click_start_time(&self) -> Option<u64> {
        *self.first_click_start_time.borrow()
    }

    fn clear_soft_click_status(&self) {
        println!("Clearing soft click status");
        *self.first_click_start_time.borrow_mut() = None
    }

    pub fn copy_desktop_background_slice(
        &self,
        desktop_background: &mut Box<SingleFramebufferLayer>,
    ) {
        let background_slice = desktop_background.get_slice(self.frame());
        let mut copy = SingleFramebufferLayer::new(background_slice.frame().size);
        copy.get_full_slice().blit2(&background_slice);
        *self.desktop_background_slice.borrow_mut() = Some(copy);
    }

    pub fn set_desktop_gradient_background_color(&self, desktop_gradient_background_color: Color) {
        *self.desktop_gradient_background_color.borrow_mut() =
            Some(desktop_gradient_background_color)
    }

    pub fn render(&self) {
        let mut slice = self.layer.borrow_mut().get_full_slice();
        let font = FONT.lock();

        // The background of the desktop shortcut depends on our current mouse interaction state
        match self.mouse_interaction_state() {
            ShortcutMouseInteractionState::Unhovered | ShortcutMouseInteractionState::Hover => {
                // Just render the desktop background
                let mut background_slice = self.desktop_background_slice.borrow_mut();
                let mut background_slice = background_slice.as_mut().unwrap();
                slice.blit2(&background_slice.get_full_slice());
            }
            ShortcutMouseInteractionState::LeftClickDown => {
                slice.fill(Color::new(80, 80, 255));
            }
            ShortcutMouseInteractionState::LeftClickUp(_) => {
                slice.fill(Color::new(127, 127, 255));
            }
        }

        let image_size = Self::icon_image_size();
        let image_margin = Size::new(self.frame().width() - image_size.width, 8);
        let image_origin = Point::new(
            (image_margin.width as f64 / 2.0) as isize,
            (image_margin.height as f64 / 2.0) as isize,
        );

        // Render the shortcut icon
        self.icon.render(
            &self
                .layer
                .borrow_mut()
                .get_slice(Rect::from_parts(image_origin, image_size)),
        );

        // Render the title label
        let font_size = Size::new(14, 14);
        let label_height = 28;
        let font_size = Size::new(8, 10);
        let label_height = 18;
        let label_mid = Point::new(
            (self.frame().width() as f64 / 2.0) as isize,
            (self.frame().height() as f64 - (label_height as f64 / 2.0)) as isize,
        );
        let title_len = self.title.len();
        let label_origin = Point::new(
            label_mid.x - (((font_size.width * title_len as isize) as f64 / 2.0) as isize),
            //4,
            //4,
            label_mid.y - ((font_size.height as f64 / 2.0) as isize),
            //label_mid.y - ((font_size.height as f64 / 2.0) as isize) - 6,
            //label_mid.y - ((font_size.height as f64 / 2.0) as isize) - 20,
            //self.frame().height() - font_size.height - 28,
        );
        // If the background gradient is too dark, set the shortcuts text color to white so it's always visible.
        // Per ITU-R BT.709
        let desktop_gradient_background_color =
            self.desktop_gradient_background_color.borrow().unwrap();
        let luma = (0.2126 * desktop_gradient_background_color.r as f64)
            + (0.7152 * desktop_gradient_background_color.g as f64)
            + (0.0722 * desktop_gradient_background_color.b as f64);
        let (mut text_color, border_color) = if luma < 64.0 {
            (Color::new(205, 205, 205), Color::new(205, 205, 205))
        } else {
            (Color::new(50, 50, 50), Color::dark_gray())
        };

        // Override the text color to white if the shortcut's been clicked
        match self.mouse_interaction_state() {
            ShortcutMouseInteractionState::LeftClickDown
            | ShortcutMouseInteractionState::LeftClickUp(_) => {
                text_color = Color::white();
            }
            _ => {}
        }

        let mut cursor = label_origin;
        for ch in self.title.chars() {
            slice.draw_char(ch, cursor, text_color, font_size);
            /*
            let (_, metrics) =
                render_char_onto(ch, &font, &mut slice, cursor, text_color, font_size);
            */
            //cursor.x += metrics.advance_width as isize + 2;
            cursor.x += font_size.width;
        }

        match self.mouse_interaction_state() {
            ShortcutMouseInteractionState::Hover
            | ShortcutMouseInteractionState::LeftClickDown
            | ShortcutMouseInteractionState::LeftClickUp(_) => {
                // Draw a border around the shortcut
                slice.fill_rect(
                    Rect::with_size(slice.frame().size),
                    border_color,
                    StrokeThickness::Width(2),
                );
            }
            _ => {}
        }
    }

    fn set_mouse_interaction_state(&self, interaction_state: ShortcutMouseInteractionState) {
        *self.interaction_state.borrow_mut() = interaction_state
    }

    fn mouse_interaction_state(&self) -> ShortcutMouseInteractionState {
        *self.interaction_state.borrow()
    }

    pub fn set_frame(&self, frame: Rect) {
        *self.frame.borrow_mut() = frame
    }
}

impl DesktopElement for DesktopShortcut {
    fn id(&self) -> usize {
        self.id
    }

    fn frame(&self) -> Rect {
        *self.frame.borrow()
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

    fn handle_mouse_entered(&self) -> MouseInteractionCallbackResult {
        self.set_mouse_interaction_state(ShortcutMouseInteractionState::Hover);
        self.render();
        MouseInteractionCallbackResult::RedrawRequested
    }

    fn handle_mouse_exited(&self) -> MouseInteractionCallbackResult {
        self.set_mouse_interaction_state(ShortcutMouseInteractionState::Unhovered);
        self.clear_soft_click_status();
        self.render();
        MouseInteractionCallbackResult::RedrawRequested
    }

    fn handle_left_click_began(&self, _mouse_pos: Point) -> MouseInteractionCallbackResult {
        self.set_mouse_interaction_state(ShortcutMouseInteractionState::LeftClickDown);
        if !self.is_in_soft_click() {
            self.set_first_click_start_time(get_timestamp());
        } else {
            let double_click_duration = get_timestamp() - self.first_click_start_time().unwrap();
            println!("Double click duration {double_click_duration}");
            // Always reset the double-click timer, regardless of whether the double-click succeeded.
            // This lets the user try double-clicking again if they weren't quick enough.
            self.clear_soft_click_status();
            if double_click_duration <= 500 {
                // Quick double-click, launch the underlying program
                request_program_launch(&self.path);
            }
        }
        self.render();
        MouseInteractionCallbackResult::RedrawRequested
    }

    fn handle_left_click_ended(&self, _mouse_pos: Point) -> MouseInteractionCallbackResult {
        self.set_mouse_interaction_state(ShortcutMouseInteractionState::LeftClickUp(
            get_timestamp() as usize,
        ));
        self.render();
        MouseInteractionCallbackResult::RedrawRequested
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
        DesktopShortcut::size() + Size::new(8, 12)
    }

    fn set_occupant(&self, shortcut: Option<Rc<DesktopShortcut>>) {
        *self.occupant.borrow_mut() = shortcut;
    }
}

pub struct DesktopShortcutsState {
    desktop_size: Size,
    slots: Vec<DesktopShortcutGridSlot>,
    /// Lookup (x,y) in all the possible grid positions of desktop icons, to the slot's linear
    /// position in the `slots` vector.
    slot_indexes_by_coordinates: BTreeMap<(isize, isize), usize>,
    pub shortcuts: RefCell<Vec<Rc<DesktopShortcut>>>,
}

impl DesktopShortcutsState {
    pub fn new(desktop_size: Size) -> Self {
        let mut slots = vec![];
        let mut slot_indexes_by_coordinates = BTreeMap::new();
        let grid_slot_size = DesktopShortcutGridSlot::size();
        // Iterate by columns so when searching for a free space linearly we fill in columns first
        for (x_idx, x) in (0..(desktop_size.width - grid_slot_size.width))
            .step_by(grid_slot_size.width as usize)
            .enumerate()
        {
            for (y_idx, y) in (AWM_MENU_BAR_HEIGHT
                ..(desktop_size.height
                    - grid_slot_size.height
                    - AWM_MENU_BAR_HEIGHT
                    - AWM_DOCK_HEIGHT))
                .step_by(grid_slot_size.height as usize)
                .enumerate()
            {
                slot_indexes_by_coordinates.insert((x_idx as isize, y_idx as isize), slots.len());
                slots.push(DesktopShortcutGridSlot::new(Point::new(x, y)));
            }
        }
        Self {
            desktop_size,
            slots,
            slot_indexes_by_coordinates,
            shortcuts: RefCell::new(vec![]),
        }
    }

    pub fn update_background(
        &self,
        background_layer: &mut Box<SingleFramebufferLayer>,
        desktop_gradient_background_color: Color,
    ) {
        for shortcut in self.slots.iter() {
            let occupant = shortcut.occupant.borrow();
            if let Some(occupant) = &*occupant {
                occupant.copy_desktop_background_slice(background_layer);
                occupant.set_desktop_gradient_background_color(desktop_gradient_background_color);
                occupant.render();
            }
        }
    }

    pub fn shortcut_origin_for_slot_frame(slot_frame: Rect) -> Point {
        let shortcut_size = DesktopShortcut::size();
        let shortcut_origin = Point::new(
            slot_frame.mid_x() - ((shortcut_size.width as f64 / 2.0) as isize),
            slot_frame.mid_y() - ((shortcut_size.height as f64 / 2.0) as isize),
        );
        shortcut_origin
    }

    fn add_shortcut_to_slot(
        &self,
        background_layer: &mut Box<SingleFramebufferLayer>,
        desktop_gradient_background_color: Color,
        id: usize,
        icon: &BitmapImage,
        path: &str,
        title: &str,
        slot: &DesktopShortcutGridSlot,
    ) -> Rc<DesktopShortcut> {
        let new_shortcut = Rc::new(DesktopShortcut::new(
            id,
            icon,
            Self::shortcut_origin_for_slot_frame(slot.frame),
            path,
            title,
        ));
        self.shortcuts.borrow_mut().push(Rc::clone(&new_shortcut));
        slot.set_occupant(Some(Rc::clone(&new_shortcut)));
        new_shortcut.copy_desktop_background_slice(background_layer);
        new_shortcut.set_desktop_gradient_background_color(desktop_gradient_background_color);
        new_shortcut.render();
        new_shortcut
    }

    pub fn add_shortcut_by_coordinates(
        &self,
        background_layer: &mut Box<SingleFramebufferLayer>,
        desktop_gradient_background_color: Color,
        id: usize,
        icon: &BitmapImage,
        path: &str,
        title: &str,
        coordinates: (isize, isize),
    ) -> Rc<DesktopShortcut> {
        let slot_index = self.slot_indexes_by_coordinates.get(&coordinates).unwrap();
        let slot = &self.slots[*slot_index];
        self.add_shortcut_to_slot(
            background_layer,
            desktop_gradient_background_color,
            id,
            icon,
            path,
            title,
            slot,
        )
    }

    fn free_slots(&self) -> impl Iterator<Item = &DesktopShortcutGridSlot> {
        self.slots.iter().filter(|s| s.occupant.borrow().is_none())
    }

    pub fn add_shortcut_to_next_free_slot(
        &self,
        background_layer: &mut Box<SingleFramebufferLayer>,
        desktop_gradient_background_color: Color,
        id: usize,
        icon: &BitmapImage,
        path: &str,
        title: &str,
    ) -> Rc<DesktopShortcut> {
        // Find the next empty grid slot
        if let Some(found_slot) = self.free_slots().next() {
            self.add_shortcut_to_slot(
                background_layer,
                desktop_gradient_background_color,
                id,
                icon,
                path,
                title,
                found_slot,
            )
        } else {
            panic!("Failed to find a free slot to place another desktop shortcut");
        }
    }

    pub fn transfer_shortcut_to_nearest_slot(&self, shortcut: &Rc<DesktopShortcut>) -> Rect {
        // Find the previous slot that contained this shortcut
        let previous_slot = self
            .slots
            .iter()
            .find(|slot| {
                if let Some(occupant) = &*slot.occupant.borrow() {
                    Rc::ptr_eq(occupant, shortcut)
                } else {
                    false
                }
            })
            .unwrap();
        previous_slot.set_occupant(None);

        let shortcut_frame = shortcut.frame();
        let nearest_slot = self
            .free_slots()
            .filter(|slot| slot.frame.intersects_with(shortcut_frame))
            .max_by(|slot1, slot2| {
                let s1_overlapping_area =
                    slot1.frame.area_overlapping_with(shortcut_frame).unwrap();
                let s2_overlapping_area =
                    slot2.frame.area_overlapping_with(shortcut_frame).unwrap();
                s1_overlapping_area.area().cmp(&s2_overlapping_area.area())
            })
            // Default to the previous slot if we were unable to find a good place to put this shortcut
            .unwrap_or(previous_slot);
        nearest_slot.set_occupant(Some(Rc::clone(shortcut)));
        nearest_slot.frame
    }
}
