axle provides two scheduler policies; MLFQ and round-robin.

Whenever a task relinquishes the CPU, its runtime is recorded in an internal structure. This is used to compute the overall CPU time consumed by each task. The user can press ctrl+p at any time to dump CPU utilization statistics to axle's syslog. This log includes the respective percentage of CPU time consumed by every task that has ran since boot.

axle has a daemon process which periodically updates blocked tasks waiting for an external event to wake. This daemon is called iosentinel, since it was originally restricted to just waking tasks upon I/O (such as a keystroke or mouse event). However, currently, tasks can block for other reasons, such as sleeping for a time interval or waiting for a child process to terminate.

When tasks are instantiated, they recieve an array which they populate with tasks they have spawned. Upon a fork(), the newly created process is added to the parent process's child tasks.

On a wait() syscall (or a variant thereof), the parent task blocks until one of its children terminates, at which point it will be woken by iosentinel. The parent will then do the work of tearing down the child process, and, if the parent isn't waiting for any more children to die, resume execution.

