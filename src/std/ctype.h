#ifndef STD_CTYPE_H
#define STD_CTYPE_H

#include "std_base.h"
#include <stdbool.h>
#include <stdint.h>

__BEGIN_DECLS

/// Check if a character is alphanumeric
/// @param ch Character to check
/// @return True if the character is alphabetic or numeric, false otherwise
STDAPI bool isalnum(char ch);

/// Check if a character is alphabetic
/// @param ch Character to check
/// @return True if the character is alphabetic, false otherwise
STDAPI bool isalpha(char ch);

/// Check if a character is an ASCII value
/// @param ch Character to check
/// @return True if the character is ASCII, false otherwise
STDAPI bool isascii(char ch);

/// Check if a character is an ASCII control character
/// @param ch Character to check
/// @return True if the character is an ASCII control character, false otherwise
STDAPI bool iscntrl(char ch);

/// Check if a character is numeric
/// @param ch Character to check
/// @return True if the character is a digit, false otherwise
STDAPI bool isdigit(char ch);

/// Check if a character is graphical (printable)
/// @param ch Character to check
/// @return True if the character is graphical, false otherwise
STDAPI bool isgraph(char ch);

/// Check if a character is a lowercase letter
/// @param ch Character to check
/// @return True if the character is a lowercase letter, false otherwise
STDAPI bool islower(char ch);

/// Check if a character is printable
/// @param ch Character to check
/// @return True if the character is printable, false otherwise
STDAPI bool isprint(char ch);

/// Check if a character is an uppercase letter
/// @param ch Character to check
/// @return True if the character is an uppercase letter, false otherwise
STDAPI bool isupper(char ch);

/// Check if a character is a hexadecimal digit
/// @param ch Character to check
/// @return True if the character is a hexadecimal digit, false otherwise
STDAPI bool isxdigit(char ch);


/// Converts a hexadecimal digit to its corresponding integer value (0-15)
/// @param ch Hexadecimal character to convert to a numberic value
/// @return Integral value of the hexadecimal digit, or 0 if not a hexadecimal digit
STDAPI int digittoint(char ch);

/// Converts a character into the ASCII character range by masking off the non-ASCII bits
/// @param ch Character to make ASCII
/// @return Character value after being coerced to ASCII range
STDAPI char toascii(char ch);

/// Converts a character to its uppercase equivalent if it is alphabetic
/// @param ch Character to make uppercase
/// @return Uppercase version of ch, or ch if it wasn't alphabetic
STDAPI char toupper(char ch);

/// Converts a character ot its lowercase equivalent if it is alphabetic
/// @param ch Character ot make lowercase
/// @return Lowercase version of ch, or ch if it wasn't alphabetic
STDAPI char tolower(char ch);


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
STDAPI uint8_t _ctype[256];

__END_DECLS

#endif // STD_STRING_H
