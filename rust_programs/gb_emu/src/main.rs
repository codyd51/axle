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
mod serial;
mod timer;
use std::{
    cell::RefCell,
    io::Write,
    num::ParseIntError,
    rc::{Rc, Weak},
    time::Duration,
};

use cpu::CpuState;
use gameboy::{GameBoy, GameBoyHardwareProvider};
use interrupts::InterruptController;
use joypad::{Button, Joypad};
use mmu::{Addressable, BootRom, DmaController, EchoRam, GameRom, Mmu, Ram};
use pixels::{Error, Pixels, SurfaceTexture};
use ppu::Ppu;
use serial::SerialDebugPort;
use timer::Timer;
use winit::event::{ElementState, Event, VirtualKeyCode};
use winit::event_loop::{ControlFlow, EventLoop};
use winit::window::WindowBuilder;
use winit::{
    dpi::{LogicalPosition, LogicalSize},
    event::WindowEvent,
};

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

fn main_gfx() {
    let event_loop = EventLoop::new();

    let vram_debug_window = {
        let size = LogicalSize::new(256 as f64, 256 as f64);
        let scaled_size = LogicalSize::new(256 as f64 * 1.5, 256 as f64 * 1.5);
        WindowBuilder::new()
            .with_title("VRAM Viewer")
            .with_inner_size(scaled_size)
            .with_min_inner_size(scaled_size)
            .with_visible(true)
            .with_resizable(false)
            .with_position(LogicalPosition::new(100, 100))
            .build(&event_loop)
            .unwrap()
    };

    let mut vram_debug_pixels = {
        let window_size = vram_debug_window.inner_size();
        let surface_texture =
            SurfaceTexture::new(window_size.width, window_size.height, &vram_debug_window);
        Pixels::new(
            256.try_into().unwrap(),
            256.try_into().unwrap(),
            surface_texture,
        )
        .unwrap()
    };
    vram_debug_pixels.render().unwrap();

    let window = {
        let size = LogicalSize::new(WINDOW_WIDTH as f64, WINDOW_HEIGHT as f64);
        let scaled_size = LogicalSize::new(WINDOW_WIDTH as f64 * 3.0, WINDOW_HEIGHT as f64 * 3.0);
        WindowBuilder::new()
            .with_title("GameBoy")
            //.with_inner_size(size)
            .with_inner_size(scaled_size)
            .with_min_inner_size(scaled_size)
            .with_visible(true)
            .with_resizable(false)
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
    let ppu = Rc::new(Ppu::new(pixels, vram_debug_pixels));
    let ppu_clone = Rc::clone(&ppu);
    let rom_path = &std::env::args().collect::<Vec<String>>()[1];
    let game_rom = Rc::new(GameRom::new(&rom_path));
    let tile_ram = Rc::new(Ram::new(0x8000, 0x1800));
    let background_map = Rc::new(Ram::new(0x9800, 0x800));
    let oam_ram = Rc::new(Ram::new(0xfe00, 0x9f));

    let working_ram = Rc::new(Ram::new(0xc000, 0x2000));
    let working_ram_clone = Rc::clone(&working_ram);
    let echo_ram = Rc::new(EchoRam::new(working_ram_clone, 0xe000, 0x1e00));
    let high_ram = Rc::new(Ram::new(0xff80, 0x7f));

    let serial_debug_port = Rc::new(SerialDebugPort::new());
    let serial_debug_port_clone = Rc::clone(&serial_debug_port);

    let timer = Rc::new(Timer::new());
    let timer_clone = Rc::clone(&timer);

    let interrupt_controller = Rc::new(InterruptController::new());
    let interrupt_controller_clone = Rc::clone(&interrupt_controller);

    let joypad = Rc::new(Joypad::new());
    let joypad_clone = Rc::clone(&joypad);

    let dma_controller = Rc::new(DmaController::new());
    let dma_controller_clone = Rc::clone(&dma_controller);

    let mmu = Rc::new(Mmu::new(vec![
        // TODO(PT): We should come up with a way to remove the boot ROM
        // after it exits, because it'll slow down every memory access afterwards
        bootrom,
        game_rom,
        tile_ram,
        background_map,
        ppu,
        high_ram,
        interrupt_controller,
        serial_debug_port,
        joypad,
        timer,
        working_ram,
        echo_ram,
        dma_controller,
        oam_ram,
    ]));

    let mut cpu = CpuState::new(Rc::clone(&mmu));
    //cpu.enable_debug();

    let gameboy = GameBoy::new(
        Rc::clone(&mmu),
        cpu,
        ppu_clone,
        interrupt_controller_clone,
        serial_debug_port_clone,
        timer_clone,
        joypad_clone,
        dma_controller_clone,
    );
    gameboy.mock_bootrom();

    // Ref: https://users.rust-lang.org/t/winit-0-20-the-state-of-window/29485/28
    // Ref: https://github.com/rust-windowing/winit/blob/master/examples/window_run_return.rs
    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Poll;

        match event {
            Event::MainEventsCleared => {
                for i in 0..256 {
                    gameboy.step();
                }
                            }
                        }
                    }
                }
                _ => (),
            },
            _ => {}
        }
    });
}

fn main_debug() {
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
