use core::cell::RefCell;

use alloc::{boxed::Box, vec::Vec};
use bitmatch::bitmatch;
#[cfg(feature = "use_std")]
use pixels::Pixels;

#[cfg(not(feature = "use_std"))]
use axle_rt::println;

use crate::{
    gameboy::{GameBoy, GameBoyHardwareProvider},
    interrupts::InterruptType,
    mmu::{Addressable, Mmu},
};

const WINDOW_WIDTH: usize = 160;
const WINDOW_HEIGHT: usize = 144;

pub trait GraphicsLayer {
    fn get_pixel_buffer(&mut self) -> &mut [u8];
    fn render_to_screen(&self);
}

#[cfg(feature = "use_std")]
impl GraphicsLayer for Pixels {
    fn get_pixel_buffer(&mut self) -> &mut [u8] {
        self.get_frame()
    }

    fn render_to_screen(&self) {
        self.render().unwrap();
    }
}

#[derive(Copy, Clone, PartialEq, Debug)]
pub enum PpuMode {
    OamSearch,
    PixelTransfer,
    HBlank,
    VBlank,
}

pub struct Ppu {
    main_window_layer: RefCell<Box<dyn GraphicsLayer>>,
    //vram_debug_layer: RefCell<Box<dyn GraphicsLayer>>,
    lcd_control: RefCell<u8>,
    ly: RefCell<u8>,
    lyc: RefCell<u8>,
    scy: RefCell<u8>,
    scx: RefCell<u8>,
    wx: RefCell<u8>,
    wy: RefCell<u8>,
    bgp: RefCell<u8>,
    obp0: RefCell<u8>,
    obp1: RefCell<u8>,
    stat: RefCell<u8>,
    pub current_mode: RefCell<PpuMode>,
    ticks: RefCell<usize>,
    scanline_x: usize,
    window_current_y: RefCell<usize>,
}

const TILE_WIDTH: usize = 8;
const TILE_HEIGHT: usize = 8;
const SCREEN_WIDTH: usize = 160;
const SCREEN_HEIGHT: usize = 144;

enum StatusUpdate {
    LycEqualsLy(bool),
    Mode(PpuMode),
}

#[derive(Clone, Copy)]
enum TileDataAddressingMode {
    Unsigned,
    Signed,
}

impl Ppu {
    const LCD_CONTROL_ADDR: u16 = 0xff40;
    const STAT_ADDR: u16 = 0xff41;
    const SCY_ADDR: u16 = 0xff42;
    const SCX_ADDR: u16 = 0xff43;
    const LY_ADDR: u16 = 0xff44;
    const LYC_ADDR: u16 = 0xff45;
    const BGP_ADDR: u16 = 0xff47;
    const OBP0_ADDR: u16 = 0xff48;
    const OBP1_ADDR: u16 = 0xff49;
    const WY_ADDR: u16 = 0xff4a;
    const WX_ADDR: u16 = 0xff4b;

    pub fn new(
        main_window_layer: Box<dyn GraphicsLayer>,
        //vram_debug_layer: Box<dyn GraphicsLayer>,
    ) -> Self {
        Ppu {
            main_window_layer: RefCell::new(main_window_layer),
            //vram_debug_layer: RefCell::new(vram_debug_layer),
            lcd_control: RefCell::new(0b10000000),
            ly: RefCell::new(0),
            lyc: RefCell::new(0),
            scy: RefCell::new(0),
            scx: RefCell::new(0),
            wx: RefCell::new(0),
            wy: RefCell::new(0),
            bgp: RefCell::new(0),
            obp0: RefCell::new(0),
            obp1: RefCell::new(0),
            stat: RefCell::new(0),
            current_mode: RefCell::new(PpuMode::OamSearch),
            ticks: RefCell::new(0),
            scanline_x: 0,
            window_current_y: RefCell::new(0),
        }
    }

    fn set_tile_data_addressing_mode(&self, addressing_mode: TileDataAddressingMode) {
        let mut lcd_control = self.lcd_control.borrow_mut();
        match addressing_mode {
            TileDataAddressingMode::Unsigned => {
                // Enable LCDControl.4
                *lcd_control |= (1 << 4)
            }
            TileDataAddressingMode::Signed => {
                // Disable LCDControl.4
                *lcd_control &= !(1 << 4)
            }
        }
    }

    fn get_current_mode(&self) -> PpuMode {
        *self.current_mode.borrow()
    }

