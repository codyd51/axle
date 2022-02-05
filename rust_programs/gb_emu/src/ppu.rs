use std::cell::RefCell;

use bitmatch::bitmatch;
use pixels::Pixels;

use crate::{
    gameboy::GameBoyHardwareProvider,
    interrupts::InterruptType,
    mmu::{Addressable, Mmu},
    WINDOW_HEIGHT, WINDOW_WIDTH,
};

#[derive(Copy, Clone, PartialEq)]
enum PpuMode {
    OamSearch,
    PixelTransfer,
    HBlank,
    VBlank,
}

pub struct Ppu {
    pixels: RefCell<Pixels>,
    vram_debug_pixels: RefCell<Pixels>,
    lcd_control: RefCell<u8>,
    ly: RefCell<u8>,
    lyc: RefCell<u8>,
    scy: RefCell<u8>,
    scx: RefCell<u8>,
    stat: RefCell<u8>,
    current_mode: RefCell<PpuMode>,
    ticks: RefCell<usize>,
    scanline_x: usize,
}

const TILE_WIDTH: usize = 8;
const TILE_HEIGHT: usize = 8;

enum StatusUpdate {
    LycEqualsLy(bool),
    Mode(PpuMode),
}

impl Ppu {
    const LCD_CONTROL_ADDR: u16 = 0xff40;
    const STAT_ADDR: u16 = 0xff41;
    const SCY_ADDR: u16 = 0xff42;
    const SCX_ADDR: u16 = 0xff43;
    const LY_ADDR: u16 = 0xff44;
    const LYC_ADDR: u16 = 0xff45;

    pub fn new(pixels: Pixels, vram_debug_pixels: Pixels) -> Self {
        Ppu {
            pixels: RefCell::new(pixels),
            vram_debug_pixels: RefCell::new(vram_debug_pixels),
            lcd_control: RefCell::new(0b10000000),
            ly: RefCell::new(0),
            lyc: RefCell::new(0),
            scy: RefCell::new(0),
            scx: RefCell::new(0),
            stat: RefCell::new(0),
            current_mode: RefCell::new(PpuMode::OamSearch),
            ticks: RefCell::new(0),
            scanline_x: 0,
        }
    }

    fn draw_tile(&self, mmu: &Mmu, tile_idx: usize, origin_x: usize, origin_y: usize) {
        let mut pixels = self.vram_debug_pixels.borrow_mut();
        let mut frame = pixels.get_frame();
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
    }

    fn draw_sprite(&self, mmu: &Mmu, tile_idx: usize, origin_x: usize, origin_y: usize) {
        //let mut pixels = self.vram_debug_pixels.borrow_mut();
        let mut pixels = self.pixels.borrow_mut();
        let mut frame = pixels.get_frame();
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
    }

