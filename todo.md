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