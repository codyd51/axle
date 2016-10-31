#include "ctype.h"

// This array holds one byte per possible character value. Each byte
// has certain flags set which describe the class of the character.
// For example, any character whose index in this array has the
// CTYPE_HEX bit set is a possible character in a hexadecimal value.
uint8_t _ctype[256] = {
	CTYPE_CNTRL,                              // \x00 (NUL) '\0'
	CTYPE_CNTRL,                              // \x01 (SOH)
	CTYPE_CNTRL,                              // \x02 (STX)
	CTYPE_CNTRL,                              // \x03 (ETX)
	CTYPE_CNTRL,                              // \x04 (EOT)
	CTYPE_CNTRL,                              // \x05 (ENQ)
	CTYPE_CNTRL,                              // \x06 (ACK)
	CTYPE_CNTRL,                              // \x07 (BEL) '\a'
	CTYPE_CNTRL,                              // \x08 (BS)
	CTYPE_SPACE | CTYPE_CNTRL,                // \x09 (HT)  '\t'
	CTYPE_SPACE | CTYPE_CNTRL,                // \x0A (LF)  '\n'
	CTYPE_SPACE | CTYPE_CNTRL,                // \x0B (VT)  '\v'
	CTYPE_SPACE | CTYPE_CNTRL,                // \x0C (FF)  '\f'
	CTYPE_SPACE | CTYPE_CNTRL,                // \x0D (CR)  '\r'
	CTYPE_CNTRL,                              // \x0E (SI)
	CTYPE_CNTRL,                              // \x0F (SO)
	CTYPE_CNTRL,                              // \x10 (DLE)
	CTYPE_CNTRL,                              // \x11 (DC1)
	CTYPE_CNTRL,                              // \x12 (DC2)
	CTYPE_CNTRL,                              // \x13 (DC3)
	CTYPE_CNTRL,                              // \x14 (DC4)
	CTYPE_CNTRL,                              // \x15 (NAK)
	CTYPE_CNTRL,                              // \x16 (SYN)
	CTYPE_CNTRL,                              // \x17 (ETB)
	CTYPE_CNTRL,                              // \x18 (CAN)
	CTYPE_CNTRL,                              // \x19 (EM)
	CTYPE_CNTRL,                              // \x1A (SUB)
	CTYPE_CNTRL,                              // \x1B (ESC) '\e'
	CTYPE_CNTRL,                              // \x1C (FS)
	CTYPE_CNTRL,                              // \x1D (GS)
	CTYPE_CNTRL,                              // \x1E (RS)
	CTYPE_CNTRL,                              // \x1F (US)
	CTYPE_PRINT | CTYPE_SPACE,                // \x20 ' ' (space)
	CTYPE_PRINT | CTYPE_PUNCT,                // \x21 '!'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x22 '"'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x23 '#'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x24 '$'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x25 '%'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x26 '&'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x27 '\'' (apostrophe)
	CTYPE_PRINT | CTYPE_PUNCT,                // \x28 '('
	CTYPE_PRINT | CTYPE_PUNCT,                // \x29 ')'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x2A '*'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x2B '+'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x2C ','
	CTYPE_PRINT | CTYPE_PUNCT,                // \x2D '-'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x2E '.'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x2F '/'
	CTYPE_PRINT | CTYPE_DIGIT | CTYPE_HEX,    // \x30 '0'
	CTYPE_PRINT | CTYPE_DIGIT | CTYPE_HEX,    // \x31 '1'
	CTYPE_PRINT | CTYPE_DIGIT | CTYPE_HEX,    // \x32 '2'
	CTYPE_PRINT | CTYPE_DIGIT | CTYPE_HEX,    // \x33 '3'
	CTYPE_PRINT | CTYPE_DIGIT | CTYPE_HEX,    // \x34 '4'
	CTYPE_PRINT | CTYPE_DIGIT | CTYPE_HEX,    // \x35 '5'
	CTYPE_PRINT | CTYPE_DIGIT | CTYPE_HEX,    // \x36 '6'
	CTYPE_PRINT | CTYPE_DIGIT | CTYPE_HEX,    // \x37 '7'
	CTYPE_PRINT | CTYPE_DIGIT | CTYPE_HEX,    // \x38 '8'
	CTYPE_PRINT | CTYPE_DIGIT | CTYPE_HEX,    // \x39 '9'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x3A ':'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x3B ';'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x3C '<'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x3D '='
	CTYPE_PRINT | CTYPE_PUNCT,                // \x3E '>'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x3F '?'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x40 '@'
	CTYPE_PRINT | CTYPE_UPPER | CTYPE_HEX,    // \x41 'A'
	CTYPE_PRINT | CTYPE_UPPER | CTYPE_HEX,    // \x42 'B'
	CTYPE_PRINT | CTYPE_UPPER | CTYPE_HEX,    // \x43 'C'
	CTYPE_PRINT | CTYPE_UPPER | CTYPE_HEX,    // \x44 'D'
	CTYPE_PRINT | CTYPE_UPPER | CTYPE_HEX,    // \x45 'E'
	CTYPE_PRINT | CTYPE_UPPER | CTYPE_HEX,    // \x46 'F'
	CTYPE_PRINT | CTYPE_UPPER,                // \x47 'G'
	CTYPE_PRINT | CTYPE_UPPER,                // \x48 'H'
	CTYPE_PRINT | CTYPE_UPPER,                // \x49 'I'
	CTYPE_PRINT | CTYPE_UPPER,                // \x4A 'J'
	CTYPE_PRINT | CTYPE_UPPER,                // \x4B 'K'
	CTYPE_PRINT | CTYPE_UPPER,                // \x4C 'L'
	CTYPE_PRINT | CTYPE_UPPER,                // \x4D 'M'
	CTYPE_PRINT | CTYPE_UPPER,                // \x4E 'N'
	CTYPE_PRINT | CTYPE_UPPER,                // \x4F 'O'
	CTYPE_PRINT | CTYPE_UPPER,                // \x50 'P'
	CTYPE_PRINT | CTYPE_UPPER,                // \x51 'Q'
	CTYPE_PRINT | CTYPE_UPPER,                // \x52 'R'
	CTYPE_PRINT | CTYPE_UPPER,                // \x53 'S'
	CTYPE_PRINT | CTYPE_UPPER,                // \x54 'T'
	CTYPE_PRINT | CTYPE_UPPER,                // \x55 'U'
	CTYPE_PRINT | CTYPE_UPPER,                // \x56 'V'
	CTYPE_PRINT | CTYPE_UPPER,                // \x57 'W'
	CTYPE_PRINT | CTYPE_UPPER,                // \x58 'X'
	CTYPE_PRINT | CTYPE_UPPER,                // \x59 'Y'
	CTYPE_PRINT | CTYPE_UPPER,                // \x5A 'Z'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x5B '['
	CTYPE_PRINT | CTYPE_PUNCT,                // \x5C '\'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x5D ']'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x5E '^'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x5F '_'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x60 '`'
	CTYPE_PRINT | CTYPE_LOWER | CTYPE_HEX,    // \x61 'a'
	CTYPE_PRINT | CTYPE_LOWER | CTYPE_HEX,    // \x62 'b'
	CTYPE_PRINT | CTYPE_LOWER | CTYPE_HEX,    // \x63 'c'
	CTYPE_PRINT | CTYPE_LOWER | CTYPE_HEX,    // \x64 'd'
	CTYPE_PRINT | CTYPE_LOWER | CTYPE_HEX,    // \x65 'e'
	CTYPE_PRINT | CTYPE_LOWER | CTYPE_HEX,    // \x66 'f'
	CTYPE_PRINT | CTYPE_LOWER,                // \x67 'g'
	CTYPE_PRINT | CTYPE_LOWER,                // \x68 'h'
	CTYPE_PRINT | CTYPE_LOWER,                // \x69 'i'
	CTYPE_PRINT | CTYPE_LOWER,                // \x6A 'j'
	CTYPE_PRINT | CTYPE_LOWER,                // \x6B 'k'
	CTYPE_PRINT | CTYPE_LOWER,                // \x6C 'l'
	CTYPE_PRINT | CTYPE_LOWER,                // \x6D 'm'
	CTYPE_PRINT | CTYPE_LOWER,                // \x6E 'n'
	CTYPE_PRINT | CTYPE_LOWER,                // \x6F 'o'
	CTYPE_PRINT | CTYPE_LOWER,                // \x70 'p'
	CTYPE_PRINT | CTYPE_LOWER,                // \x71 'q'
	CTYPE_PRINT | CTYPE_LOWER,                // \x72 'r'
	CTYPE_PRINT | CTYPE_LOWER,                // \x73 's'
	CTYPE_PRINT | CTYPE_LOWER,                // \x74 't'
	CTYPE_PRINT | CTYPE_LOWER,                // \x75 'u'
	CTYPE_PRINT | CTYPE_LOWER,                // \x76 'v'
	CTYPE_PRINT | CTYPE_LOWER,                // \x77 'w'
	CTYPE_PRINT | CTYPE_LOWER,                // \x78 'x'
	CTYPE_PRINT | CTYPE_LOWER,                // \x79 'y'
	CTYPE_PRINT | CTYPE_LOWER,                // \x7A 'z'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x7B '{'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x7C '|'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x7D '}'
	CTYPE_PRINT | CTYPE_PUNCT,                // \x7E '~'
	CTYPE_CNTRL,                              // \x7F (DEL) '\b'
	// Everything else is 0, because it's not ASCII
};