    fn draw_tile(&self, mmu: &Mmu, tile_idx: usize, origin_x: usize, origin_y: usize) {
        /*
        let mut vram_debug_layer = self.vram_debug_layer.borrow_mut();
        let mut frame = vram_debug_layer.get_pixel_buffer();
        let tile_size = 16;
        let vram_base = self.tile_data_base_address() as usize;

        let tile_base = vram_base + (tile_idx * tile_size);
        //println!("Tile idx {tile_idx:02x} base {tile_base:04x}");

        for row in 0..TILE_HEIGHT {
            let row_byte1 = mmu.read((tile_base + (2 * row) + 0).try_into().unwrap());
            let row_byte2 = mmu.read((tile_base + (2 * row) + 1).try_into().unwrap());
            /*
            println!("Row bytes {:02x} {:02x}", row_byte1, row_byte2);
            println!(
                "tile_base + (2 * row) + 0: {:04x}",
                tile_base + (2 * row) + 0
            );
            println!(
                "tile_base + (2 * row) + 1: {:04x}",
                tile_base + (2 * row) + 1
            );
            */
            for px in 0..TILE_WIDTH {
                let px_color_id = ((row_byte1 >> (TILE_WIDTH - px - 1)) & 0b1) << 1
                    | ((row_byte2 >> (TILE_WIDTH - px - 1)) & 0b1);
                let color = match px_color_id {
                    0b00 => (255, 255, 255),
                    0b01 => (255, 0, 0),
                    0b10 => (0, 0, 0),
                    0b11 => (0, 0, 255),
                    _ => panic!("Invalid index"),
                };
                let point = (origin_x + px, origin_y + row);
                //let frame_idx = (origin.1 * (WINDOW_WIDTH as u16) * 4) + (origin.0 * 4);
                let frame_idx = (point.1 * 256 * 4) + (point.0 * 4);
                /*
                println!(
                    "Pixel at ({}, {}), Tile ({}, {})",
                    point.0, point.1, px, row
                );
                */
                frame[(frame_idx + 0) as usize] = color.0;
                frame[(frame_idx + 1) as usize] = color.1;
                frame[(frame_idx + 2) as usize] = color.2;
                frame[(frame_idx + 3) as usize] = 0xff;
            }
        }
        */
    }

    fn draw_sprite(&self, mmu: &Mmu, tile_idx: usize, origin_x: usize, origin_y: usize) {
        /*
        let mut main_window_layer = self.main_window_layer.borrow_mut();
        let mut frame = main_window_layer.get_pixel_buffer();
        let tile_size = 16;
        let vram_base = 0x8000;

        let tile_base = vram_base + (tile_idx * tile_size);
        //println!("Tile idx {tile_idx:02x} base {tile_base:04x}");

        for row in 0..TILE_HEIGHT {
            let row_byte1 = mmu.read((tile_base + (2 * row) + 0).try_into().unwrap());
            let row_byte2 = mmu.read((tile_base + (2 * row) + 1).try_into().unwrap());
            /*
            println!("Row bytes {:02x} {:02x}", row_byte1, row_byte2);
            println!(
                "tile_base + (2 * row) + 0: {:04x}",
                tile_base + (2 * row) + 0
            );
            println!(
                "tile_base + (2 * row) + 1: {:04x}",
                tile_base + (2 * row) + 1
            );
            */
            for px in 0..TILE_WIDTH {
                let px_color_id = ((row_byte1 >> (TILE_WIDTH - px - 1)) & 0b1) << 1
                    | ((row_byte2 >> (TILE_WIDTH - px - 1)) & 0b1);
                if px_color_id == 0 {
                    continue;
                }
                let color = match px_color_id {
                    0b00 => (255, 255, 255),
                    0b01 => (255, 0, 0),
                    0b10 => (0, 0, 0),
                    0b11 => (0, 0, 255),
                    _ => panic!("Invalid index"),
                };
                let point = (origin_x + px, origin_y + row);
                let frame_idx = (point.1 * WINDOW_WIDTH * 4) + (point.0 * 4);
                /*
                println!(
                    "Pixel at ({}, {}), Tile ({}, {})",
                    point.0, point.1, px, row
                );
                */
                frame[(frame_idx + 0) as usize] = color.0;
                frame[(frame_idx + 1) as usize] = color.1;
                frame[(frame_idx + 2) as usize] = color.2;
                frame[(frame_idx + 3) as usize] = 0xff;
            }
        }
        */
    }

    pub fn update_status(&self, system: &dyn GameBoyHardwareProvider, status: StatusUpdate) {
        let mut stat = self.stat.borrow_mut();
        match status {
            StatusUpdate::LycEqualsLy(equals) => {
                let bit_index = 2;
                if equals {
                    // Enable the bit
                    *stat |= (1 << bit_index);
                } else {
                    // Disable the bit
                    *stat &= !(1 << bit_index);
                }
                /*
                println!(
                    "update_status LycEqualsLy? {equals}, lyc = {}, ly = {}",
                    *self.lyc.borrow(),
                    *self.ly.borrow()
                );
                */
                //println!("update_status LycEqualsLy? {equals}",);

                if equals {
                    // Trigger an interrupt if this source is enabled
                    if (*stat >> 6) & 0b1 == 0b1 {
                        //println!("Trigger LYC=LY");
                        system
                            .get_interrupt_controller()
                            .trigger_interrupt(InterruptType::LCDStat);
                    }
                }
            }
            StatusUpdate::Mode(mode) => {
                let value = match mode {
                    PpuMode::HBlank => 0b00,
                    PpuMode::VBlank => 0b01,
                    PpuMode::OamSearch => 0b10,
                    PpuMode::PixelTransfer => 0b11,
                };
                // First, mask off whatever was already in the register
                *stat &= !(0b11);
                // Now update it with the mode we're setting
                *stat |= value;

                // Trigger an interrupt for this mode switch?
                let interrupt_source_bit_index = match mode {
                    PpuMode::HBlank => Some(3),
                    PpuMode::VBlank => Some(4),
                    PpuMode::OamSearch => Some(5),
                    _ => None,
                };
                if let Some(interrupt_source_bit_index) = interrupt_source_bit_index {
                    if (*stat >> interrupt_source_bit_index) & 0b1 == 0b1 {
                        system
                            .get_interrupt_controller()
                            .trigger_interrupt(InterruptType::LCDStat);
                    }
                }
            }
        }
    }

    fn set_mode(&self, system: &dyn GameBoyHardwareProvider, state: PpuMode) {
        self.update_status(system, StatusUpdate::Mode(state));
        *self.current_mode.borrow_mut() = state;

        if state == PpuMode::VBlank {
            //println!("PPU requested VBlank interrupt...");
            system
                .get_interrupt_controller()
                .trigger_interrupt(InterruptType::VBlank);
            self.blit_to_os_window();

            // Reset the window line-counter
            *self.window_current_y.borrow_mut() = 0;
        }
    }

    fn current_mode(&self) -> PpuMode {
        *self.current_mode.borrow()
    }

    #[bitmatch]
    fn tile_data_base_address(&self) -> u16 {
        let lcd_control = *self.lcd_control.borrow();
        #[bitmatch]
        match lcd_control {
            "xxx0xxxx" => 0x8800,
            "xxx1xxxx" => 0x8000,
        }
    }

    #[bitmatch]
    fn background_tile_map_base_address(&self) -> u16 {
        let lcd_control = *self.lcd_control.borrow();
        #[bitmatch]
        match lcd_control {
            "xxxx0xxx" => 0x9800,
            "xxxx1xxx" => 0x9c00,
        }
    }

    #[bitmatch]
    fn window_tile_map_base_address(&self) -> u16 {
        let lcd_control = *self.lcd_control.borrow();
        #[bitmatch]
        match lcd_control {
            "x0xxxxxx" => 0x9800,
            "x1xxxxxx" => 0x9c00,
        }
    }

    pub fn blit_to_os_window(&self) {
        self.main_window_layer.borrow().render_to_screen();
    }

    fn get_scroll_y(&self) -> u8 {
        *self.scy.borrow()
    }

    fn get_scroll_x(&self) -> u8 {
        *self.scx.borrow()
    }

    fn get_tile_row_from_vram<F>(
        &self,
        system: &dyn GameBoyHardwareProvider,
        tile_base_address: usize,
        row_idx: usize,
        color_id_to_color: F,
    ) -> Vec<Option<(u8, u8, u8)>>
    where
        F: Fn(u8) -> Option<(u8, u8, u8)>,
    {
        let mmu = system.get_mmu();
        // We've got the base address of the tile, now compute the base of the
        // row that was requested
        // TODO(PT): Tile row size to constant
        let tile_row_size_in_bytes = 2usize;
        let row_base_address = tile_base_address + (row_idx * tile_row_size_in_bytes);

        // The tile row data takes 2 bytes to store
        let row_byte1 = mmu.read(row_base_address as u16);
        let row_byte2 = mmu.read((row_base_address + 1) as u16);
        let mut pixels = Vec::new();
        for px_idx in 0..8 {
            let px_color_id = ((row_byte2 >> (TILE_WIDTH - px_idx - 1)) & 0b1) << 1
                | ((row_byte1 >> (TILE_WIDTH - px_idx - 1)) & 0b1);
            //let px_color_id = (((row_byte1 >> px_idx) & 0b1) << 1) | ((row_byte2 >> px_idx) & 0b1);
            let color = color_id_to_color(px_color_id);
            pixels.push(color);
        }
        pixels
    }

    fn get_tile_row_from_tile_grid(
        &self,
        system: &dyn GameBoyHardwareProvider,
        tile_map_base_address: u16,
        row_idx: usize,
        x_within_tile_data: usize,
        y_within_tile_data: usize,
    ) -> Vec<(u8, u8, u8)> {
        let mmu = system.get_mmu();
        // The background tile map is really a linear array, rather than a 32x32 grid.
        // Convert our tile map coordinate to an index
        let background_texture_to_tile_map_ratio = 256 / 32;
        let x_within_tile_map = x_within_tile_data / background_texture_to_tile_map_ratio;
        let y_within_tile_map = y_within_tile_data / background_texture_to_tile_map_ratio;
        let tile_grid_lookup_idx = (y_within_tile_map * 32) + x_within_tile_map;
        // Now that we've got an index into the background tile map, look up the
        // tile index that should be rendered here
        let tile_idx = mmu.read(tile_map_base_address + (tile_grid_lookup_idx as u16));

        // We've got all the information to look up the raw tile data in the tile RAM
        // First, compute the base address of the tile - but remember that we'll be
        // reading some row after the tile's base
        let tile_ram_base = self.tile_data_base_address();
        // Each tile is 8 rows of pixel data, where each row takes 2 bytes to store
        // Thus, the total size of a tile is 16 bytes
        let tile_row_size_in_bytes = 2usize;
        let tile_size_in_bytes = tile_row_size_in_bytes * 8;
        let tile_base_address = match tile_ram_base {
            0x8800 => {
                // Signed addressing mode
                0x9000_u16.wrapping_add(
                    (((tile_idx as i8) as u16).wrapping_mul(tile_size_in_bytes as u16)),
                )
            }
            _ => tile_ram_base + (tile_idx as u16 * tile_size_in_bytes as u16),
        };

        // Tiles in VRam contain 2 bits indiciating the pixel color, but this color is actually
        // an index into the background palette register.
        // Map the color index to the desired color read from the palette.
        let background_palette = *self.bgp.borrow();
        let wrapped_pixels =
            self.get_tile_row_from_vram(system, tile_base_address as usize, row_idx, |color_id| {
                let background_palette_color_id = match color_id {
                    0b00 => (background_palette >> 0) & 0b11,
                    0b01 => (background_palette >> 2) & 0b11,
                    0b10 => (background_palette >> 4) & 0b11,
                    0b11 => (background_palette >> 6) & 0b11,
                    _ => panic!("Invalid index"),
                };
                match background_palette_color_id {
                    /*
                    0b00 => Some((7, 22, 28)),
                    0b01 => Some((44, 90, 75)),
                    0b10 => Some((106, 148, 90)),
                    0b11 => Some((224, 248, 211)),
                    */
                    0b00 => Some((227, 246, 211)),
                    0b01 => Some((147, 189, 121)),
                    0b10 => Some((64, 102, 86)),
                    0b11 => Some((11, 23, 30)),
                    _ => panic!("Invalid index"),
                }
            });
        wrapped_pixels.iter().map(|px| px.unwrap()).collect()
    }

    fn is_window_enabled(&self) -> bool {
        (*self.lcd_control.borrow() >> 5) & 0b1 == 0b1
    }

    fn render_scanline(&self, system: &dyn GameBoyHardwareProvider, screen_y: u8) {
        let mut main_window_layer = self.main_window_layer.borrow_mut();
        let mut frame = main_window_layer.get_pixel_buffer();

        // The game will maintain a 32x32 grid of tiles
        // Each entry in the grid is an index into the tile data representing the tile
        // that should be rendered there.
        // There's a maximum of 256 tiles, and so this tile index takes up a byte.
        //
        // We want to find the tiles that overlap with the scanline that should be rendered.
        //
        // We know what screen scanline is currently being rendered, but this doesn't directly
        // correspond to a row within the tilemap: the screen is generally configured
        // to render an offset within the tilemap, based on the scroll registers
        let rendered_y_within_tile_map = screen_y.wrapping_add(self.get_scroll_y());
        // The above Y is within the context of the 256x256 background texture.
        // The actual tiles that are rendered in each 8x8 slot are given by a separate
        // 32x32 grid, each entry of which contains an index into VRAM data.
        // Calculate which row within the 32x32 grid is rendered for this Y.
        let background_texture_to_tile_map_ratio = 256 / 32;
        let base_y_within_tile_map = (rendered_y_within_tile_map as usize) & !(TILE_HEIGHT - 1);
        let rendered_y_tile_map_row = base_y_within_tile_map / background_texture_to_tile_map_ratio;

        let scx = self.get_scroll_x();
        let base_x_within_tile_map = scx as usize;
        let base_x_of_starting_tile = base_x_within_tile_map & !(TILE_WIDTH - 1);
        let base_x_tile_map_col = base_x_of_starting_tile / background_texture_to_tile_map_ratio;

        let mut rendered_pixel_count = 0;
        let mut background_tile_base_x = base_x_of_starting_tile;
        // For the first tile we draw, the scroll X might be such that we should throw away
        // the first few tile data pixels, because we shouldn't start drawing until an
        // offset within the tile.
        let mut cursor_within_tile_pixels = base_x_within_tile_map - base_x_of_starting_tile;
        /*
        //
        // Look up the tile within the window map, if the window is enabled and the (X, Y) overlaps
        // with the window area
        let tile_map_base_address = {
            if screen_x + 0 >= *self.wx.borrow() && screen_y >= *self.wy.borrow() {
                self.window_tile_map_base_address()
            } else {
                self.background_tile_map_base_address()
            }
        };
        println!("WX {} WY {} ", *self.wx.borrow(), *self.wy.borrow());
        let tile_map_base_address = self.window_tile_map_base_address();
        */
        let mut tile_row = self.get_tile_row_from_tile_grid(
            system,
            self.background_tile_map_base_address(),
            (rendered_y_within_tile_map - (base_y_within_tile_map as u8)).into(),
            background_tile_base_x,
            rendered_y_within_tile_map.into(),
        );

        let row_base = (screen_y as usize) * SCREEN_WIDTH * 4;
        let mut source_tile_map = self.background_tile_map_base_address();
        let mut is_fetching_from_window_tile_map = false;
        // TODOO(PT): This is offset by 7, offset here instead of adding down below?
        let window_start_x = *self.wx.borrow() as usize;
        let will_draw_window_this_scanline = self.is_window_enabled()
            && screen_y >= *self.wy.borrow()
            && window_start_x < SCREEN_WIDTH + 7;
        //println!("window_start_x {window_start_x}, window_start_y {}, will_draw_window {will_draw_window_this_scanline}", *self.wy.borrow());
        while rendered_pixel_count < SCREEN_WIDTH {
            // We need to fetch the next tile if we've run out of pixels within the current tile
            let mut need_to_fetch_next_tile = cursor_within_tile_pixels == TILE_WIDTH;

            // Have we reached where we should begin the drawing the window?
            if will_draw_window_this_scanline && rendered_pixel_count + 7 == window_start_x {
                // We've just switched to the window tile map, so we definitely need to fetch
                // a new tile to draw
                need_to_fetch_next_tile = true;
                source_tile_map = self.window_tile_map_base_address();
                is_fetching_from_window_tile_map = true;
            }

            if need_to_fetch_next_tile {
                cursor_within_tile_pixels = 0;

                if is_fetching_from_window_tile_map {
                    //let y_within_window = (rendered_y_within_tile_map - *self.wy.borrow()) as usize;
                    let y_within_window = *self.window_current_y.borrow();
                    let tile_base_y_within_window = y_within_window & !(TILE_HEIGHT - 1);
                    tile_row = self.get_tile_row_from_tile_grid(
                        system,
                        self.window_tile_map_base_address(),
                        y_within_window - tile_base_y_within_window,
                        rendered_pixel_count - window_start_x + 7,
                        y_within_window,
                    );
                } else {
                    // We've drawn all of the pixels in this tile, start drawing the next one
                    background_tile_base_x += TILE_WIDTH;
                    if background_tile_base_x >= 256 {
                        // Wrap around to the start of the row
                        background_tile_base_x = 0;
                    }
                    tile_row = self.get_tile_row_from_tile_grid(
                        system,
                        source_tile_map,
                        (rendered_y_within_tile_map - (base_y_within_tile_map as u8)).into(),
                        background_tile_base_x,
                        rendered_y_within_tile_map.into(),
                    );
                }
            }

            // We have tile data to push out
            let px = tile_row[cursor_within_tile_pixels];
            let px_offset = row_base + (rendered_pixel_count * 4);
            frame[px_offset + 0] = px.0;
            frame[px_offset + 1] = px.1;
            frame[px_offset + 2] = px.2;
            frame[px_offset + 3] = 0xff;

            rendered_pixel_count += 1;
            cursor_within_tile_pixels += 1;
        }

        if will_draw_window_this_scanline {
            *self.window_current_y.borrow_mut() += 1;
        }
    }

    fn render_sprites_on_scanline(&self, system: &dyn GameBoyHardwareProvider, screen_y: u8) {
        let mut main_window_layer = self.main_window_layer.borrow_mut();
        let mut frame = main_window_layer.get_pixel_buffer();
        // Now, draw all the sprite scanlines that should be rendered here
        // Firstly, let's iterate OAM to pull all the sprites that overlap with
        // this scanline.
        // TODO(PT): For fidelity, this search should actually be done in the
        // OAMSearch screen state.
        let oam_base = 0xfe00;
        // TODO(PT): The hardware enforces a 10-sprite-per-scanline limit, and some
        // games rely on this to hide sprites by making them the 11th sprite in a line.

        // OAM entry format:
        // (Y, X, Tile Index, Attributes)
        let oam_entry_size_in_bytes = 4;
        let oam_entry_count = 40;
        let mmu = system.get_mmu();
        let double_height_sprites = ((*self.lcd_control.borrow()) >> 2) & 0b1 == 0b1;
        for i in 0..oam_entry_count {
            let oam_entry_base_addr = oam_base + (i * oam_entry_size_in_bytes);

            let sprite_oam_coordinates_start_y = mmu.read(oam_entry_base_addr + 0) as usize;
            let sprite_oam_coordinates_start_x = mmu.read(oam_entry_base_addr + 1) as usize;
            let mut tile_map_index_of_sprite = mmu.read(oam_entry_base_addr + 2);
            let sprite_attributes = mmu.read(oam_entry_base_addr + 3);

            // Check whether the scanline we're drawing is within this tile
            // Note that the coordinate system of OAM entries is translated (-8, -16) relative
            // to the screen coordinate system.
            //println!("OAM ({sprite_oam_coordinates_start_x}, {sprite_oam_coordinates_start_y}), TileMap {tile_map_index_of_sprite}, Attrs {sprite_attributes:08b}");
            /*
            if tile_map_index_of_sprite == 0 {
                continue;
            }
            */
            let mut sprite_screen_coordinates = (
                sprite_oam_coordinates_start_x - 8,
                //sprite_oam_coordinates_start_x.wrapping_sub(8),
                sprite_oam_coordinates_start_y - 16,
                //sprite_oam_coordinates_start_y.wrapping_sub(16),
            );

            let sprite_width = 8;
            let sprite_height = match double_height_sprites {
                true => 16,
                false => 8,
            };

            //let is_sprite_flipped_along_x_axis = sprite_attributes & (1 << 6) != 0;
            let is_sprite_flipped_along_x_axis = (sprite_attributes >> 6) & 0b1 == 0b1;
            /*
            if is_sprite_flipped_along_x_axis {
                sprite_screen_coordinates.1 += sprite_height * 2;
            }
            */

            if (screen_y as usize) < sprite_screen_coordinates.1
                || (screen_y as usize) >= sprite_screen_coordinates.1 + sprite_height
            {
                // Skip this sprite as the current scanline is not within it
                continue;
            }

            /*
            if is_sprite_flipped_along_x_axis {
                println!("Inverted sprite x {sprite_oam_coordinates_start_x} y {sprite_oam_coordinates_start_y} tile_map_index {tile_map_index_of_sprite} attrs {sprite_attributes:08b}");
            }
            */

            //
            // This scanline contains a row from this sprite!
            // Read the tile data
            let mut row_within_tile = match is_sprite_flipped_along_x_axis {
                true => (sprite_height - 1) - ((screen_y as usize) - sprite_screen_coordinates.1),
                false => (screen_y as usize) - sprite_screen_coordinates.1,
            };
            /*
            if is_sprite_flipped_along_x_axis {
                println!(
                    "Row within tile {row_within_tile} un-inverted {}",
                    (screen_y as usize) - sprite_screen_coordinates.1
                );
            }
            */
            // If this is a tall sprite, handle drawing the second 8-line sprite
            if double_height_sprites && row_within_tile >= 8 {
                row_within_tile -= 8;
                tile_map_index_of_sprite += 1;
            }

            /*
            if is_sprite_flipped_along_x_axis {
                tile_map_index_of_sprite -= 1;
            }
            */

            //println!("Sprite ({}, {}), Tile {tile_map_index_of_sprite:02x}, Attrs {sprite_attributes:08b}", sprite_screen_coordinates.0, sprite_screen_coordinates.1);
            // Sprites are always placed at 0x8000
            let sprite_vram_base_address = 0x8000;
            // TODO(PT): Size of tile to constant?
            let tile_size_in_bytes = 16;
            let tile_base_address = sprite_vram_base_address
                + ((tile_map_index_of_sprite as usize) * tile_size_in_bytes);

            let palette = if (sprite_attributes >> 4) & 0b1 == 0b0 {
                *self.obp0.borrow()
            } else {
                *self.obp1.borrow()
            };
            let tile_row = self.get_tile_row_from_vram(
                system,
                tile_base_address,
                row_within_tile,
                |color_id| {
                    if color_id == 0b00 {
                        // For sprites, a color ID of 0 means transparent
                        return None;
                    }
                    let sprite_palette_color_id = match color_id {
                        0b01 => (palette >> 2) & 0b11,
                        0b10 => (palette >> 4) & 0b11,
                        0b11 => (palette >> 6) & 0b11,
                        _ => panic!("Invalid index"),
                    };
                    match sprite_palette_color_id {
                        /*
                        0b01 => Some((44, 90, 75)),
                        0b10 => Some((106, 148, 90)),
                        0b11 => Some((224, 248, 211)),
                        */
                        0b00 => Some((227, 246, 211)),
                        0b01 => Some((147, 189, 121)),
                        0b10 => Some((64, 102, 86)),
                        0b11 => Some((11, 23, 30)),
                        _ => panic!("Invalid index"),
                    }
                },
            );

            let sprite_attributes = mmu.read(oam_entry_base_addr + 3);
            // Is the sprite flipped on the Y axis?
            let is_sprite_flipped_along_y_axis = sprite_attributes & (1 << 5) != 0;

            let row_base = (screen_y as usize) * SCREEN_WIDTH * 4;
            for x_idx in 0..TILE_WIDTH {
                let px_x = (sprite_screen_coordinates.0 as usize) + x_idx;
                if px_x >= SCREEN_WIDTH {
                    break;
                }
                let tile_px_idx = if is_sprite_flipped_along_y_axis {
                    7 - x_idx
                } else {
                    x_idx
                };
                let px = tile_row[tile_px_idx];
                if let Some(px) = px {
                    let px_offset = row_base + (px_x * 4);
                    frame[px_offset + 0] = px.0;
                    frame[px_offset + 1] = px.1;
                    frame[px_offset + 2] = px.2;
                    frame[px_offset + 3] = 0xff;
                }
            }
        }
    }

    pub fn step(&self, system: &dyn GameBoyHardwareProvider) {
        let mmu = system.get_mmu();
        let mut ticks = self.ticks.borrow_mut();
        let mut ly = self.ly.borrow_mut();

        /*
        // Reached LYC?
        if *ly == *self.lyc.borrow_mut() {
            // Trigger LYC interrupt if the LYC=LY interrupt source bit is set
            if (*self.stat.borrow() >> 6) & 0b1 == 0b1 {
                system
                    .get_interrupt_controller()
                    .trigger_interrupt(InterruptType::LCDStat);
            }
        }
        */

        *ticks += 1;
        match self.current_mode() {
            PpuMode::OamSearch => {
                // TODO(PT): Collect sprite data
                if *ticks == 20 {
                    //println!("OAMSearch -> PixelTransfer");
                    self.set_mode(system, PpuMode::PixelTransfer);
                } else {
                    //println!("Do OAM search");
                }
            }
            PpuMode::PixelTransfer => {
                // TODO(PT): Push pixel data
                if *ticks == 63 {
                    //println!("PixelTransfer -> HBlank");
                    self.set_mode(system, PpuMode::HBlank);
                }
            }
            PpuMode::HBlank => {
                // As a simple hack, draw the full scanline when we enter HBlank
                if *ticks == 64 {
                    self.render_scanline(system, *ly);
                    self.render_sprites_on_scanline(system, *ly);

                    /*
                    println!(
                        "calling update_status, lyc = {}, ly = {}",
                        *self.lyc.borrow(),
                        *ly
                    );
                    */
                    self.update_status(
                        system,
                        StatusUpdate::LycEqualsLy(*ly == *self.lyc.borrow()),
                    );
                }
                // Have we reached the bottom of the screen?
                // TODO(PT): Wait, then go back to sprite search for the next line, or vblank
                if *ticks == 114 {
                    *ticks = 0;
                    *ly += 1;

                    if *ly == (WINDOW_HEIGHT as u8) {
                        self.set_mode(system, PpuMode::VBlank);
                        // Trigger a VBlank interrupt
                        // Debug VRAM
                        for tile in 0..256 {
                            let tiles_per_row = 16;
                            let col = tile % tiles_per_row;
                            let row = tile / tiles_per_row;
                            let origin = (col * TILE_WIDTH, row * TILE_HEIGHT);
                            self.draw_tile(&mmu, tile, origin.0, origin.1);
                        }
                        //self.vram_debug_layer.borrow().render_to_screen();
                    } else {
                        self.set_mode(system, PpuMode::OamSearch);
                    }
                }
            }
            PpuMode::VBlank => {
                // TODO(PT): Wait, then go back to sprite search for the top line
                if *ticks == 114 {
                    *ly += 1;
                    *ticks = 0;
                    if *ly == 153 {
                        *ly = 0;
                        self.set_mode(system, PpuMode::OamSearch);
                    }
                }
            }
        }
    }
}