    pub fn update_status(&self, status: StatusUpdate) {
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
            }
        }
    }

    fn set_mode(&self, system: &dyn GameBoyHardwareProvider, state: PpuMode) {
        self.update_status(StatusUpdate::Mode(state));
        *self.current_mode.borrow_mut() = state;

        if state == PpuMode::VBlank {
            //println!("PPU requested VBlank interrupt...");
            system
                .get_interrupt_controller()
                .trigger_interrupt(InterruptType::VBlank);
            self.blit_to_os_window();
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
    fn tile_map_base_address(&self) -> u16 {
        let lcd_control = *self.lcd_control.borrow();
        #[bitmatch]
        match lcd_control {
            "xxxx0xxx" => 0x9800,
            "xxxx1xxx" => 0x9c00,
        }
    }

    pub fn blit_to_os_window(&self) {
        self.pixels.borrow().render().unwrap();
    }

    pub fn step(&self, system: &dyn GameBoyHardwareProvider) {
        let mmu = system.get_mmu();
        let mut ticks = self.ticks.borrow_mut();
        let mut ly = self.ly.borrow_mut();
        //dbg!(*ticks, *ly);
        //println!("Y: {ly}, Ticks: {ticks}");

        // Reached LYC?
        if *ly == *self.lyc.borrow_mut() {
            // TODO(PT): Only do this if this INT is enabled
            //println!("LYC == LY");
            system
                .get_interrupt_controller()
                .trigger_interrupt(InterruptType::LCDStat);
        }

        *ticks += 1;
        match self.current_mode() {
            PpuMode::OamSearch => {
                // TODO(PT): Collect sprite data
                if *ticks == 20 {
                    //println!("OAMSearch -> PixelTransfer");
                    //*state = PpuMode::PixelTransfer;
                    self.set_mode(system, PpuMode::PixelTransfer);
                } else {
                    //println!("Do OAM search");
                }
            }
            PpuMode::PixelTransfer => {
                // TODO(PT): Push pixel data
                if *ticks == 63 {
                    //println!("PixelTransfer -> HBlank");
                    //*state = PpuMode::HBlank;
                    self.set_mode(system, PpuMode::HBlank);
                }
            }
            PpuMode::HBlank => {
                // As a simple hack, draw the full scanline when we enter HBlank
                if *ticks == 64 {
                    let mut pixels = self.pixels.borrow_mut();
                    let mut frame = pixels.get_frame();
                    let screen_y = *ly;
                    let vram_y =
                        ((screen_y as u16 + *(self.scy.borrow()) as u16) % 256 as u16) as u8;
                    //let vram_y = (screen_y + *(self.scy.borrow()));
                    //let vram_y = (((screen_y as u16) + 140) % 256 as u16) as u8;
                    /*
                    println!(
                        "Screen Y {screen_y} VRam Y {vram_y} Scroll {}",
                        *(self.scy.borrow())
                    );
                    */
                    // Start off with a Y, which might be in the middle of a tile!
                    // Then iterate through all the X's in the scanline,
                    // but we only need to iterate with an 8-step, because we'll be drawing
                    // tile rows that are 8 pixels wide
                    for screen_x in (0..WINDOW_WIDTH as u8).step_by(8) {
                        let x =
                            ((screen_x as u16 + *(self.scx.borrow()) as u16) % 256 as u16) as u8;
                        //
                        // The game will maintain a 32x32 grid of where each tile should be placed
                        // Within this grid, the game only places 'tile indexes' rather than tile data
                        // There's a maximum of 256 tiles, and so this tile index takes up a byte.
                        //
                        // First things first: Let's look up what tile we should be rendering, given
                        // this (x, y) pair.
                        // Note that while the X is definitely at the start of the tile, the Y can be
                        // anywhere within the tile.
                        // Let's re-adjust the Y so it sits at a tile boundary, for purposes of finding
                        // the tile to place.
                        let tile_base_y = vram_y & !(8 - 1);
                        //println!("X {x}, Y {vram_y}, TileBaseY {tile_base_y}");
                        // We've now got a tile base coordinate in the screen coordinate system,
                        // a 256x256 grid.
                        // Now, convert this to the 32x32 lookup grid coordinates.
                        let (background_map_lookup_x, background_map_lookup_y) =
                            (x / 8, tile_base_y / 8);
                        /*
                        println!(
                            "\tBackgroundMap ({background_map_lookup_x}, {background_map_lookup_y})"
                        );
                        */
                        // The background tile map is really a linear array, rather than a 32x32 grid.
                        // Convert our tile map coordinate to an index
                        let background_map_lookup_idx =
                            (background_map_lookup_y as u16 * 32) + background_map_lookup_x as u16;
                        // Now that we've got an index into the background tile map, look up the
                        // tile index that should be rendered here
                        let background_map_base_address = self.tile_map_base_address();
                        let tile_idx =
                            mmu.read(background_map_base_address + background_map_lookup_idx);

                        //println!("\tRender Tile Index {tile_idx}");

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
                                //println!("Using signed addressing!");
                                0x9000_u16.wrapping_add(
                                    (((tile_idx as i8) as u16)
                                        .wrapping_mul(tile_size_in_bytes as u16)),
                                )
                            }
                            _ => tile_ram_base + (tile_idx as u16 * tile_size_in_bytes as u16),
                        };

                        // We're really interested in the start of the row we're going to draw
                        let y_within_tile = vram_y - tile_base_y;
                        let row_base_address = tile_base_address
                            + (y_within_tile as u16 * tile_row_size_in_bytes as u16);
                        /*
                        println!(
                            "\tTile row #{y_within_tile} data starts at 0x{row_base_address:04x}"
                        );
                        */
                        // The tile row data takes 2 bytes to store
                        let row_byte1 = mmu.read(row_base_address);
                        let row_byte2 = mmu.read(row_base_address + 1);
                        //println!("\tTile data: {row_byte1:02x}:{row_byte2:02x}");

                        // We've now got the tile row data to render to this scanline!
                        for tile_x in 0..8 {
                            let px_color_id = ((row_byte1 >> (TILE_WIDTH - tile_x - 1)) & 0b1) << 1
                                | ((row_byte2 >> (TILE_WIDTH - tile_x - 1)) & 0b1);
                            let color = match px_color_id {
                                // Black
                                0b11 => (0, 0, 0),
                                0b10 => (80, 80, 80),
                                0b01 => (160, 160, 160),
                                0b00 => (255, 255, 255),
                                // Green
                                /*
                                0b00 => (20, 50, 20),
                                0b01 => (45, 90, 45),
                                0b10 => (110, 140, 40),
                                0b11 => (145, 200, 45),
                                */
                                /*
                                // Pink
                                0b00 => (105, 2, 105),
                                0b01 => (171, 50, 171),
                                0b10 => (189, 115, 189),
                                0b11 => (245, 184, 245),
                                */
                                _ => panic!("Invalid index"),
                            };
                            let point = ((screen_x + tile_x as u8) as usize, screen_y as usize);
                            let frame_idx = (point.1 * WINDOW_WIDTH * 4) + (point.0 * 4);
                            frame[(frame_idx + 0) as usize] = color.0;
                            frame[(frame_idx + 1) as usize] = color.1;
                            frame[(frame_idx + 2) as usize] = color.2;
                            frame[(frame_idx + 3) as usize] = 0xff;
                        }
                    }

                    // Now, draw all the sprite scanlines that should be rendered here
                    // Firstly, let's iterate OAM to pull all the sprites that overlap with
                    // this scanline.
                    // TODO(PT): For fidelity, this search should actually be done in the
                    // OAMSearch screen state.
                    let oam_base = 0xfe00;
                    // OAM entry format:
                    // (Y, X, Tile Index, Attributes)
                    let oam_entry_size = 4;
                    let oam_entry_count = 40;
                    for i in 0..oam_entry_count {
                        let oam_entry_addr = oam_base + (oam_entry_size * i);

                        // Hack to clean up debug prints
                        /*
                        if mmu.read(oam_entry_addr + 2) == 0 {
                            continue;
                        }
                        */

                        // Check whether the current row we're drawing is within this tile
                        // Note that the Y origin in the OAM data is 16 less than the screen's origin
                        let oam_entry_start_y = mmu.read(oam_entry_addr + 0);
                        //let oam_entry_start_y = 12;
                        let oam_entry_start_y_to_screen = (oam_entry_start_y as i16) - 16;

                        // Is the row we're currently drawing within this tile?
                        if (screen_y as i16) < oam_entry_start_y_to_screen
                            || (screen_y as i16) >= oam_entry_start_y_to_screen + 8
                        {
                            continue;
                        }
                        //println!("ScreenY {screen_y} should draw tile that starts at {oam_entry_start_y_to_screen}");

                        // Note that the X origin in the OAM data is 8 less than the screen's origin
                        let oam_entry_start_x = mmu.read(oam_entry_addr + 1);
                        //let oam_entry_start_x = 4;
                        let oam_entry_start_x_to_screen = (oam_entry_start_x as i16) - 8;

                        // The third byte in the OAM entry gives us the index into the tile map of
                        // what should be rendered
                        let oam_entry_tile_index = mmu.read(oam_entry_addr + 2);

                        // Each tile is 8 rows of pixel data, where each row takes 2 bytes to store
                        // Thus, the total size of a tile is 16 bytes
                        let tile_row_size_in_bytes = 2_u16;
                        let tile_size_in_bytes = tile_row_size_in_bytes * 8;
                        let sprite_vram_base = 0x8000_u16;
                        let tile_base =
                            sprite_vram_base + ((oam_entry_tile_index as u16) * tile_size_in_bytes);

                        // Iterate each row of tile data
                        for tile_row in 0..8 {
                            // Skip any off-screen rows
                            let tile_row_to_screen = oam_entry_start_y_to_screen + tile_row;
                            if tile_row_to_screen < 0
                                || tile_row_to_screen >= (WINDOW_HEIGHT as i16)
                            {
                                continue;
                            }

                            // We're really interested in the start of the row we're going to draw
                            let row_base_address =
                                tile_base + ((tile_row as u16) * (tile_row_size_in_bytes as u16));
                            // Remember that the tile row data takes 2 bytes to store
                            let tile_row_byte1 = mmu.read(row_base_address);
                            let tile_row_byte2 = mmu.read(row_base_address + 1);

                            // Draw each sprite pixel in this row
                            for tile_col in 0..8 {
                                // Skip any off-screen columns
                                let tile_col_to_screen = oam_entry_start_x_to_screen + tile_col;
                                if tile_col_to_screen < 0
                                    || tile_col_to_screen >= (WINDOW_WIDTH as i16)
                                {
                                    continue;
                                }

                                let px_color_id = ((tile_row_byte1
                                    >> (TILE_WIDTH - (tile_col as usize) - 1))
                                    & 0b1)
                                    << 1
                                    | ((tile_row_byte2 >> (TILE_WIDTH - (tile_col as usize) - 1))
                                        & 0b1);
                                // For sprites, a color code of 0 means transparent
                                if px_color_id == 0b00 {
                                    continue;
                                }
                                let color = match px_color_id {
                                    // Green
                                    0b01 => (45, 90, 45),
                                    0b10 => (110, 140, 40),
                                    0b11 => (145, 200, 45),
                                    _ => panic!("Invalid index"),
                                };
                                let px_x = (oam_entry_start_x_to_screen + tile_col) as usize;
                                let px_y = (oam_entry_start_y_to_screen + tile_row) as usize;
                                let frame_idx = (px_y * WINDOW_WIDTH * 4) + (px_x * 4);
                                frame[(frame_idx + 0) as usize] = color.0;
                                frame[(frame_idx + 1) as usize] = color.1;
                                frame[(frame_idx + 2) as usize] = color.2;
                                frame[(frame_idx + 3) as usize] = 0xff;
                            }
                        }
                    }
                }
                // Have we reached the bottom of the screen?
                // TODO(PT): Wait, then go back to sprite search for the next line, or vblank
                if *ticks == 114 {
                    *ticks = 0;
                    *ly += 1;

                    if *ly == (WINDOW_HEIGHT as u8) {
                        //println!("Reached screen bottom");
                        //self.pixels.borrow_mut().render().unwrap();
                        //println!("HBlank -> VBlank");
                        //*state = PpuMode::VBlank;
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
                        self.vram_debug_pixels.borrow().render().unwrap();

                        /*
                        // Draw the sprites
                        let oam_base = 0xfe00;
                        for sprite_index in (0..40 as u16)
                        /*.step_by(4)*/
                        {
                            let oam_offset = oam_base + (sprite_index * 4);
                            //println!("OAM addr {oam_offset:04x}");
                            let sprite_y = mmu.read(oam_offset + 0);
                            let sprite_x = mmu.read(oam_offset + 1);
                            let tile_index = mmu.read(oam_offset + 2);
                            //let tile_index = sprite_index;
                            //if tile_index < 255 && sprite_x < 248 && sprite_y < 248 {
                            if tile_index < 255 && sprite_x >= 8 && sprite_y >= 16 {
                                println!("Tile at {sprite_x}, {sprite_y}, index {tile_index}, address {oam_offset:04x}");
                                self.draw_sprite(
                                    &mmu,
                                    tile_index as usize,
                                    (sprite_x - 8).into(),
                                    (sprite_y - 16).into(),
                                );
                            }
                        }
                        */

                        // Blit the screen
                        //self.pixels.borrow().render().unwrap();
                    } else {
                        //println!("HBlank -> OAMSearch");
                        //*state = PpuMode::OamSearch;
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
                        //println!("VBlank -> OAMSearch");
                        //*state = PpuMode::OamSearch;
                        self.set_mode(system, PpuMode::OamSearch);

                        /*
                        let tile_map_row_size = 32;
                        for tile_map_byte_idx in 0..0x400 {
                            let tile_map_byte_addr = tile_map_byte_idx + 0x9800;
                            let col = tile_map_byte_idx % tile_map_row_size;
                            let row = tile_map_byte_idx / tile_map_row_size;
                            //let tile_map_byte: u8 = cpu.memory.read(tile_map_byte_addr);
                            let tile_map_byte = mmu.read(tile_map_byte_addr);
                            let x = col * 8;
                            let y = row * 8;
                            if x < 160 && y < 140 {
                                /*
                                println!(
                                    "Tile map byte @ {tile_map_byte_addr:04x}: {:02x} at ({}, {}), ({x}, {y})",
                                    tile_map_byte, col, row
                                );
                                */
                                self.draw_tile(mmu, tile_map_byte as usize, x as usize, y as usize);
                            }
                        }

                        for tile in 0..30 {
                            let tiles_per_row = WINDOW_WIDTH / TILE_WIDTH;
                            let col = tile % tiles_per_row;
                            let row = tile / tiles_per_row;
                            let origin = (col * TILE_WIDTH, row * TILE_HEIGHT);
                            //draw_tile(frame, cpu, tile, origin.0, origin.1);
                            self.draw_tile(mmu, tile, origin.0, origin.1);
                        }

                        self.pixels.borrow().render().unwrap();
                        */

                        //self.vram_debug_pixels.borrow().render().unwrap();
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
            Ppu::SCX_ADDR => {
                if *(self.scx.borrow()) != val {
                    //§println!("Setting new scx {val}");
                }
                if val != 0 {
                    *(self.scx.borrow_mut()) = val
                }
            }
            Ppu::STAT_ADDR => {
                // The bottom 3 bits are read-only, so mask them off the requested write
                let masked_write = val & !(0b111);
                // Preserve the current bottom 3 bits
                let mut stat = self.stat.borrow_mut();
                let masked_current_value = *stat & 0b111;
                let new_value = masked_write | masked_current_value;
                //println!("Wrote to LCD STAT register: {val:08b} {new_value:08b}");
                *stat = new_value
            }
            _ => panic!("Unrecognised address"),
        }
    }
}
