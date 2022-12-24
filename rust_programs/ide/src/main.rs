#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(default_alloc_error_handler)]
#![feature(format_args_nl)]
#![feature(core_intrinsics)]

extern crate alloc;
extern crate libc;

use alloc::{format, rc::Rc, string::String};
use core::cell::RefCell;
use linker_messages::{AssembleSource, AssembledElf, LINKER_SERVICE_NAME};
use output_view::OutputView;
use source_code_view::SourceCodeView;
use status_view::StatusView;

use libgui::ui_elements::UIElement;
use libgui::AwmWindow;

use axle_rt::{
    amc_message_await__u32_event, amc_message_send, amc_register_service,
    core_commands::{
        AmcExecBuffer, AmcQueryServiceRequest, AmcSupervisedProcessEventMsg, AMC_CORE_SERVICE_NAME,
    },
    println, AmcMessage,
};
use axle_rt::{core_commands::SupervisedProcessEvent, ExpectsEventField};

use agx_definitions::{Color, Point, Rect, Size};

use file_manager_messages::{ReadFile, ReadFileResponse, FILE_SERVER_SERVICE_NAME};

mod ide_messages;
mod output_view;
mod source_code_view;
mod status_view;
use ide_messages::IDE_SERVICE_NAME;

enum SupervisedProgram {
    SpawnedProgram,
    SpawnedLinker,
}

#[derive(Debug)]
pub enum Message {
    SendCompileRequest,
}

pub struct MessageHandler {
    ide: RefCell<Option<Rc<IdeMainView>>>,
}

impl MessageHandler {
    fn new() -> Self {
        Self {
            ide: RefCell::new(None),
        }
    }

    fn set_ide(&self, ide: &Rc<IdeMainView>) {
        *self.ide.borrow_mut() = Some(Rc::clone(ide))
    }

    pub fn publish(&self, message: Message) {
        self.ide.borrow().as_ref().unwrap().handle_message(message);
    }
}

struct IdeMainView {
    _window: Rc<AwmWindow>,
    status_view: Rc<StatusView>,
    source_code_view: Rc<SourceCodeView>,
    linker_output_view: Rc<OutputView>,
    pub program_output_view: Rc<OutputView>,
    _message_handler: Rc<MessageHandler>,
    spawned_process_pid: RefCell<Option<usize>>,
    spawned_linker_pid: RefCell<Option<usize>>,
    awaiting_process_spawn: RefCell<bool>,
    awaiting_linker_spawn: RefCell<bool>,
}

