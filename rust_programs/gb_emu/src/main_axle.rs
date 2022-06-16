use alloc::vec;
use alloc::{boxed::Box, rc::Rc, vec::Vec};
use core::cell::RefCell;

// Ref: https://stackoverflow.com/questions/51516773/how-to-use-vec
#[macro_use]
#[cfg(feature = "use_std")]
use {
    pixels::{Error, Pixels, SurfaceTexture},
    std::{
        cell::RefCell,
        io::Write,
        mem,
        num::ParseIntError,
        rc::{Rc, Weak},
        time::Duration,
    },
    winit::{
        dpi::{LogicalPosition, LogicalSize},
        event::{ElementState, Event, VirtualKeyCode, WindowEvent},
        event_loop::{ControlFlow, EventLoop},
        window::{Fullscreen, WindowBuilder},
    },
};

use agx_definitions::Size;
use awm_messages::{
    AwmCreateWindow, AwmCreateWindowResponse, AwmWindowRedrawReady, AwmWindowUpdateTitle,
};
use axle_rt::{
    amc_has_message, amc_message_await, amc_message_await_untyped, amc_message_send,
    amc_register_service, AmcMessage, ContainsEventField, ExpectsEventField,
};
use libgui::{
    window::AwmWindow,
    window_events::{KeyDown, KeyUp},
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

struct Window {
    window_framebuf: RefCell<Box<[u8]>>,
    //window_framebuf: Box<[u8]>,
    screen_resolution: Size,
    game_framebuf: Vec<u8>,
}

use axle_rt::println;

impl Window {
    fn new(title: &str, size: Size) -> Self {
        // Start off by getting a window from awm
        let awm_service_name = "com.axle.awm";
        amc_message_send(awm_service_name, AwmCreateWindow::new(&size));
        // awm should send back info about the window that was created
        let window_info: AmcMessage<AwmCreateWindowResponse> =
            amc_message_await(Some(awm_service_name));

        let bpp = window_info.body().bytes_per_pixel as isize;
        let screen_resolution = Size::from(&window_info.body().screen_resolution);

        println!("Window::new framebuf slice");
        let framebuffer_slice = core::ptr::slice_from_raw_parts_mut(
            window_info.body().framebuffer_ptr,
            (bpp * screen_resolution.width * screen_resolution.height)
                .try_into()
                .unwrap(),
        );
        let framebuffer: &mut [u8] = unsafe { &mut *(framebuffer_slice as *mut [u8]) };
        println!("Window::new finish framebuf slice");

        amc_message_send(awm_service_name, AwmWindowUpdateTitle::new(title));

        unsafe {
            Self {
                window_framebuf: RefCell::new(Box::from_raw(framebuffer)),
                screen_resolution,
                //window_framebuf: Box::from_raw(framebuffer),
                game_framebuf: vec![0; (size.width * size.height * bpp) as usize],
            }
        }
    }
}

impl GraphicsLayer for Window {
    fn get_pixel_buffer(&mut self) -> &mut [u8] {
        &mut self.game_framebuf
    }

    fn render_to_screen(&self) {
        //println!("Rendering to screen...");
        // Blit the game framebuf to the window framebuf
        let mut window_framebuf = self.window_framebuf.borrow_mut();
        let game_framebuf = &self.game_framebuf;
        let gameboy_width = 160;
        let gameboy_row_size = gameboy_width * 4;
        let window_width = gameboy_width * 2;
        let window_row_size = window_width * 4;
        for y in (0..288).step_by(2) {
            for y_dup in 0..2 {
                let window_row_off = (y + y_dup) * (self.screen_resolution.width as usize) * 4;
                let game_row_off = (y / 2 * gameboy_row_size);

                /*
                window_framebuf[window_row_off..window_row_off + gameboy_row_size]
                    .copy_from_slice(&game_framebuf[game_row_off..game_row_off + gameboy_row_size]);
                */

                for x in 0..160 {
                    let window_px_off = window_row_off + (x * 4 * 2);
                    let game_px_off = game_row_off + (x * 4);
                    window_framebuf[window_px_off + 0] = game_framebuf[game_px_off + 0];
                    window_framebuf[window_px_off + 1] = game_framebuf[game_px_off + 1];
                    window_framebuf[window_px_off + 2] = game_framebuf[game_px_off + 2];
                    //window_framebuf[window_px_off + 3] = game_framebuf[game_px_off + 3];
                    window_framebuf[window_px_off + 4] = game_framebuf[game_px_off + 0];
                    window_framebuf[window_px_off + 5] = game_framebuf[game_px_off + 1];
                    window_framebuf[window_px_off + 6] = game_framebuf[game_px_off + 2];
                }
            }
        }
        //println!("Finished to screen!");
        amc_message_send("com.axle.awm", AwmWindowRedrawReady::new());
    }
}

pub fn main() {
    amc_register_service("com.axle.gameboy");

    let window = Window::new("GameBoy", Size::new(160 * 2, 144 * 2));

    let bootrom = Rc::new(BootRom::new("/usr/roms/bootrom.gb"));
    let ppu = Rc::new(Ppu::new(Box::new(window)));
    let ppu_clone = Rc::clone(&ppu);
    let game_rom = Rc::new(GameRom::new("/usr/roms/legend_of_zelda.gb"));
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

    loop {
        //while amc_has_message(Some("com.axle.awm")) {
        while amc_has_message(None) {
            //
            let msg_unparsed: AmcMessage<[u8]> =
                unsafe { amc_message_await_untyped(Some("com.axle.awm")).unwrap() };

            // Parse the first bytes of the message as a u32 event field
            let raw_body = msg_unparsed.body();
            if raw_body.len() < 4 {
                println!("BODY Less than 4 bytes!");
                continue;
            }
            let event = u32::from_ne_bytes(
                // We must slice the array to the exact size of a u32 for the conversion to succeed
                raw_body[..core::mem::size_of::<u32>()]
                    .try_into()
                    .expect("Failed to get 4-length array from message body"),
            );

            // Each inner call to body_as_type_unchecked is unsafe because we must be
            // sure we're casting to the right type.
            // Since we verify the type on the LHS, each usage is safe.
            //
            // Wrap the whole thing in an unsafe block to reduce
            // boilerplate in each match arm.
            unsafe {
                match event {
                    // Keyboard events
                    KeyDown::EXPECTED_EVENT => {
                        handle_key_down(&gameboy, body_as_type_unchecked(raw_body))
                    }
                    KeyUp::EXPECTED_EVENT => {
                        handle_key_up(&gameboy, body_as_type_unchecked(raw_body))
                    }
                    // Ignore unknown events
                    _ => (),
                }
            }
        }
        for i in 0..1 {
            gameboy.step();
        }
    }
    //window.enter_event_loop();
}

pub trait AwmWindowEvent: ExpectsEventField + ContainsEventField {}

impl AwmWindowEvent for KeyDown {}
impl AwmWindowEvent for KeyUp {}

const KEY_IDENT_UP_ARROW: u32 = 0x999;
const KEY_IDENT_DOWN_ARROW: u32 = 0x998;
const KEY_IDENT_LEFT_ARROW: u32 = 0x997;
const KEY_IDENT_RIGHT_ARROW: u32 = 0x996;
const KEY_IDENT_Z: u32 = 122;
const KEY_IDENT_X: u32 = 120;
const KEY_IDENT_C: u32 = 99;
const KEY_IDENT_V: u32 = 118;

unsafe fn body_as_type_unchecked<T: AwmWindowEvent>(body: &[u8]) -> &T {
    &*(body.as_ptr() as *const T)
}

fn handle_key_event(gameboy: &dyn GameBoyHardwareProvider, keycode: u32, is_pressed: bool) {
    println!("Keycode: {keycode}");
    let joypad_button = match keycode {
        KEY_IDENT_LEFT_ARROW => Some(Button::Left),
        KEY_IDENT_RIGHT_ARROW => Some(Button::Right),
        KEY_IDENT_UP_ARROW => Some(Button::Up),
        KEY_IDENT_DOWN_ARROW => Some(Button::Down),
        KEY_IDENT_Z => Some(Button::A),
        KEY_IDENT_X => Some(Button::B),
        KEY_IDENT_C => Some(Button::Start),
        KEY_IDENT_V => Some(Button::Select),
        _ => None,
    };
    if let Some(joypad_button) = joypad_button {
        println!("Got key {joypad_button:?} {joypad_button}");
        if is_pressed {
            gameboy.get_joypad().set_button_pressed(joypad_button);
        } else {
            gameboy.get_joypad().set_button_released(joypad_button);
        }
    }
}

fn handle_key_down(gameboy: &dyn GameBoyHardwareProvider, key_down_event: &KeyDown) {
    //println!("KeyDown {}", key_down_event.key);
    handle_key_event(gameboy, key_down_event.key, true);
}

fn handle_key_up(gameboy: &dyn GameBoyHardwareProvider, key_up_event: &KeyUp) {
    //println!("KeyUp {}", key_up_event.key);
    handle_key_event(gameboy, key_up_event.key, false);
}
