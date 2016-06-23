#include <std/std.h>

//return system time with millisecond precision
uint32_t time();
//to be used when unique id's are required, but accuracy is not.
//if time_unique() is called on the same timestamp more than once, it gaurantees the later request(s)
//have a unique id of their own.
uint32_t time_unique();
