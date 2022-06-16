use std::{
    cell::RefCell,
    io::Write,
    mem,
    num::ParseIntError,
    rc::{Rc, Weak},
    time::Duration,
};

use pixels::{Error, Pixels, SurfaceTexture};
use winit::event_loop::{ControlFlow, EventLoop};
use winit::window::WindowBuilder;
use winit::{
    dpi::{LogicalPosition, LogicalSize},
    event::WindowEvent,
};
use winit::{
    event::{ElementState, Event, VirtualKeyCode},
    window::Fullscreen,
};

use crate::cpu::CpuState;
use crate::gameboy::{GameBoy, GameBoyHardwareProvider};
use crate::interrupts::InterruptController;
use crate::joypad::{Button, Joypad};
use crate::mmu::{Addressable, BootRom, DmaController, EchoRam, GameRom, Mmu, Ram};
use crate::ppu::{GraphicsLayer, Ppu, PpuMode};
use crate::serial::SerialDebugPort;
use crate::timer::Timer;

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

struct DummySoundController {}

impl DummySoundController {
    fn new() -> Self {
        Self {}
    }
    fn is_sound_register(addr: u16) -> bool {
        (addr >= 0xff10 && addr <= 0xff26) || (addr >= 0xff30 && addr <= 0xff3f)
    }
}

impl Addressable for DummySoundController {
    fn contains(&self, addr: u16) -> bool {
        DummySoundController::is_sound_register(addr)
    }

    fn read(&self, addr: u16) -> u8 {
        // Uninitialised reads return 0xff
        0xff
    }

    fn write(&self, addr: u16, val: u8) {
        // Ignore writes
    }
}