impl Addressable for Ppu {
    fn contains(&self, addr: u16) -> bool {
        match addr {
            Ppu::LCD_CONTROL_ADDR => true,
            Ppu::LY_ADDR => true,
            Ppu::LYC_ADDR => true,
            Ppu::SCY_ADDR => true,
            Ppu::SCX_ADDR => true,
            Ppu::STAT_ADDR => true,
            Ppu::BGP_ADDR => true,
            Ppu::OBP0_ADDR => true,
            Ppu::OBP1_ADDR => true,
            Ppu::WX_ADDR => true,
            Ppu::WY_ADDR => true,
            _ => false,
        }
    }

    fn read(&self, addr: u16) -> u8 {
        match addr {
            Ppu::LCD_CONTROL_ADDR => *self.lcd_control.borrow(),
            Ppu::LY_ADDR => *self.ly.borrow(),
            Ppu::LYC_ADDR => *self.lyc.borrow(),
            Ppu::SCY_ADDR => *self.scy.borrow(),
            Ppu::SCX_ADDR => *self.scx.borrow(),
            Ppu::STAT_ADDR => *self.stat.borrow(),
            Ppu::BGP_ADDR => *self.bgp.borrow(),
            Ppu::OBP0_ADDR => *self.obp0.borrow(),
            Ppu::OBP1_ADDR => *self.obp1.borrow(),
            Ppu::WX_ADDR => *self.wx.borrow(),
            Ppu::WY_ADDR => *self.wy.borrow(),
            _ => panic!("Unrecognised address"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            Ppu::LCD_CONTROL_ADDR => {
                //println!("Writing to LCD Control: {val:08b}");
                *(self.lcd_control.borrow_mut()) = val
            }
            Ppu::LY_ADDR => {
                //panic!("Writes to LY are not allowed")
                println!("Writes to LY are not allowed")
            }
            Ppu::LYC_ADDR => *(self.lyc.borrow_mut()) = val,
            Ppu::SCY_ADDR => *(self.scy.borrow_mut()) = val,
            Ppu::SCX_ADDR => *(self.scx.borrow_mut()) = val,
            Ppu::STAT_ADDR => {
                // The bottom 3 bits are read-only, so mask them off the requested write
                let masked_write = val & !(0b111);
                // Preserve the current bottom 3 bits
                let mut stat = self.stat.borrow_mut();
                let masked_current_value = *stat & 0b111;
                *stat = masked_write | masked_current_value
                //println!("Wrote to LCD STAT register: {val:08b} {new_value:08b}");
            }
            Ppu::BGP_ADDR => *(self.bgp.borrow_mut()) = val,
            Ppu::OBP0_ADDR => *(self.obp0.borrow_mut()) = val,
            Ppu::OBP1_ADDR => *(self.obp1.borrow_mut()) = val,
            Ppu::WX_ADDR => *(self.wx.borrow_mut()) = val,
            Ppu::WY_ADDR => *(self.wy.borrow_mut()) = val,
            _ => panic!("Unrecognised address"),
        }
    }
}

#[cfg(test)]
mod tests {
    use std::{cell::RefCell, rc::Rc};