impl IdeMainView {
    fn new(window: Rc<AwmWindow>, _window_size: Size) -> Rc<Self> {
        let status_view_sizer = |superview_size: Size| {
            Rect::from_parts(Point::zero(), Size::new(superview_size.width, 100))
        };
        let source_code_view_sizer = move |superview_size| {
            let status_view_frame = status_view_sizer(superview_size);
            let usable_height = superview_size.height - status_view_frame.height();
            Rect::from_parts(
                Point::new(0, status_view_frame.max_y()),
                Size::new(
                    ((superview_size.width as f64) * 0.4) as _,
                    ((usable_height as f64) * 0.825) as isize,
                    /*
                    300 + 22,
                    600 + 22,
                    */
                ),
            )
        };
        let linker_output_sizer = move |superview_size| {
            let source_code_view_frame = source_code_view_sizer(superview_size);
            Rect::from_parts(
                Point::new(
                    source_code_view_frame.width(),
                    source_code_view_frame.min_y(),
                ),
                Size::new(
                    superview_size.width - source_code_view_frame.width(),
                    source_code_view_frame.height(),
                ),
            )
        };
        let program_output_view_sizer = move |superview_size| {
            // TODO(PT): This pattern has exponential growth on calls to status_view_sizer()
            let source_code_view_frame = source_code_view_sizer(superview_size);
            let usable_height = superview_size.height - source_code_view_frame.max_y();
            Rect::from_parts(
                Point::new(0, source_code_view_frame.max_y()),
                Size::new(superview_size.width, usable_height),
            )
        };

        let message_handler = Rc::new(MessageHandler::new());

        let status_view: Rc<StatusView> =
            StatusView::new(&message_handler, move |_v, superview_size| {
                status_view_sizer(superview_size)
            });
        Rc::clone(&window).add_component(Rc::clone(&status_view) as Rc<dyn UIElement>);
        //status_view.set_status("Idle");

        let source_code_view: Rc<SourceCodeView> =
            SourceCodeView::new(&message_handler, move |_v, superview_size| {
                source_code_view_sizer(superview_size)
            });
        Rc::clone(&window).add_component(Rc::clone(&source_code_view) as Rc<dyn UIElement>);
        source_code_view.view.draw_cursor();
        /*
        println!("Drawing first rect...");
        source_code_view.view.view.view.layer.fill_rect(
            Rect::from_parts(Point::zero(), Size::new(300, 300)),
            Color::new(100, 120, 200),
            agx_definitions::StrokeThickness::Filled,
        );
        println!("Drawing second rect...");
        source_code_view.view.view.view.layer.fill_rect(
            Rect::from_parts(Point::new(0, 300), Size::new(300, 300)),
            Color::red(),
            agx_definitions::StrokeThickness::Filled,
        );
        */
        /*
        println!("Drawing text...");
        for ch in "Test string\nTest newline\nTest line 3\nTest line 4\nTest line5".chars() {
            source_code_view
                .view
                .draw_char_and_update_cursor(ch, Color::black());
        }
        println!("Done drawing rects...");
        */

        let linker_output_view: Rc<OutputView> =
            OutputView::new(Size::new(10, 12), move |_v, superview_size| {
                linker_output_sizer(superview_size)
            });
        linker_output_view.set_title("Compiler");
        Rc::clone(&window).add_component(Rc::clone(&linker_output_view) as Rc<dyn UIElement>);

        let program_output_view: Rc<OutputView> =
            OutputView::new(Size::new(10, 12), move |_v, superview_size| {
                program_output_view_sizer(superview_size)
            });
        program_output_view.set_title("Output");
        Rc::clone(&window).add_component(Rc::clone(&program_output_view) as Rc<dyn UIElement>);

        let out = Rc::new(Self {
            _window: window,
            status_view,
            source_code_view,
            linker_output_view,
            program_output_view,
            _message_handler: Rc::clone(&message_handler),
            spawned_process_pid: RefCell::new(None),
            spawned_linker_pid: RefCell::new(None),
            awaiting_process_spawn: RefCell::new(false),
            awaiting_linker_spawn: RefCell::new(false),
        });

        message_handler.set_ide(&out);

        out
    }

    fn handle_message(&self, message: Message) {
        println!("Main controller got message {message:?}");
        match message {
            Message::SendCompileRequest => {
                /*
                                let source_code_view = &self.source_code_view;
                                source_code_view.view.view.clear();
                                let source = ".global _start
                .section .text
                _start:
                mov $0xc, %rax
                mov $0x1, %rbx
                mov $str, %rcx
                mov $len, %rdx
                int $0x80
                mov $0xd, %rax
                mov $0x0, %rbx
                int $0x80
                .section .rodata
                str:
                .ascii \"Hello, world!\"
                .equ len, . - str
                ";
                                for ch in source.chars() {
                                    source_code_view
                                        .view
                                        .view
                                        .draw_char_and_update_cursor(ch, Color::black());
                                }
                */

                // New compilation/run session, clear output views
                self.linker_output_view.clear();
                self.program_output_view.clear();

                self.launch_linker();
                let source_code = self.source_code_view.get_text();
                *self.awaiting_process_spawn.borrow_mut() = true;
                AssembleSource::send(&source_code);
            }
        }
    }

