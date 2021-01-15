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

Driver library - provides in/out functions, what else?

awm library - helpers for constructing window, other communications? update title bar?

The mouse can't pre-empt the RTL driver because they're the same priority, so it loses data
The RTL driver needs to be split into a GUI and message server...