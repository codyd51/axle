#include "keymap.h"

keymap_t kb_us = {
//normal keys
{
  /* first row - indices 0 to 14 */
  0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  
  /* second row - indices 15 to 28 */
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', //Enter key
  
  /* 29 = Control, 30 - 41: third row */
  0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
  
  /* fourth row, indices 42 to 54, zeroes are shift-keys*/
  0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
  
  '*',
  
  /* Special keys */
  
  0, //ALT - 56
  ' ', // Space - 57
  0, //Caps lock - 58
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // F1 to F10 - 59 to 68
  0, //Num lock - 69
  0, //Scroll lock - 70
  0, //Home - 71
  0, //Up arrow - 72
  0, //Page up - 73
  '-',
  0, //Left arrow - 75
  0,
  0, //Right arrow -77
  '+',
  0, //End - 79
  0, //Dowm arrow - 80
  0, //Page down - 81
  0, //Insert - 82
  0, //Delete - 83
  0, 0, 0,
  0, //F11 - 87
  0, //F12 - 88
  0, //All others undefined
},
//caps
{
  /* first row - indices 0 to 14 */
  0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  
  /* second row - indices 15 to 28 */
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', //Enter key
  
  /* 29 = Control, 30 - 41: third row */
  0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
  
  /* fourth row, indices 42 to 54, zeroes are shift-keys*/
  0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
  
  '*',
  
  /* Special keys */
  
  0, //ALT - 56
  ' ', // Space - 57
  0, //Caps lock - 58
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // F1 to F10 - 59 to 68
  0, //Num lock - 69
  0, //Scroll lock - 70
  0, //Home - 71
  0, //Up arrow - 72
  0, //Page up - 73
  '-',
  0, //Left arrow - 75
  0,
  0, //Right arrow -77
  '+',
  0, //End - 79
  0, //Dowm arrow - 80
  0, //Page down - 81
  0, //Insert - 82
  0, //Delete - 83
  0, 0, 0,
  0, //F11 - 87
  0, //F12 - 88
  0, //All others undefined
},

// control_map
{
  29, // Ctrl
  56, // Alt
  0,  // AltGr
  42, // left Shift
  54, // right Shift
  58, // Caps lock
  70, // Scroll lock
  69  // Num lock
},

//Set the initial status of all control keys to "not active"
0
};