bool isalnum(char ch) {
	return _ctype[(uint8_t)ch] & (CTYPE_LOWER | CTYPE_UPPER | CTYPE_DIGIT);
}

bool isalpha(char ch) {
	return _ctype[(uint8_t)ch] & (CTYPE_LOWER | CTYPE_UPPER);
}

bool isascii(char ch) {
	// ASCII if 8th bit is zero
	return !(ch >> 7);
}

bool iscntrl(char ch) {
	return _ctype[(uint8_t)ch] & (CTYPE_CNTRL);
}

bool isdigit(char ch) {
	return _ctype[(uint8_t)ch] & CTYPE_DIGIT;
}

bool isgraph(char ch) {
	return _ctype[(uint8_t)ch] & (CTYPE_LOWER | CTYPE_UPPER | CTYPE_DIGIT | CTYPE_PUNCT);
}

bool islower(char ch) {
	return _ctype[(uint8_t)ch] & CTYPE_LOWER;
}

bool isprint(char ch) {
	return _ctype[(uint8_t)ch] & CTYPE_PRINT;
}

bool isupper(char ch) {
	return _ctype[(uint8_t)ch] & CTYPE_UPPER;
}

bool isxdigit(char ch) {
	return _ctype[(uint8_t)ch] & CTYPE_HEX;
}


int digittoint(char ch) {
	if(_ctype[(uint8_t)ch] & CTYPE_DIGIT) {
		// Character is 0-9, so get the value
		return ch - '0';
	}
	else if(_ctype[(uint8_t)ch] & CTYPE_HEX) {
		// Character is a-f or A-F, so force to lowercase and convert to value
		return (ch | 0x20) - 'a' + 0xa;
	}
	else {
		// Not a hexadecimal value
		return 0;
	}
}

char toascii(char ch) {
	// Mask character to only ASCII bits
	return ch & 0x7f;
}

char toupper(char ch) {
	// If already uppercase or not an alphabetic, just return the character
	if (_ctype[(uint8_t)ch] & CTYPE_LOWER) {
		ch &= ~0x20;
	}
	return ch;
}

char tolower(char ch) {
	// If already lowercase or not an alphabetic, just return the character
	if (_ctype[(uint8_t)ch] & CTYPE_UPPER) {
		ch |= 0x20;
	}
	return ch;
}
