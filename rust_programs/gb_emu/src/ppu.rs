use std::cell::RefCell;

use pixels::Pixels;

use crate::{
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
    current_state: RefCell<PpuState>,
    ticks: RefCell<usize>,
    scanline_x: usize,
}

const TILE_WIDTH: usize = 8;
const TILE_HEIGHT: usize = 8;

impl Ppu {
    const LY_ADDR: u16 = 0xff44;

    pub fn new(pixels: Pixels) -> Self {
        Ppu {
            pixels: RefCell::new(pixels),
            ly: RefCell::new(0),
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
                frame[(frame_idx + 0) as usize] = color.0;
                frame[(frame_idx + 1) as usize] = color.1;
                frame[(frame_idx + 2) as usize] = color.2;
                frame[(frame_idx + 3) as usize] = 0xff;
            }
        }
    }

    pub fn step(&self, mmu: &Mmu) {
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
                // Have we reached the bottom of the screen?
                // TODO(PT): Wait, then go back to sprite search for the next line, or vblank
                if *ticks == 114 {
                    *ticks = 0;
                    *ly += 1;
                    if *ly == (WINDOW_HEIGHT as u8) {
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

                        //self.pixels.borrow().render().unwrap();

                        /*
                        for tile_map_row in 0..32 {
                            for tile_map_col in 0..32 {
                                let tile_map_byte_addr = tile_map_byte_
                            }
                        }
                        */

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
                    }
                }
            }
        }
    }
}

impl Addressable for Ppu {
    fn contains(&self, addr: u16) -> bool {
        addr == Ppu::LY_ADDR
    }

    fn read(&self, addr: u16) -> u8 {
        match addr {
            Ppu::LY_ADDR => *self.ly.borrow(),
            _ => panic!("Unrecognised address"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            Ppu::LY_ADDR => *(self.ly.borrow_mut()) = val,
            _ => panic!("Unrecognised address"),
        }
    }
}
