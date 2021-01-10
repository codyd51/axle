#ifndef AWM_H
#define AWM_H

#include <kernel/amc.h>

// Commands that can be sent and recevied via an amc_msg_body_command_ptr_t

// Overload the "send" and "receive" names to be the same command
// When AWM receives it, it will interpret as a request to provide a framebuffer
// When a client receives it, it will interpret as a provided framebuffer
// Sent to awm as an amc_command_msg
#define AWM_REQUEST_WINDOW_FRAMEBUFFER (1 << 0)
// Sent from awm as an amc_command_ptr_msg
#define AWM_CREATED_WINDOW_FRAMEBUFFER (1 << 0)

#define AWM_WINDOW_REDRAW_READY (1 << 1)

#endif