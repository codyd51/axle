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
mod gameboy;
mod interrupts;
mod joypad;
mod mmu;
mod ppu;
use std::{
    cell::RefCell,
    io::Write,
    rc::{Rc, Weak},
    time::Duration,
};

use cpu::CpuState;
use gameboy::GameBoy;
use interrupts::InterruptController;
use joypad::Joypad;
use mmu::{Addressable, BootRom, EchoRam, GameRom, Mmu, Ram};
use pixels::{Error, Pixels, SurfaceTexture};
use ppu::Ppu;
use winit::dpi::LogicalSize;
use winit::event::{Event, VirtualKeyCode};
use winit::event_loop::{ControlFlow, EventLoop};
use winit::window::WindowBuilder;

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

fn main2() {
    let event_loop = EventLoop::new();
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
    let event_loop = EventLoop::new();
    let window = {
        let size = LogicalSize::new(WINDOW_WIDTH as f64, WINDOW_HEIGHT as f64);
        let scaled_size = LogicalSize::new(WINDOW_WIDTH as f64 * 3.0, WINDOW_HEIGHT as f64 * 3.0);
        WindowBuilder::new()
            .with_title("GameBoy")
            //.with_inner_size(size)
            .with_inner_size(scaled_size)
            .with_min_inner_size(scaled_size)
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
    pixels.render().unwrap();

    let bootrom = Rc::new(BootRom::new("/Users/philliptennen/Downloads/DMG_ROM.bin"));
    let ppu = Rc::new(Ppu::new(pixels));
    let ppu_clone = Rc::clone(&ppu);
    let game_rom = Rc::new(GameRom::new(
        "/Users/philliptennen/Downloads/Tetris (World).gb",
    ));
    let tile_ram = Rc::new(Ram::new(0x8000, 0x1800));
    let background_map = Rc::new(Ram::new(0x9800, 0x800));

    let working_ram = Rc::new(Ram::new(0xc000, 0x2000));
    let working_ram_clone = Rc::clone(&working_ram);
    let echo_ram = Rc::new(EchoRam::new(working_ram_clone, 0xe000, 0x1e00));
    let high_ram = Rc::new(Ram::new(0xff80, 0x7f));

    let interrupt_controller = Rc::new(InterruptController::new());
    let interrupt_controller_clone = Rc::clone(&interrupt_controller);

    let joypad = Rc::new(Joypad::new());

    let mmu = Rc::new(Mmu::new(vec![
        bootrom,
        interrupt_controller,
        joypad,
        ppu,
        tile_ram,
        background_map,
        working_ram,
        high_ram,
        echo_ram,
        game_rom,
    ]));
    let mut cpu = CpuState::new(Rc::clone(&mmu));
    cpu.enable_debug();
    let gameboy = GameBoy::new(Rc::clone(&mmu), cpu, ppu_clone, interrupt_controller_clone);

    // Ref: https://users.rust-lang.org/t/winit-0-20-the-state-of-window/29485/28
    // Ref: https://github.com/rust-windowing/winit/blob/master/examples/window_run_return.rs
    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Poll;

        match event {
            Event::MainEventsCleared => {
                for i in 0..128 {
                    gameboy.step();
                }
            }
            _ => {}
        }
    });
}