    use pixels::{Pixels, SurfaceTexture};
    use winit::{
        dpi::{LogicalPosition, LogicalSize},
        event_loop::EventLoop,
        window::WindowBuilder,
    };

    use crate::{
        cpu::CpuState,
        gameboy::GameBoyHardwareProvider,
        interrupts::InterruptController,
        joypad::Joypad,
        mmu::{Mmu, Ram},
        WINDOW_HEIGHT, WINDOW_WIDTH,
    };

    use super::{GraphicsLayer, Ppu, PpuMode, TileDataAddressingMode};

    struct TestLayer {
        //pixel_buffer: [u8; 160 * 144 * 4],
        pixel_buffer: Vec<u8>,
    }

    impl TestLayer {
        fn new(size: usize) -> Self {
            Self {
                //pixel_buffer: [0; 160 * 144 * 4],
                pixel_buffer: vec![0; size],
            }
        }
    }

    impl GraphicsLayer for TestLayer {
        fn get_pixel_buffer(&mut self) -> &mut [u8] {
            &mut self.pixel_buffer
        }

        fn render_to_screen(&self) {
            println!("Skipping screen render for testbed layer");
        }
    }

    struct PpuTestSystem {
        pub mmu: Rc<Mmu>,
        pub cpu: Rc<RefCell<CpuState>>,
        interrupt_controller: Rc<InterruptController>,
        pub ppu: Rc<Ppu>,
    }

