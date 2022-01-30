use std::cell::RefCell;

use pixels::Pixels;

use crate::{
    gameboy::GameBoyHardwareProvider,
    mmu::{Addressable, Mmu},
    WINDOW_HEIGHT, WINDOW_WIDTH,
};

enum PpuState {
    OamSearch,
    PixelTransfer,
    HBlank,
    VBlank,
}

pub struct Ppu {
    pixels: RefCell<Pixels>,
    ly: RefCell<u8>,
    scy: RefCell<u8>,
    current_state: RefCell<PpuState>,
    ticks: RefCell<usize>,
    scanline_x: usize,
}

const TILE_WIDTH: usize = 8;
const TILE_HEIGHT: usize = 8;

impl Ppu {
    const SCY_ADDR: u16 = 0xff42;
    const LY_ADDR: u16 = 0xff44;

    pub fn new(pixels: Pixels) -> Self {
        Ppu {
            pixels: RefCell::new(pixels),
            ly: RefCell::new(0),
            scy: RefCell::new(0),
            current_state: RefCell::new(PpuState::OamSearch),
            ticks: RefCell::new(0),
            scanline_x: 0,
        }
    }

    fn draw_tile(&self, mmu: &Mmu, tile_idx: usize, origin_x: usize, origin_y: usize) {
        let mut pixels = self.pixels.borrow_mut();
        let mut frame = pixels.get_frame();
        let tile_size = 16;
        let vram_base = 0x8000;

        let tile_base = vram_base + (tile_idx * tile_size);

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
                let frame_idx = (point.1 * WINDOW_WIDTH * 4) + (point.0 * 4);
                println!(
                    "Pixel at ({}, {}), Tile ({}, {})",
                    point.0, point.1, px, row
                );
                frame[(frame_idx + 0) as usize] = color.0;
                frame[(frame_idx + 1) as usize] = color.1;
                frame[(frame_idx + 2) as usize] = color.2;
                frame[(frame_idx + 3) as usize] = 0xff;
            }
        }
    }

    pub fn step(&self, system: &dyn GameBoyHardwareProvider) {
        let mmu = system.get_mmu();
        let mut state = self.current_state.borrow_mut();
        let mut ticks = self.ticks.borrow_mut();
        let mut ly = self.ly.borrow_mut();
        //dbg!(*ticks, *ly);
        //println!("Y: {ly}, Ticks: {ticks}");

        *ticks += 1;
        match *state {
            PpuState::OamSearch => {
                // TODO(PT): Collect sprite data
                if *ticks == 20 {
                    //println!("OAMSearch -> PixelTransfer");
                    *state = PpuState::PixelTransfer;
                } else {
                    //println!("Do OAM search");
                }
            }
            PpuState::PixelTransfer => {
                // TODO(PT): Push pixel data
                if *ticks == 63 {
                    //println!("PixelTransfer -> HBlank");
                    *state = PpuState::HBlank;
                }
            }
            PpuState::HBlank => {
                // As a simple hack, draw the full scanline when we enter HBlank
                if *ticks == 64 {
                    let mut pixels = self.pixels.borrow_mut();
                    let mut frame = pixels.get_frame();
                    let screen_y = *ly;
                    let vram_y = screen_y + *(self.scy.borrow());
                    // Start off with a Y, which might be in the middle of a tile!
                    // Then iterate through all the X's in the scanline,
                    // but we only need to iterate with an 8-step, because we'll be drawing
                    // tile rows that are 8 pixels wide
                    for x in (0..WINDOW_WIDTH as u8).step_by(8) {
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
                        let background_map_base_address = 0x9800u16;
                        let tile_idx =
                            mmu.read(background_map_base_address + background_map_lookup_idx);
                        //println!("\tRender Tile Index {tile_idx}");

                        // We've got all the information to look up the raw tile data in the tile RAM
                        // First, compute the base address of the tile - but remember that we'll be
                        // reading a row past the tile's base
                        let tile_ram_base = 0x8000u16;
                        // Each tile is 8 rows of pixel data, where each row takes 2 bytes to store
                        // Thus, the total size of a tile is 16 bytes
                        let tile_row_size_in_bytes = 2usize;
                        let tile_size_in_bytes = tile_row_size_in_bytes * 8;
                        let tile_base_address =
                            tile_ram_base + (tile_idx as u16 * tile_size_in_bytes as u16);

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
                                0b00 => (255, 255, 255),
                                0b01 => (255, 0, 0),
                                0b10 => (0, 0, 0),
                                0b11 => (0, 0, 255),
                                _ => panic!("Invalid index"),
                            };
                            let point = ((x + tile_x as u8) as usize, screen_y as usize);
                            let frame_idx = (point.1 * WINDOW_WIDTH * 4) + (point.0 * 4);
                            /*
                            println!(
                                "\tPixel at ({}, {}), Tile ({}, {})",
                                point.0, point.1, px, y_within_tile
                            );
                            */
                            frame[(frame_idx + 0) as usize] = color.0;
                            frame[(frame_idx + 1) as usize] = color.1;
                            frame[(frame_idx + 2) as usize] = color.2;
                            frame[(frame_idx + 3) as usize] = 0xff;
                        }
                    }
                    //std::thread::sleep(std::time::Duration::new(0, 500));
                }
                // Have we reached the bottom of the screen?
                // TODO(PT): Wait, then go back to sprite search for the next line, or vblank
                if *ticks == 114 {
                    *ticks = 0;
                    *ly += 1;
                    if *ly == (WINDOW_HEIGHT as u8) {
                        //println!("Reached screen bottom");
                        self.pixels.borrow_mut().render().unwrap();
                        //println!("HBlank -> VBlank");
                        *state = PpuState::VBlank;
                    } else {
                        //println!("HBlank -> OAMSearch");
                        *state = PpuState::OamSearch;
                    }
                }
            }
            PpuState::VBlank => {
                // TODO(PT): Wait, then go back to sprite search for the top line
                if *ticks == 114 {
                    *ly += 1;
                    *ticks = 0;
                    if *ly == 153 {
                        *ly = 0;
                        //println!("VBlank -> OAMSearch");
                        *state = PpuState::OamSearch;

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
                    }
                }
            }
        }
    }
}

impl Addressable for Ppu {
    fn contains(&self, addr: u16) -> bool {
        match addr {
            Ppu::LY_ADDR => true,
            Ppu::SCY_ADDR => true,
            _ => false,
        }
    }

    fn read(&self, addr: u16) -> u8 {
        match addr {
            Ppu::LY_ADDR => *self.ly.borrow(),
            Ppu::SCY_ADDR => *self.scy.borrow(),
            _ => panic!("Unrecognised address"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            Ppu::LY_ADDR => {
                panic!("Writes to LY are not allowed")
            }
            Ppu::SCY_ADDR => *(self.scy.borrow_mut()) = val,
            _ => panic!("Unrecognised address"),
        }
    }
}
