#ifndef STD_CTYPE_H
#define STD_CTYPE_H

#include <stdbool.h>
#include <stdint.h>

/// Check if a character is alphanumeric
/// @param ch Character to check
/// @return True if the character is alphabetic or numeric, false otherwise
bool isalnum(char ch);

/// Check if a character is alphabetic
/// @param ch Character to check
/// @return True if the character is alphabetic, false otherwise
bool isalpha(char ch);

/// Check if a character is an ASCII value
/// @param ch Character to check
/// @return True if the character is ASCII, false otherwise
bool isascii(char ch);

/// Check if a character is an ASCII control character
/// @param ch Character to check
/// @return True if the character is an ASCII control character, false otherwise
bool iscntrl(char ch);

/// Check if a character is numeric
/// @param ch Character to check
/// @return True if the character is a digit, false otherwise
bool isdigit(char ch);

/// Check if a character is graphical (printable)
/// @param ch Character to check
/// @return True if the character is graphical, false otherwise
bool isgraph(char ch);

/// Check if a character is a lowercase letter
/// @param ch Character to check
/// @return True if the character is a lowercase letter, false otherwise
bool islower(char ch);

/// Check if a character is printable
/// @param ch Character to check
/// @return True if the character is printable, false otherwise
bool isprint(char ch);
/// Check if a character is an uppercase letter
/// @param ch Character to check
/// @return True if the character is an uppercase letter, false otherwise
bool isupper(char ch);

/// Check if a character is a hexadecimal digit
/// @param ch Character to check
/// @return True if the character is a hexadecimal digit, false otherwise
bool isxdigit(char ch);


/// Converts a hexadecimal digit to its corresponding integer value (0-15)
/// @param ch Hexadecimal character to convert to a numberic value
/// @return Integral value of the hexadecimal digit, or 0 if not a hexadecimal digit
int digittoint(char ch);

/// Converts a character into the ASCII character range by masking off the non-ASCII bits
/// @param ch Character to make ASCII
/// @return Character value after being coerced to ASCII range
char toascii(char ch);

/// Converts a character to its uppercase equivalent if it is alphabetic
/// @param ch Character to make uppercase
/// @return Uppercase version of ch, or ch if it wasn't alphabetic
char toupper(char ch);

/// Converts a character ot its lowercase equivalent if it is alphabetic
/// @param ch Character ot make lowercase
/// @return Lowercase version of ch, or ch if it wasn't alphabetic
char tolower(char ch);


// Bit flags for character attributes
#define CTYPE_LOWER (1 << 0)
#define CTYPE_UPPER (1 << 1)
#define CTYPE_DIGIT (1 << 2)
#define CTYPE_PUNCT (1 << 3)
#define CTYPE_HEX   (1 << 4)
#define CTYPE_SPACE (1 << 5)
#define CTYPE_CNTRL (1 << 6)
#define CTYPE_PRINT (1 << 7)

/// Array storing CTYPE_* attributes about characters
uint8_t _ctype[256];

#endif // STD_STRING_H