    impl PpuTestSystem {
        pub fn new() -> Self {
            let ppu = Rc::new(Ppu::new(
                Box::new(TestLayer::new(160 * 144 * 4)),
                Box::new(TestLayer::new(256 * 256 * 4)),
            ));

            let ram = Rc::new(Ram::new(0, 0xffff));

            // Ensure PPU has memory access priority, since it overlaps with the phony RAM
            let ppu_clone = Rc::clone(&ppu);
            let mmu = Rc::new(Mmu::new(vec![ppu_clone, ram]));

            let interrupt_controller = Rc::new(InterruptController::new());
            let mut cpu = CpuState::new(Rc::clone(&mmu));
            Self {
                mmu,
                cpu: Rc::new(RefCell::new(cpu)),
                interrupt_controller,
                ppu,
            }
        }
    }

    impl Default for PpuTestSystem {
        fn default() -> Self {
            Self::new()
        }
    }

    impl GameBoyHardwareProvider for PpuTestSystem {
        fn get_mmu(&self) -> Rc<Mmu> {
            Rc::clone(&self.mmu)
        }

        fn get_ppu(&self) -> Rc<Ppu> {
            Rc::clone(&self.ppu)
        }

        fn get_cpu(&self) -> Rc<RefCell<CpuState>> {
            Rc::clone(&self.cpu)
        }

