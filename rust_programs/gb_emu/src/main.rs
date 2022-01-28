/*
#[cfg_attr(not(target_os = "axle"), no_std)]
mod cpu;

extern crate alloc;

extern crate std;
#[cfg(not(target_os = "axle"))]
use std::println;

use cpu::CpuState;
*/
extern crate alloc;

mod cpu;
mod mmu;
use std::{
    io::Write,
    rc::{Rc, Weak},
    time::Duration,
};

use cpu::CpuState;
use mmu::{Addressable, BootRom, GameRom, Mmu};
use pixels::{Error, Pixels, SurfaceTexture};
use winit::dpi::LogicalSize;
use winit::event::{Event, VirtualKeyCode};
use winit::event_loop::{ControlFlow, EventLoop};
use winit::window::WindowBuilder;
use winit_input_helper::WinitInputHelper;

const WINDOW_WIDTH: usize = 160;
const WINDOW_HEIGHT: usize = 144;
const BOX_SIZE: i16 = 64;

#[derive(Debug)]
struct Breakpoint {
    address: u16,
    is_temporary: bool,
}

impl Breakpoint {
    fn new(address: u16, is_temporary: bool) -> Self {
        Self {
            address,
            is_temporary,
        }
    }
}

const TILE_WIDTH: usize = 8;
const TILE_HEIGHT: usize = 8;

fn draw_tile(
    frame: &mut [u8],
    cpu: &mut CpuState,
    tile_idx: usize,
    origin_x: usize,
    origin_y: usize,
) {
    let tile_size = 16;
    let vram_base = 0x8000;

    let tile_base = vram_base + (tile_idx * tile_size);

    /*
    for row in 0..TILE_HEIGHT {
        let row_byte1 = cpu
            .memory
            .read::<u8>((tile_base + (2 * row) + 0).try_into().unwrap());
        let row_byte2 = cpu
            .memory
            .read::<u8>((tile_base + (2 * row) + 1).try_into().unwrap());
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
    */
}

fn draw(frame: &mut [u8], cpu: &mut CpuState) {
    //println!("Redrawing...");
    for x in 0..WINDOW_WIDTH {
        for y in 0..WINDOW_HEIGHT {
            let frame_idx = (y * WINDOW_WIDTH * 4) + (x * 4);
            frame[(frame_idx + 0) as usize] = 255;
            frame[(frame_idx + 1) as usize] = 255;
            frame[(frame_idx + 2) as usize] = 255;
            frame[(frame_idx + 3) as usize] = 0xff;
        }
    }

    /*
    for tile in 0..255 {
        let tiles_per_row = WINDOW_WIDTH / TILE_WIDTH;
        let col = tile % tiles_per_row;
        let row = tile / tiles_per_row;
        let origin = (col * TILE_WIDTH, row * TILE_HEIGHT);
        draw_tile(frame, cpu, tile, origin.0, origin.1);
    }
    */

    /*
    // Draw vertical lines
    //for column in 0..tiles_per_row {
    for column in 0..(WINDOW_WIDTH / TILE_WIDTH) {
        let x = column * TILE_WIDTH;
        for y in 0..WINDOW_HEIGHT {
            let frame_idx = (y * WINDOW_WIDTH * 4) + (x * 4);
            frame[(frame_idx + 0) as usize] = 200;
            frame[(frame_idx + 1) as usize] = 200;
            frame[(frame_idx + 2) as usize] = 100;
            frame[(frame_idx + 3) as usize] = 0xff;
        }
    }

    // Draw horizontal lines
    for row in 0..(WINDOW_HEIGHT / TILE_HEIGHT) {
        let y = row * 8;
        for x in 0..WINDOW_WIDTH {
            let frame_idx = (y * WINDOW_WIDTH * 4) + (x * 4);
            frame[(frame_idx + 0) as usize] = 200;
            frame[(frame_idx + 1) as usize] = 200;
            frame[(frame_idx + 2) as usize] = 100;
            frame[(frame_idx + 3) as usize] = 0xff;
        }
    }
    */

    //$9800-$9BFF
    /*
    let tile_map_row_size = 32;
    //for tile_map_byte_addr in 0x9800..0x9c00 {
    for tile_map_byte_idx in 0..0x400 {
        let tile_map_byte_addr = tile_map_byte_idx + 0x9800;
        let col = tile_map_byte_idx % tile_map_row_size;
        let row = tile_map_byte_idx / tile_map_row_size;
        let tile_map_byte: u8 = cpu.memory.read(tile_map_byte_addr);
        if tile_map_byte != 0 && tile_map_byte != 0x22 {
            /*
            println!(
                "Tile map byte @ {tile_map_byte_addr:04x}: {:02x} at ({}, {})",
                tile_map_byte, col, row
            );
            */
            let x = col * 8;
            let y = row * 8;
            draw_tile(frame, cpu, tile_map_byte as usize, x as usize, y as usize);
        }
    }
    */
}

