TODO
---------------

PCI enumeration lives in a "PCI manager" process
This process has an awm window that displays PCI devices, 
and launches the appropriate drivers

Processes need a syscall to launch another process

Perhaps processes can register themselves with awm on _start
But this needs some way for processes to register their amc service 
name before starting. Perhaps an Info.plist?

If the above is done, then the PCI manager can also launch drivers via their AMC service name
Perhaps the Info.plist has enough info for the PCI manager to do discovery on available drivers

AMC services should also be able to "wait" until another AMC service is launched, then
in _start after registering the service a system-wide notification can be posted that this service is alive

Implement synchronous messaging: send, block until recv "ack"

Fix task starvation of same priority, runnable

Come up with better way to pass args to programs on startup

Driver library - provides in/out functions, assert, what else?

awm library - helpers for constructing window, other communications? update title bar?

The mouse can't pre-empt the RTL driver because they're the same priority, so it loses data
The RTL driver needs to be split into a GUI and message server...

Replace PMM mutex with spinlock

Don't map the null page

amc should be able to send smaller or larger messages
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

Timekeeping
    - Net timeouts/retransmissions
    - Liveliness pings
