amc (axle message center)
--------------------

amc is axle's IPC message-passing mechanism. 

Services can register names, "com.axle.amc"

Send a message to "com.axle.amc"

Receive a message from "com.axle.kb_driver"

Block for a message from com.axle.kb_driver, com.axle.mouse_driver, or com.axle.animation_server

Proc's know what message woke them up

Kernel driver stubs send a message to the higher-level driver, then high-level driver is woken and dispatches
Make sure that IRQs complete before waking up the higher-level driver

Services:

"com.axle.task_reaper" (Clean up completed tasks [that are not awaiting children etc])
"com.axle.idle" (Idle task)
"com.axle.awm" (Window manager)
"com.axle.kb_driver":
    There is a kernel-space driver that sends high-priority messages to this
    This is woken up *after* the IRQ and dispatches the keystroke to an appropriate task
"com.axle.mouse_driver"
"com.axle.animation_server" (Wake up awm when an animation needs a new frame rendered)
"com.axle.iosentinel" (Wake up other blocked tasks?)

Prototypes:
// Identify the current process as the provided service name
void amc_register_service(const char* service_name);

// Send a message to the process with the provided service name
void amc_send_message(const char* service_name, char* payload, int len);

// Block until a message has been received
// Returns the message contents
char* amc_await_message(void);

Flow of keystroke towards user window
-----------------------

Low-level keyboard driver:
amc_send_message("com.axle.kb_driver", KEYSTROKE, &ch, 1);

High-level keyboard driver: 
char keyboard_input = amc_await_message(KEYSTROKE);
amc_send_message("com.axle.awm", KEYSTROKE, &ch, 1);

awm:
char keyboard_input = amc_await_message(KEYSTROKE);
amc_send_message(first_responder, KEYSTROKE, &keyboard_input, 1);

Desktop program:
char keyboard_input = amc_await_message(KEYSTROKE);

Flow of printf() towards log console
-----------------------

User program:
printf("abc");
write(1, "abc", 4);
amc_send_message("com.axle.tty", STDOUT, "abc", 4);

com.axle.tty:
char* msg = amc_await_message(STDOUT);
amc_send_message("com.axle.awm", STDOUT, msg, 4);
amc_send_message("com.axle.serial_driver", STDOUT, msg, 4);

com.axle.awm:
char* msg = amc_await_message(STDOUT);
// TODO: A way to check if a service is active or check for delivery...
// If log_console isn't active, output direct-to-display desktop_overlay_show_keystroke
amc_send_message("com.axle.log_console", STDOUT, msg, 4);
// Or...
// amc_broadcast_message will send a message to everyone waiting on a message from this service, 
// instead of this service specifying a receiver
// But what if we broadcast a message before the program starts to wait for it?
amc_broadcast_message(STDOUT, msg, 4);

com.axle.log_console:
char* msg = amc_await_message(STDOUT);
draw_label(msg);

Flow of stdin/stdout towards ash
---------------------

ash:

char keyboard_input = amc_await_message(KEYSTROKE);
draw_label(keyboard_input);
// Does ash need to listen to globally broadcast messages from "com.axle.tty"?
char* printf_data = amc_await_message("com.axle.awm", STDOUT);
draw_label(printf_data);

####

Draft 2
-----------------------

enum amc_message_type {
    KEYSTROKE = 0,
    STDOUT = 1,
};

struct amc_message {
    const char* source;
    const char* dest; // May be null if the message is globally broadcast
    amc_message_type type;
    char data[64];
    int len;
}

void amc_message_construct(const char* destination_service, const char* data, int len);

// Will send the message to the provided destination service
void amc_message_send(const char* destination_service, amc_message_t* msg);
// Will send the message to any service awaiting a message from this service
void amv_message_broadcast(amc_message* msg);

// Block until a message has been received from the source service
void amc_message_await(const char* source_service, amc_message_t** out);

Flow of window stdout towards alc (axle log control)
-----------

User program:
printf("abc");
write(1, "abc", 4);
amc_send_message("com.axle.tty", STDOUT, "abc", 4);

com.axle.tty:
char* msg = amc_await_message(STDOUT);
amc_send_message("com.axle.awm", STDOUT, msg, 4);
amc_send_message("com.axle.serial_driver", STDOUT, msg, 4);

Abbrevs
----------

awm: axle window manager
amc: axle message center
ash: axle shell
alc: axle log control

Next
-----------

We want a log-viewer that accepts messages from tty and draws them
- Need gfx to be linked in every newlib app, and for the headers to be in sysroot
- Syscall or message to xserv to get a window buffer
- Syscall or message to sync that window buffer to xserv