        fn get_interrupt_controller(&self) -> Rc<crate::interrupts::InterruptController> {
            Rc::clone(&self.interrupt_controller)
        }

        fn get_joypad(&self) -> Rc<Joypad> {
            panic!("Joypad not supported in this test harness")
        }
    }

    struct Rect {
        x: usize,
        y: usize,
        width: usize,
        height: usize,
    }

    impl Rect {
        fn new(x: usize, y: usize, width: usize, height: usize) -> Self {
            Self {
                x,
                y,
                width,
                height,
            }
        }

        fn contains(&self, x: usize, y: usize) -> bool {
            (x >= self.x && x < self.x + self.width) && (y >= self.y && y < self.y + self.height)
        }
    }

    const TILE_DATA_ROW_SIZE_IN_BYTES: u16 = 2;
    const TILE_HEIGHT: u16 = 8;
    const TILE_DATA_SIZE_IN_BYTES: u16 = TILE_DATA_ROW_SIZE_IN_BYTES * TILE_HEIGHT;

    fn setup_solid_tile_data(system: &PpuTestSystem, tile_base_address: u16) {
        // This tile should render with color 01
        let tile_data_byte1 = 0b00000000;
        let tile_data_byte2 = 0b11111111;
        for tile_row in 0..TILE_HEIGHT {
            let tile_row_base_address =
                tile_base_address + (tile_row * TILE_DATA_ROW_SIZE_IN_BYTES);
            system
                .get_mmu()
                .write(tile_row_base_address + 0, tile_data_byte1);
            system
                .get_mmu()
                .write(tile_row_base_address + 1, tile_data_byte2);
        }
    }