    fn launch_linker(&self) {
        let exists_msg = AmcQueryServiceRequest::send(LINKER_SERVICE_NAME);
        // We need there to only be a single linker as we (eventually) need to supervise it
        assert!(
            !exists_msg.service_exists,
            "Linker instance was already running\n"
        );
        let path = "/usr/applications/linker";
        // Don't use LaunchProgram as we want to use the special interface that allows us to supervise the child
        //amc_message_send(FILE_SERVER_SERVICE_NAME, LaunchProgram::new(path));
        amc_message_send(FILE_SERVER_SERVICE_NAME, ReadFile::new(path));
        let linker_program_msg: AmcMessage<ReadFileResponse> =
            amc_message_await__u32_event(FILE_SERVER_SERVICE_NAME);
        let linker_program_slice = core::ptr::slice_from_raw_parts(
            (&linker_program_msg.body().data) as *const u8,
            linker_program_msg.body().len,
        );
        let linker_program_bytes: &mut [u8] = unsafe { &mut *(linker_program_slice as *mut [u8]) };
        println!("Spawning linker...");
        amc_message_send(
            AMC_CORE_SERVICE_NAME,
            AmcExecBuffer::from("linker", &linker_program_bytes.to_vec(), true),
        );
        *self.awaiting_linker_spawn.borrow_mut() = true;
        /*
        println!("Awaiting process create message");
        let process_create_msg: AmcMessage<AmcSupervisedProcessEventMsg> =
            amc_message_await__u32_event(AMC_CORE_SERVICE_NAME);
        if let SupervisedProcessEvent::ProcessCreate(pid) =
            process_create_msg.body().supervised_process_event
        {
            *self.spawned_linker_pid.borrow_mut() = Some(pid as _)
        } else {
            panic!(
                "Expected ProcessCreate event, got {:?}",
                process_create_msg.body().supervised_process_event
            );
        }
        */
    }

    fn handle_supervised_process_event(&self, msg: &AmcSupervisedProcessEventMsg) {
        //println!("Got supervisor event {:?}", msg);
        match msg.supervised_process_event {
            SupervisedProcessEvent::ProcessCreate(pid) => {
                ////////panic!("Should have been handled above!")
                // The ProcessCreate event for the linker is handled inline, so this should
                // be the spawned program
                //*self.spawned_process_pid.borrow_mut() = Some(pid as _)
                let awaiting_process_spawn = *self.awaiting_process_spawn.borrow();
                let awaiting_linker_spawn = *self.awaiting_linker_spawn.borrow();
                println!("Handling process create event for PID {pid} (awaiting proc? {awaiting_process_spawn:?} awaiting linker? {awaiting_linker_spawn:?})");
                // If we're waiting for both events, the linker certainly came first
                if awaiting_process_spawn && awaiting_linker_spawn {
                    println!(
                        "\tBoth linker and program are awaiting creation, will assign PID to linker"
                    );
                    *self.awaiting_linker_spawn.borrow_mut() = false;
                    *self.spawned_linker_pid.borrow_mut() = Some(pid as _);
                } else {
                    if awaiting_process_spawn {
                        println!("\tAssigning PID to process");
                        *self.awaiting_process_spawn.borrow_mut() = false;
                        *self.spawned_process_pid.borrow_mut() = Some(pid as _);
                    } else if awaiting_linker_spawn {
                        println!("\tAssigning PID to linker");
                        *self.awaiting_linker_spawn.borrow_mut() = false;
                        *self.spawned_linker_pid.borrow_mut() = Some(pid as _);
                    }
                }
            }
            SupervisedProcessEvent::ProcessStart(pid, entry_point) => {
                self.handle_process_start(pid, entry_point)
            }
            SupervisedProcessEvent::ProcessExit(pid, status_code) => {
                self.handle_process_exit(pid, status_code)
            }
            axle_rt::core_commands::SupervisedProcessEvent::ProcessWrite(pid, len, buf) => {
                self.handle_process_write(pid, &buf[..(len as usize)]);
            }
        }
    }

    fn program_with_pid(&self, pid: u64) -> SupervisedProgram {
        let spawned_process_pid = *self.spawned_process_pid.borrow();
        let spawned_linker_pid = *self.spawned_linker_pid.borrow();
        if let Some(spawned_process_pid) = spawned_process_pid {
            if spawned_process_pid == (pid as _) {
                return SupervisedProgram::SpawnedProgram;
            }
        }
        if let Some(spawned_linker_pid) = spawned_linker_pid {
            if spawned_linker_pid == (pid as _) {
                return SupervisedProgram::SpawnedLinker;
            }
        }
        panic!("Unhandled PID {pid}");
        //SupervisedProgram::SpawnedProgram
    }