pub fn main() {
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
            .with_inner_size(scaled_size)
            .with_min_inner_size(scaled_size)
            .with_visible(true)
            .with_resizable(false)
            //.with_fullscreen(Some(Fullscreen::Borderless(None)))
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
    //let ppu = Rc::new(Ppu::new(Box::new(pixels), Box::new(vram_debug_pixels)));
    let ppu = Rc::new(Ppu::new(Box::new(pixels)));
    let ppu_clone = Rc::clone(&ppu);
    let rom_path = &std::env::args().collect::<Vec<String>>()[1];
    let game_rom = Rc::new(GameRom::new(&rom_path));
    let tile_ram = Rc::new(Ram::new(0x8000, 0x1800));
    let background_map = Rc::new(Ram::new(0x9800, 0x800));
    let oam_ram = Rc::new(Ram::new(0xfe00, 0xa0));

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
        // Hook up a dummy sound controller so writes to the sound registers succeed
        Rc::new(DummySoundController::new()),
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
    //gameboy.mock_bootrom();

    // Ref: https://users.rust-lang.org/t/winit-0-20-the-state-of-window/29485/28
    // Ref: https://github.com/rust-windowing/winit/blob/master/examples/window_run_return.rs
    let mut waiting_for_state = PpuMode::VBlank;
    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Poll;

        match event {
            Event::MainEventsCleared => {
                let ppu = gameboy.get_ppu();
                loop {
                    gameboy.step();
                    if *ppu.current_mode.borrow() == waiting_for_state {
                        /*
                        println!(
                            "Found PPU in desired state {waiting_for_state:?}, returning event loop after {i} steps"
                        );
                        */
                        waiting_for_state = match waiting_for_state {
                            PpuMode::VBlank => PpuMode::OamSearch,
                            _ => PpuMode::VBlank,
                        };
                        break;
                    }
                }
                //}
            }
            Event::WindowEvent { window_id, event } => match event {
                WindowEvent::KeyboardInput {
                    device_id,
                    input,
                    is_synthetic,
                } => {
                    if !is_synthetic {
                        if let Some(keycode) = input.virtual_keycode {
                            if keycode == VirtualKeyCode::B {
                                let cpu = gameboy.get_cpu();
                                println!("Entering debug mode...");
                                cpu.borrow_mut().enable_debug();
                            }
                            let joypad_button = match keycode {
                                VirtualKeyCode::Left => Some(Button::Left),
                                VirtualKeyCode::Right => Some(Button::Right),
                                VirtualKeyCode::Up => Some(Button::Up),
                                VirtualKeyCode::Down => Some(Button::Down),
                                VirtualKeyCode::Z => Some(Button::A),
                                VirtualKeyCode::X => Some(Button::B),
                                VirtualKeyCode::Return => Some(Button::Start),
                                VirtualKeyCode::Back => Some(Button::Select),
                                _ => None,
                            };
                            if let Some(joypad_button) = joypad_button {
                                if input.state == ElementState::Pressed {
                                    gameboy.get_joypad().set_button_pressed(joypad_button);
                                } else if input.state == ElementState::Released {
                                    gameboy.get_joypad().set_button_released(joypad_button);
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
    let mut pixels2 = {
        let window_size = window.inner_size();
        let surface_texture = SurfaceTexture::new(window_size.width, window_size.height, &window);
        Pixels::new(
            WINDOW_WIDTH.try_into().unwrap(),
            WINDOW_HEIGHT.try_into().unwrap(),
            surface_texture,
        )
        .unwrap()
    };

    let bootrom = Rc::new(BootRom::new("/Users/philliptennen/Downloads/DMG_ROM.bin"));
    let ppu = Rc::new(Ppu::new(Box::new(pixels)));
    let ppu_clone = Rc::clone(&ppu);
    let rom_path = &std::env::args().collect::<Vec<String>>()[1];
    let game_rom = Rc::new(GameRom::new(&rom_path));
    let tile_ram = Rc::new(Ram::new(0x8000, 0x1800));
    let oam_ram = Rc::new(Ram::new(0xfe00, 0x00a0));
    let background_map = Rc::new(Ram::new(0x9800, 0x800));

    let working_ram = Rc::new(Ram::new(0xc000, 0x2000));
    let working_ram_clone = Rc::clone(&working_ram);
    let echo_ram = Rc::new(EchoRam::new(working_ram_clone, 0xe000, 0x1e00));
    let high_ram = Rc::new(Ram::new(0xff80, 0x7f));

    let serial_debug_port = Rc::new(SerialDebugPort::new());
    let serial_debug_port_clone = Rc::clone(&serial_debug_port);

    let interrupt_controller = Rc::new(InterruptController::new());
    let interrupt_controller_clone = Rc::clone(&interrupt_controller);

    let timer = Rc::new(Timer::new());
    let timer_clone = Rc::clone(&timer);

    let joypad = Rc::new(Joypad::new());
    let joypad_clone = Rc::clone(&joypad);

    let dma_controller = Rc::new(DmaController::new());
    let dma_controller_clone = Rc::clone(&dma_controller);

    let mmu = Rc::new(Mmu::new(vec![
        bootrom,
        interrupt_controller,
        serial_debug_port,
        joypad,
        timer,
        ppu,
        tile_ram,
        oam_ram,
        background_map,
        working_ram,
        high_ram,
        echo_ram,
        game_rom,
        dma_controller,
        // Hook up a dummy sound controller so writes to the sound registers succeed
        Rc::new(DummySoundController::new()),
    ]));

    let mut cpu = CpuState::new(Rc::clone(&mmu));
    cpu.enable_debug();

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

    let debugger = Debugger::new(gameboy);
    debugger.run();
}

#[derive(Copy, Clone, PartialEq)]
enum RunMode {
    NoRun,
    RunOneInstruction,
    RunToNextBreakpoint,
}

struct Debugger {
    gameboy: GameBoy,
    breakpoints: RefCell<Vec<Breakpoint>>,
    run_mode: RefCell<RunMode>,
}

impl Debugger {
    fn new(gameboy: GameBoy) -> Self {
        Self {
            gameboy,
            breakpoints: RefCell::new(Vec::new()),
            run_mode: RefCell::new(RunMode::NoRun),
        }
    }

    fn run(&self) {
        loop {
            self.debug_loop();
            let run_mode = *self.run_mode.borrow();
            match run_mode {
                RunMode::NoRun => {}
                RunMode::RunOneInstruction => {
                    self.gameboy.step();
                    self.set_run_mode(RunMode::NoRun);
                }
                RunMode::RunToNextBreakpoint => {
                    let mut i = 0;
                    loop {
                        let mut hit_breakpoint = false;
                        for breakpoint in &*self.breakpoints.borrow() {
                            if breakpoint.address == self.gameboy.get_cpu().borrow().get_pc() {
                                println!("Hit breakpoint at {:04x}", breakpoint.address);
                                hit_breakpoint = true;
                                break;
                            }
                            // TODO(PT): Remove the breakpoint?
                        }
                        if hit_breakpoint {
                            self.set_run_mode(RunMode::NoRun);
                            break;
                        }
                        self.gameboy.get_cpu().borrow_mut().step(&self.gameboy);
                    }
                }
            }
            if run_mode == RunMode::RunOneInstruction {
                self.gameboy.get_cpu().borrow().print_regs();
            }
        }
    }

    fn add_breakpoint(&self, breakpoint: Breakpoint) {
        println!("Setting breakpoint at {:04x}", breakpoint.address);
        self.breakpoints.borrow_mut().push(breakpoint)
    }

    fn set_run_mode(&self, run_mode: RunMode) {
        *self.run_mode.borrow_mut() = run_mode;
    }

    fn debug_loop(&self) {
        print!("Enter a command: ");
        std::io::stdout().flush();

        let mut stdin = std::io::stdin();
        let input = &mut String::new();
        stdin.read_line(input);

        let mut toks = input.split(' ').fuse();
        //let mut toks = input.split_whitespace().fuse();
        let cmd = toks.next();

        let mut cpu = self.gameboy.cpu.borrow_mut();
        match cmd {
            Some("b") => {
                let mut breakpoint_address_str = toks.next().unwrap();
                breakpoint_address_str = breakpoint_address_str.trim_end_matches('\n');
                //println!("{breakpoint_address_str}");
                let breakpoint_address = u16::from_str_radix(breakpoint_address_str, 16);
                let breakpoint_address = match breakpoint_address {
                    Ok(v) => v,
                    Err(ParseIntError) => {
                        println!("Malformed address: {breakpoint_address_str}");
                        return;
                    }
                };
                self.add_breakpoint(Breakpoint::new(breakpoint_address, false));
                return;
            }
            Some("d\n") => {
                println!("Deleting all breakpoints...");
                self.breakpoints.borrow_mut().clear();
                return;
            }
            Some("s\n") => {
                self.set_run_mode(RunMode::RunOneInstruction);
                return;
            }
            Some("c\n") => {
                self.set_run_mode(RunMode::RunToNextBreakpoint);
                return;
            }
            Some("regs\n") | Some("r\n") => {
                cpu.print_regs();
                return;
            }
            Some("\n") => {
                return;
            }
            _ => {
                println!("Unknown command {}", cmd.unwrap());
                std::io::stdout().flush().unwrap();
            }
        }
    }
}

fn main2() {
    //main_gfx()
}
