#include "std.h"
#include "kb.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

void terminal_putchar(char c);
void terminal_writestring(const char* data);
void terminal_removechar();