    fn handle_process_start(&self, pid: u64, _entry_point: u64) {
        let process = self.program_with_pid(pid);
        match process {
            SupervisedProgram::SpawnedProgram => self.status_view.set_status("Running..."),
            SupervisedProgram::SpawnedLinker => self.linker_output_view.write("Linker startup\n"),
        }
    }

    fn handle_process_write(&self, pid: u64, text: &[u8]) {
        let text_as_str = String::from_utf8_lossy(text);
        let process = self.program_with_pid(pid);
        match process {
            SupervisedProgram::SpawnedProgram => {
                self.program_output_view.write(&format!("{text_as_str}"))
            }
            SupervisedProgram::SpawnedLinker => {
                self.linker_output_view.write(&format!("{text_as_str}"))
            }
        }
    }

    fn handle_process_exit(&self, pid: u64, status_code: u64) {
        let process = self.program_with_pid(pid);
        match process {
            SupervisedProgram::SpawnedProgram => {
                self.status_view.set_status("Program exited");
                self.program_output_view.write(&format!(
                    "\nProcess exited with status code: {status_code}\n"
                ));
            }
            SupervisedProgram::SpawnedLinker => {
                self.linker_output_view.write(&format!(
                    "\nLinker exited with status code: {status_code}\n"
                ));
            }
        }
    }
}

/// # Safety
/// You must have previously checked that the payload is of the provided type
pub unsafe fn body_as_type_unchecked<T>(body: &[u8]) -> &T {
    &*(body.as_ptr() as *const T)
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service(IDE_SERVICE_NAME);

    let window_size = Size::new(1400, 900);
    let window = Rc::new(AwmWindow::new("IDE", window_size));
    let ide_view = Rc::new(RefCell::new(IdeMainView::new(
        Rc::clone(&window),
        window_size,
    )));

    window.add_message_handler(move |_window, msg_unparsed: AmcMessage<[u8]>| {
        let raw_body = msg_unparsed.body();
        let event = u32::from_ne_bytes(
            // We must slice the array to the exact size of a u32 for the conversion to succeed
            raw_body[..core::mem::size_of::<u32>()]
                .try_into()
                .expect("Failed to get 4-length array from message body"),
        );

        match msg_unparsed.source() {
            AMC_CORE_SERVICE_NAME => {
                // Is it an event from our supervised process?
                unsafe {
                    if let AmcSupervisedProcessEventMsg::EXPECTED_EVENT = event {
                        Rc::clone(&ide_view.borrow())
                            .handle_supervised_process_event(body_as_type_unchecked(raw_body));
                    } else {
                        println!("Dropping unhandled message from core");
                    }
                }
            }
            LINKER_SERVICE_NAME => {
                let elf_msg: &AssembledElf = unsafe { body_as_type_unchecked(raw_body) };
                assert_eq!(event, AssembledElf::EXPECTED_EVENT);
                ide_view
                    .borrow()
                    .status_view
                    .set_status("Compilation succeeded");

                let elf_data = unsafe {
                    let elf_data_slice = core::ptr::slice_from_raw_parts(
                        (&elf_msg.data) as *const u8,
                        elf_msg.data_len,
                    );
                    let elf_data: &mut [u8] = &mut *(elf_data_slice as *mut [u8]);
                    elf_data.to_vec()
                };
                println!("Got ELF data from linker of len {:?}", elf_data.len());
                ide_view.borrow().linker_output_view.write(&format!(
                    "Got ELF data from linker of len {:?}",
                    elf_data.len()
                ));
                amc_message_send(
                    AMC_CORE_SERVICE_NAME,
                    AmcExecBuffer::from("com.axle.runtime_generated", &elf_data, true),
                );
            }
            _ => println!("Dropping unhandled message from {}", msg_unparsed.source()),
        }
        _window.draw();
    });

    println!("Entering event loop...");
    window.enter_event_loop();
    0
}