fn main2() {
    let event_loop = EventLoop::new();
    let mut input = WinitInputHelper::new();
    let window = {
        //let size = LogicalSize::new(WINDOW_WIDTH as f64, WINDOW_HEIGHT as f64);
        let size = LogicalSize::new(WINDOW_WIDTH as f64, WINDOW_HEIGHT as f64);
        let scaled_size = LogicalSize::new(WINDOW_WIDTH as f64 * 3.0, WINDOW_HEIGHT as f64 * 3.0);
        WindowBuilder::new()
            .with_title("GameBoy")
            //.with_inner_size(size)
            .with_inner_size(scaled_size)
            .with_min_inner_size(size)
            .with_visible(true)
            .with_resizable(true)
            .build(&event_loop)
            .unwrap()
    };

    let mut pixels = {
        let window_size = window.inner_size();
        let surface_texture = SurfaceTexture::new(window_size.width, window_size.height, &window);
        Pixels::new(
            WINDOW_WIDTH.try_into().unwrap(),
            WINDOW_HEIGHT.try_into().unwrap(),
            surface_texture,
        )
        .unwrap()
    };

    /*
    let mut cpu = CpuState::new();
    let bootrom = std::fs::read("/Users/philliptennen/Downloads/DMG_ROM.bin").unwrap();
    cpu.load_bootrom(bootrom);
    // TODO(PT): For the bootrom to succeed, 0x104 must contain the Nintendo logo:
    // https://gbdev.gg8.se/wiki/articles/The_Cartridge_Header
    //cpu.enable_debug();
    cpu.print_regs();
    for i in 0x8000..0xa000 {
        //rom[i] = 0xff;
        cpu.memory.write_u8(i, 0x22);
    }
    let cart_logo = [
        0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0c, 0x00,
        0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e, 0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd,
        0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc, 0xdd, 0xdc, 0x99, 0x9f, 0xbb,
        0xb9, 0x33, 0x3e,
    ];
    let header_start = 0x104;
    for i in 0..cart_logo.len() {
        let ptr = header_start + i;
        cpu.memory.write_u8(ptr.try_into().unwrap(), cart_logo[i]);
    }

    let mut breakpoints = Vec::new();
    //breakpoints.push(0x0);
    breakpoints.push(Breakpoint::new(0, true));
    //cpu.enable_debug();

    let mut i = 0;
    let mut delay = 0;
    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Poll;

        // Resize the window
        // TODO(PT): Resizing doesn't work
        if let Some(size) = input.window_resized() {
            println!("RESIZING");
            pixels.resize_surface(size.width, size.height);
            window.request_redraw();
        }

        match event {
            Event::RedrawRequested(_) => {
                draw(pixels.get_frame(), &mut cpu);
                pixels.render().unwrap();
            }
            Event::MainEventsCleared => {
                i += 1;
                cpu.step();
                //if i % WINDOW_WIDTH == 0 {
                if i % 400 == 0 {
                    draw(pixels.get_frame(), &mut cpu);
                    pixels.render().unwrap();
                }
                /*
                if delay != 0 {
                    println!("Sleeping...");
                    std::thread::sleep(Duration::from_millis(delay));
                }
                */
                /*
                let mut brk = None;
                let mut brk_idx = 0;
                for (i, b) in breakpoints.iter().enumerate() {
                    if b.address == cpu.get_pc() {
                        brk = Some((b, i));
                        break;
                    }
                }
                //if breakpoints.contains(&cpu.get_pc()) {
                if let Some(brk_and_idx) = brk {
                    println!("Breakpoint at 0x{:04x}", brk_and_idx.0.address);
                    print!("> ");
                    std::io::stdout().flush().unwrap();
                    if brk_and_idx.0.is_temporary {
                        //println!("Dropping temp breakpoint {:?}", brk);
                        breakpoints.swap_remove(brk_and_idx.1);
                    }

                    loop {
                        let mut stdin = std::io::stdin();
                        let input = &mut String::new();
                        stdin.read_line(input);
                        //println!("Read line {input}");
                        let mut toks = input.split(' ').fuse();
                        let cmd = toks.next();
                        //println!("Handling cmd {}", cmd.unwrap());
                        match cmd {
                            Some("b") => {
                                let mut breakpoint_address_str = toks.next().unwrap();
                                breakpoint_address_str =
                                    breakpoint_address_str.trim_end_matches('\n');
                                //println!("{breakpoint_address_str}");
                                let breakpoint_address =
                                    u16::from_str_radix(breakpoint_address_str, 16).unwrap();
                                breakpoints.push(Breakpoint::new(breakpoint_address, false));
                                break;
                            }
                            Some("s\n") => {
                                cpu.step();
                                //breakpoints.push(cpu.get_pc());
                                breakpoints.push(Breakpoint::new(cpu.get_pc(), true));
                                //println!("Set step breakpoint at {:04x}", cpu.get_pc());
                                break;
                            }
                            Some("regs\n") | Some("r\n") => {
                                cpu.print_regs();
                                break;
                            }
                            Some("c\n") => {
                                cpu.step();
                                break;
                            }
                            _ => {
                                println!("Unknown command {}", cmd.unwrap());
                                print!("> ");
                                std::io::stdout().flush().unwrap();
                            }
                        }
                    }
                } else {
                    //println!("Stepping...");
                    cpu.step();
                }
                window.request_redraw();
                */
            }
            _ => {}
        }
    });
    */
}

fn main() {
    /*
    let bootrom = BootRom::new("/Users/philliptennen/Downloads/DMG_ROM.bin");
    let game_rom = GameRom::new("/Users/philliptennen/Downloads/Tetris (World).gb");
    let mmu = Mmu::new(vec![&bootrom, &game_rom]);
    let cpu = CpuState::new(&mmu);
    */
}