    fn assert_rect_contains_solid_tile(system: &PpuTestSystem, expected_solid_rect: Rect) {
        let ppu = system.get_ppu();
        let mut window_layer = ppu.main_window_layer.borrow_mut();
        let pixel_buffer = window_layer.get_pixel_buffer();
        let width = 160;
        let height = 144;
        for y in 0..height {
            for x in 0..width {
                let off = (width * y * 4) + (x * 4);
                if expected_solid_rect.contains(x, y) {
                    //println!("\t{x} {y} is bounded...");
                    assert_eq!(pixel_buffer[off + 0], 160);
                    assert_eq!(pixel_buffer[off + 1], 160);
                    assert_eq!(pixel_buffer[off + 2], 160);
                } else {
                    assert_eq!(pixel_buffer[off + 0], 0xff);
                    assert_eq!(pixel_buffer[off + 1], 0xff);
                    assert_eq!(pixel_buffer[off + 2], 0xff);
                }
            }
        }
    }

    fn render_frame(system: &PpuTestSystem) {
        let ppu = system.get_ppu();
        loop {
            ppu.step(system);
            if ppu.get_current_mode() == PpuMode::VBlank {
                break;
            }
        }
    }

    // TODO(PT): Also test with a negative tile index
    // TODO(PT): Also test a tile in the middle of the screen
    // TODO(PT): Also test with a negative X-scroll: do we see blank pixels before the tile that's placed at (0, 0)?

    #[test]
    fn test_ppu_background_tile_origin_signed_addressing() {
        // Given a background tile map with a tile at (0, 0)
        let system = PpuTestSystem::new();

        let tile_map_base_address = 0x9800;
        // Tile map (0, 0) should render the tile at index 5
        let tile_index = 5;
        system
            .get_mmu()
            .write(tile_map_base_address, tile_index as u8);

        // We've not modified LCD Control, so we're in signed addressing mode by default
        let tile_vram_base_address = 0x9000;
        let tile_base_address = tile_vram_base_address + (TILE_DATA_SIZE_IN_BYTES * tile_index);
        setup_solid_tile_data(&system, tile_base_address);

        render_frame(&system);

        // Now, inspect what was rendered
        let expected_tile_area = Rect::new(0, 0, 8, 8);
        assert_rect_contains_solid_tile(&system, expected_tile_area);
    }

