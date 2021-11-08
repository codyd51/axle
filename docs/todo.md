TODO
---------------

PCI enumeration lives in a "PCI manager" process
This process has an awm window that displays PCI devices, 
and launches the appropriate drivers (check)

Processes need a syscall to launch another process

Perhaps processes can register themselves with awm on _start
But this needs some way for processes to register their amc service 
name before starting. Perhaps an Info.plist?

If the above is done, then the PCI manager can also launch drivers via their AMC service name
Perhaps the Info.plist has enough info for the PCI manager to do discovery on available drivers

AMC services should also be able to "wait" until another AMC service is launched, then
in _start after registering the service a system-wide notification can be posted that this service is alive

Implement synchronous messaging: send, block until recv "ack"

Fix task starvation of same priority, runnable (check)

Come up with better way to pass args to programs on startup

Driver library - provides in/out functions, assert, what else?

awm library - helpers for constructing window, other communications? update title bar? (check)

The mouse can't pre-empt the RTL driver because they're the same priority, so it loses data
The RTL driver needs to be split into a GUI and message server...

Replace PMM mutex with spinlock

Don't map the null page

amc should be able to send smaller or larger messages (check)
    Network packets can be 300+ bytes,
    but I don't want to inflate amc message size for every message type

    Instead of providing a fixed-size amc_message_t,
    the amc_message_send API should take a buffer and a size.
    AMC prepends a header, copies the buffer, and saves it.
    Messages can thus be an arbitary size.

text box uses a "resizable bytes container" that does auto-expand/realloc when
its contents are too big. It stores the text contents there. It implements scrolling.

There is a "net" backend that constructs packets of the various protocols,
and there is a "libnet" frontend library that handles all the state transitions for
DNS and TCP.
    - ARP also has an asynchronous response
    - Discovering the router's MAC should probably also be kicked off / waited for by libnet
    - libnet will receive events from the net backend informing it about packets
    - libnet may have a higher-level mechanism like "wait for a DNS answer about this domain"
    - If the DNS answer is already in the cache, the call will return without blocking
    - The net backend and kernel will know how to block and unblock processes using libnet
    - The net backend may only unblock a process using libnet when the process has received
        exactly the event it was awaiting
        (Probably this will entail both a command and a command-specific structure indicating 
            eg the DNS answer being waited for)

Symbolicate ELF stack traces

Send a ping to every program periodically to ensure its liveliness
    - In a separate monitor that uses the same flow control as the awm queue monitor
    - Every 5 seconds sends a liveliness check to every amc service
    - If no response in 3 seconds, send awm a "hang notice"
    - If it begins responding, send awm a "hang resolved"

Timekeeping
    - Net timeouts/retransmissions
    - Liveliness pings

Implement an event loop API instead of apps needing to manage the event loop themselves (check)
    - Callbacks can be set up for:
        - amc messages
        - timers
    - Apps can subscribe callbacks for amc messages

Move ARP, etc into callback model

Display number of pending amc messages on each window so we can watch the queue drain
    - Perhaps this can be a window that queries amc debug info every 3 seconds and
       draws the stats
    - Roll into a task monitor?

awm de-duplicates redraw messages from the same window (redraws only once) (check)

DNS might need to wait for ARP
    - We might need a more generic "do ARP with this callback function pointer"
        - Where the function pointer might be, "unblock via an amc reply",
            - Or, "send the DNS query to the router"

Settings application (check)
    - Shows gradient color options for desktop background
        - Sends msg to awm
    - Predefined list, or show color sliders?
        - Good opportunity to build widgets
            - But should wait for proper UI toolkit event loop

UI toolkit event loop (check)
    - layout_subviews?

Terminal emulator

awm composites and does visible-rect-splitting

Parse truetype fonts and render them

Persistent storage

AWM_FOCUS_EXITED

TLS

Notepad
Window minimizing
Shrink left X edge on scrollbar
Fix scrollbar UI glitch
Scroll bar stitching

Objective-C implementation?

SMTP

