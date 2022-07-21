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
use libgui::window::AwmWindow;

use axle_rt::{
    amc_message_await, amc_message_send, amc_register_service,
    core_commands::{
        AmcExecBuffer, AmcQueryServiceRequest, AmcSupervisedProcessEventMsg, AMC_CORE_SERVICE_NAME,
    },
    printf, println, AmcMessage,
};
use axle_rt::{core_commands::SupervisedProcessEvent, ExpectsEventField};

use agx_definitions::{Point, Rect, Size};

use file_manager_messages::{LaunchProgram, FILE_SERVER_SERVICE_NAME};

mod ide_messages;
mod output_view;
mod source_code_view;
mod status_view;
mod text_input_view;
mod text_view;
use ide_messages::IDE_SERVICE_NAME;

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
    pub output_view: Rc<OutputView>,
    _message_handler: Rc<MessageHandler>,
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
                    superview_size.width,
                    ((usable_height as f64) * 0.825) as isize,
                ),
            )
        };
        let output_view_sizer = move |superview_size| {
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
        status_view.set_status("Idle");

        let source_code_view: Rc<SourceCodeView> =
            SourceCodeView::new(&message_handler, move |_v, superview_size| {
                source_code_view_sizer(superview_size)
            });
        Rc::clone(&window).add_component(Rc::clone(&source_code_view) as Rc<dyn UIElement>);

        let output_view: Rc<OutputView> =
            OutputView::new(move |_v, superview_size| output_view_sizer(superview_size));
        Rc::clone(&window).add_component(Rc::clone(&output_view) as Rc<dyn UIElement>);

        let out = Rc::new(Self {
            _window: window,
            status_view,
            source_code_view,
            output_view,
            _message_handler: Rc::clone(&message_handler),
        });

        message_handler.set_ide(&out);

        out
    }

    fn handle_message(&self, message: Message) {
        println!("Main controller got message {message:?}");
        match message {
            Message::SendCompileRequest => {
                self.launch_linker_if_necessary();
                let source_code = self.source_code_view.get_text();
                AssembleSource::send(&source_code);
                let elf_msg: AmcMessage<AssembledElf> =
                    amc_message_await(Some(LINKER_SERVICE_NAME));
                self.status_view.set_status("Compilation succeeded");

                let elf_data = unsafe {
                    let elf_data_slice = core::ptr::slice_from_raw_parts(
                        (&elf_msg.body().data) as *const u8,
                        elf_msg.body().data_len,
                    );
                    let elf_data: &mut [u8] = &mut *(elf_data_slice as *mut [u8]);
                    elf_data.to_vec()
                };
                println!("Got ELF data from linker of len {:?}", elf_data.len());
                amc_message_send(
                    "com.axle.core",
                    AmcExecBuffer::from("com.axle.runtime_generated", &elf_data, true),
                );
            }
        }
    }

    fn launch_linker_if_necessary(&self) {
        let exists_msg = AmcQueryServiceRequest::send(LINKER_SERVICE_NAME);
        if exists_msg.service_exists {
            printf!("Will not launch linker because it's already active\n");
            return;
        }
        let path = "/usr/applications/linker";
        amc_message_send(FILE_SERVER_SERVICE_NAME, LaunchProgram::new(path));
    }

    fn handle_supervised_process_event(&self, msg: &AmcSupervisedProcessEventMsg) {
        //println!("Got supervisor event {:?}", msg);
        match msg.supervised_process_event {
            SupervisedProcessEvent::ProcessStart(entry_point) => {
                self.handle_process_start(entry_point)
            }
            SupervisedProcessEvent::ProcessExit(status_code) => {
                self.handle_process_exit(status_code)
            }
            axle_rt::core_commands::SupervisedProcessEvent::ProcessWrite(len, buf) => {
                self.handle_process_write(&buf[..(len as usize)]);
            }
        }
    }

    fn handle_process_start(&self, entry_point: u64) {
        /*
        self.output_view
            .write(&format!("Process started, entry point: {entry_point:x}\n"));
        */
        self.status_view.set_status("Running...");
    }

    fn handle_process_write(&self, text: &[u8]) {
        let text_as_str = String::from_utf8_lossy(text);
        self.output_view.write(&format!("{text_as_str}\n"));
    }

    fn handle_process_exit(&self, status_code: u64) {
        self.output_view
            .write(&format!("Process exited with status code: {status_code}\n"));
        self.status_view.set_status("Program exited");
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

    let window_size = Size::new(680, 800);
    let window = Rc::new(AwmWindow::new("IDE", window_size));
    let ide_view = Rc::new(RefCell::new(IdeMainView::new(
        Rc::clone(&window),
        window_size,
    )));

    window.add_message_handler(move |_window, msg_unparsed: AmcMessage<[u8]>| {
        match msg_unparsed.source() {
            AMC_CORE_SERVICE_NAME => {
                // Is it an event from our supervised process?
                let raw_body = msg_unparsed.body();
                let event = u32::from_ne_bytes(
                    // We must slice the array to the exact size of a u32 for the conversion to succeed
                    raw_body[..core::mem::size_of::<u32>()]
                        .try_into()
                        .expect("Failed to get 4-length array from message body"),
                );

                unsafe {
                    // TODO(PT): Process started event
                    match event {
                        AmcSupervisedProcessEventMsg::EXPECTED_EVENT => {
                            Rc::clone(&ide_view.borrow())
                                .handle_supervised_process_event(body_as_type_unchecked(raw_body));
                        }
                        _ => (),
                    }
                }
            }
            _ => println!("Dropping unhandled message from {}", msg_unparsed.source()),
        }
    });

    window.enter_event_loop();
    0
}