    #[test]
    fn test_ppu_background_tile_origin_unsigned_addressing() {
        // Given a background tile map with a tile at (0, 0)
        let system = PpuTestSystem::new();

        let tile_map_base_address = 0x9800;
        // Tile map (0, 0) should render the tile at index 5
        let tile_index = 5;
        system
            .get_mmu()
            .write(tile_map_base_address, tile_index as u8);

        // Set the correct bit in the LCD control register to enable unsigned addressing mode
        system
            .get_ppu()
            .set_tile_data_addressing_mode(TileDataAddressingMode::Unsigned);

        // We've not set LCD Control, so we're in signed addressing mode
        let tile_vram_base_address = 0x8000;
        let tile_base_address = tile_vram_base_address + (TILE_DATA_SIZE_IN_BYTES * tile_index);
        setup_solid_tile_data(&system, tile_base_address);

        render_frame(&system);

        // Now, inspect what was rendered
        let expected_tile_area = Rect::new(0, 0, 8, 8);
        assert_rect_contains_solid_tile(&system, expected_tile_area);
    }

    fn write_tile_map_index(system: &PpuTestSystem, x: u16, y: u16, tile_data_index: u8) {
        let tile_map_base_address = 0x9800_u16;
        let tile_map_index = (y * 32) + x;
        system
            .get_mmu()
            .write(tile_map_base_address + tile_map_index, tile_data_index);
    }

    #[test]
    fn test_ppu_background_tile_origin_signed_addressing_offset() {
        // Given a background tile map with a tile at (5, 3)
        let system = PpuTestSystem::new();
        let tile_data_index = 100;
        write_tile_map_index(&system, 5, 3, tile_data_index);

        // We've not modified LCD Control, so we're in signed addressing mode by default
        let tile_vram_base_address = 0x9000;
        let tile_base_address =
            tile_vram_base_address + (TILE_DATA_SIZE_IN_BYTES * (tile_data_index as u16));
        setup_solid_tile_data(&system, tile_base_address);

        render_frame(&system);

        // Now, inspect what was rendered
        let expected_tile_area = Rect::new(40, 24, 8, 8);
        assert_rect_contains_solid_tile(&system, expected_tile_area);
    }

    #[test]
    fn test_ppu_background_tile_origin_signed_addressing_offset_scroll_y() {
        // TODO(PT): Continue here?
        // Given a background tile map with a tile at (5, 3)
        let system = PpuTestSystem::new();
        let tile_data_index = 100;
        write_tile_map_index(&system, 5, 3, tile_data_index);

        // We've not modified LCD Control, so we're in signed addressing mode by default
        let tile_vram_base_address = 0x9000;
        let tile_base_address =
            tile_vram_base_address + (TILE_DATA_SIZE_IN_BYTES * (tile_data_index as u16));
        setup_solid_tile_data(&system, tile_base_address);

        // Set Y scroll
        let y_scroll_offset = 5;
        {
            let ppu = system.get_ppu();
            let mut scy = ppu.scy.borrow_mut();
            *scy = y_scroll_offset as u8;
        }

        render_frame(&system);

        // Now, inspect what was rendered
        // The tile should be rendered upwards by the Y scroll offset
        let expected_tile_area = Rect::new(40, 24 - y_scroll_offset, 8, 8);
        assert_rect_contains_solid_tile(&system, expected_tile_area);
    }

    #[test]
    fn test_ppu_background_tile_origin_signed_addressing_offset_scroll_x() {
        // Given a background tile map with a tile at (5, 3)
        let system = PpuTestSystem::new();
        let tile_data_index = 100;
        write_tile_map_index(&system, 5, 3, tile_data_index);

        // We've not modified LCD Control, so we're in signed addressing mode by default
        let tile_vram_base_address = 0x9000;
        let tile_base_address =
            tile_vram_base_address + (TILE_DATA_SIZE_IN_BYTES * (tile_data_index as u16));
        setup_solid_tile_data(&system, tile_base_address);

        // Set X scroll
        let x_scroll_offset = 5;
        {
            let ppu = system.get_ppu();
            let mut scx = ppu.scx.borrow_mut();
            *scx = x_scroll_offset as u8;
        }

        render_frame(&system);

        let mut buffer: Vec<u8> = vec![0; 160 * 144 * 4];
        for y in 0..144 {
            for x in 0..160 {
                let idx = (y * 160 * 4) + (x * 4);
                buffer[idx + 3] = 0xff;
                if y % 8 == 0 || x % 8 == 0 {
                    buffer[idx + 0] = 0x55;
                    buffer[idx + 1] = 0xaa;
                    buffer[idx + 2] = 0x11;
                    buffer[idx + 3] = 0xff;
                }
            }
        }

        let ppu = system.get_ppu();
        let mut window_layer = ppu.main_window_layer.borrow_mut();
        let pixel_buffer = window_layer.get_pixel_buffer();
        let width = 160;
        let height = 144;
        for y in 0..144 {
            for x in 0..160 {
                let idx = (y * 160 * 4) + (x * 4);
                if pixel_buffer[idx + 0] != 0xff
                    || pixel_buffer[idx + 1] != 0xff
                    || pixel_buffer[idx + 2] != 0xff
                {
                    buffer[idx + 0] = pixel_buffer[idx + 0];
                    buffer[idx + 1] = pixel_buffer[idx + 1];
                    buffer[idx + 2] = pixel_buffer[idx + 2];
                }
            }
        }

        // Save the buffer as "image.png"
        image::save_buffer("image.png", &buffer, 160, 144, image::ColorType::Rgba8).unwrap();

        // Now, inspect what was rendered
        // The tile should be rendered upwards by the X scroll offset
        let expected_tile_area = Rect::new(40 - x_scroll_offset, 24, 8, 8);
        assert_rect_contains_solid_tile(&system, expected_tile_area);
    }
}