Arrow keys (check)
Key-up events in libgui (check)
Velocity in breakout

Fix mouse position within gui_view

Process destruction
GUI destruction
Window closing
gui_scroll_view_t (check)
    Presents a scroll bar
    Remove gui_text_field's scroll bar?

Revamp gui_text_view and gui_text_input to subclass gui_view

gui_text_input uses a timer to control its flashing cursor

scroll view scrollbar (check)
scroll view layer stitching

awm uses libgui (with a custom event loop)

awm exposes a primary/secondary tint color
    Preferences reads this to set its initial slider values
    Values are randomised on boot (or eventually perisisted to a config file)
    Snake uses it for its tail

ray tracer
game of life
sand sim
amc delivery space is initiall small (1MB) and grows as large messages are sent
    reduces overall memory footprint

stress-test hash map, it seems broken with linked list + delete

check mem usage of an amc task with framebuf that spawns another (copied page tables?) (check)

free elf loader memory (check)
free sbrk memory (check)

amc delivery pool grows as necessary instead of fixed 32mb

Rework shared memory concept
    awm should be able to pool together framebufs
    awm doesn't need to 'check' whether a region's still valid
    Instead of having a pair on either end, they can have > 2 processes attach

Multi-stage compositing
    First stage: find dirty rects
        Mouse moved, window updated
    
    For mouse moved dirty rect:
        Split the rect into every affected layer
            (Could be two windows plus the background)
        Create new list of dirty rect + source layer

    Blit each dirty rect + source layer to the screen memory

Goals:
    Events that invalidate draw rects:
        Window updated
        Window moved (background rects updated)
        Z-order changed

    2 events:
        - Need to recalculate split regions
            Can do this background, then backmost window, next window
                Background:
                    Iterate all windows
                Backmost window
                    Iterate all windows above it
            O(n^2)
            Need to do minimally, when windows move etc

        - Need to use split regions to draw something
    
libgui supports no windows (check)
awm supports multiple windows for the same owner service
libgui supports multiple windows 
libgui supports partial draw

gui_view progressively grows memory as size changes

Crash reporter app (check)
When exit signal happens, send backtrace (check)
    How to get assertion message? (check)
    assert() itself can alert the crash reporter (check)

awm subscribes to process-died notifications from amc (check)
    Cleans up window (check)
    New amc syscall to flush pending messages from awm
        Flushes either from deliver queue or unknown-services message queue

libgui can send a resize event
    image_viewer resizes to native image dimensions when an image is opened

Two-way mapping in process death notifying so we can remove the watcher when the watcher dies

Desktop icons (check)

blit_layer_scaled
    For awm scaling windows during animations?

Do drivers (i.e. ATA or NIC) need to send EOI immediately and queue up reads after that?
    What happens if another IRQ comes in from a different device during data delivery before EOI?
    What happens if another IRQ comes in from the same device after EOI but before the queued read is performed?

GUI callbacks should be lists of function pointers instead of single pointers
    Ex. multiple callbacks can register key-down for the same view

As an optimisation, FAT allocator remembers where it last found a free entry
    Unset once we free a FAT entry

File manager has a 'format disk' button
Implement file copy UI (from initrd to hdd)
Implement finder that can enter and leave directories
ATA drive does DMA transfers / read sectors
File partial writes
Windows don't close when clicking 'X'

Use cpuid to get CPU model/vendor string, and display it in the UI along with other system info
Use x86_64 interrupt stack table (IST)
Use x86_64 Process Context Identifier (PCI) to reduce TLB misses with multitasking

Update build_quick to run when the build product is older than the source

Real-time clock (for wall clock datetime)
(urgent) On special events (Christmas, Halloween, New Year's Eve, summer equinox, Easter, Daisy BDay, etc.), have a novelty cursor that shows up s a holiday-appropriate icon (ex. little Christmas tree that looks like an arrow and functions as one as appropriate, a pumpkin as a loading icon)

Loading icon - program startup
    Could be a rotating (diamond) version of the square cursor

Crash in awm when preferences exits without calling gui_enter_event_loop()
