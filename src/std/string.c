#include "string.h"
#include <std/kheap.h>

#define ALIGN_DOWN(base, size)  ((base) & -((__typeof__ (base)) (size)))

#define ALIGN_UP(base, size)    ALIGN_DOWN ((base) + (size) - 1, (size))

#define PTR_ALIGN_DOWN(base, size) \
	  ((__typeof__ (base)) ALIGN_DOWN ((uintptr_t) (base), (size)))

char* itoa(int i, char b[]) {
	char const digit[] = "0123456789";
	char* p = b;
	if (i < 0) {
		*p++ = '-';
		i *= -1;
	}
	int shifter = i;
	do {
		//move to where representation ends
		++p;
		shifter = shifter/10;
	} while(shifter);
	
	*p = '\0';
	
	do {
		//move back, inserting digits as we go
		*--p = digit[i%10];
		i = i/10;
	} while (i);
	return b;
}

long long int atoi(const char *c) {
    long long int value = 0;
    int sign = 1;
    if( *c == '+' || *c == '-' ) {
        if(*c == '-') sign = -1;
        c++;
    }
    while (isdigit(*c))
    {
        value *= 10;
        value += (int) (*c-'0');
        c++;
    }
    return (value * sign);
}

char* strcat(char *dest, const char *src) {
	size_t i,j;
	for (i = 0; dest[i] != '\0'; i++)
			;
	for (j = 0; src[j] != '\0'; j++)
			dest[i+j] = src[j];
	dest[i+j] = '\0';
	return dest;
}

char* strccat(char* dest, char src) {
	size_t i;
	for (i = 0; dest[i] != '\0'; i++)
		;
	dest[i] = src;
	dest[i+1] = '\0';
	return dest;
}

int strcmp(const char *lhs, const char *rhs) {
	while (*lhs == *rhs) {
        	if (*lhs == '\0' || *rhs == '\0') break;
        
		lhs++;
        	rhs++;
    	}
    
    	return *lhs - *rhs;
}

char* delchar(char* str) {
	size_t i;
	for (i = 0; str[i] != '\0'; i++)
		;
	if (i >= 1) {
		str[i-1] = '\0';
		return str;
	}
	return "";
}

char *strtok_r (char *s, const char *delim, char **save_ptr) {
	char *token;

	if (s == NULL) {
		s = *save_ptr;
	}

	s += strspn(s, delim);
	if (*s == '\0') {
		*save_ptr = s;
		return NULL;
	}

	token = s;
	s = strpbrk(token, delim);
	if (s == NULL) {
		*save_ptr = strchr(token, '\0');
	} else {
		*s = '\0';
		*save_ptr = s + 1;
	}
	return token;	  
}

char **strsplit(const char *string, const char *delim, size_t *out) {
	if (!string || !string[0]) {
		return NULL;
	}

	char *str = strdup(string);
	// Allocate as much space as characters in the string
	size_t size = strlen(str);
	char **v = calloc(size, sizeof(char *));

	char *saveptr;
	char *s = strtok_r(str, delim, &saveptr);
	size_t i;
	for (i = 0; s; i++) {
		v[i] = strdup(s);
		s = strtok_r(NULL, delim, &saveptr);
	}
	// Then resize the array to a smaller size
	v = realloc(v, (i + 1) * sizeof(char *));

	v[i] = NULL;
	if (out) {
		*out = i;
	}

	kfree(str);

	return v;
}

size_t strlen(const char* str) {
	size_t ret = 0;
	while (str[ret] != 0) {
		ret++;
	}
	return ret;
}

char *strcpy(char *dest, const char *src) {
	int i = 0;
	while (1) {
		dest[i] = src[i];
		if (dest[i] == '\0') {
			break;
		}
		i++;
	}
	return dest;
}

char* strncpy(char* dest, const char* src, size_t count) {
	int i = 0;
	while (i < count) {
		dest[i] = src[i];
		if (dest[i] == '\0') {
			break;
		}
		i++;
	}
	if (i != count) {
		for (; i < count; i++) {
			dest[i] = '\0';
		}
	}

	return dest;
}

int isblank(char c) {
	return (c == ' ' || c == '\t');
}

int isspace(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\12');
}

char *strdup (const char *s) {
	char *d = kmalloc (strlen (s) + 1);
	if (d == NULL) return NULL;
	strcpy (d,s);
	return d;
}

size_t strspn(const char *str, const char *accept) {
	if (accept[0] == '\0') {
		return 0;
	}
	if (accept[1] == '\0') {
		const char *a = str;
		for (; *str == *accept; str++);
			return str - a;
	}


	unsigned char table[256];
	unsigned char *p = memset(table, 0, 64);
	memset(p + 64, 0, 64);
	memset(p + 128, 0, 64);
	memset(p + 192, 0, 64);

	unsigned char *s = (unsigned char*) accept;

	do {
		p[*s++] = 1;
	} while (*s);

	s = (unsigned char*) str;
	if (!p[s[0]]) return 0;
	if (!p[s[1]]) return 1;
	if (!p[s[2]]) return 2;
	if (!p[s[3]]) return 3;

	s = (unsigned char *) PTR_ALIGN_DOWN (s, 4);

	unsigned int c0, c1, c2, c3;
	do {
		s += 4;
		c0 = p[s[0]];
		c1 = p[s[1]];
		c2 = p[s[2]];
		c3 = p[s[3]];
	} while ((c0 & c1 & c2 & c3) != 0);

	size_t count = s - (unsigned char *) str;
	return (c0 & c1) == 0 ? count + c0 : count + c2 + 2;
}

char *__strchrnul (const char *s, int c_in) {
	const unsigned char *char_ptr;
	const unsigned long int *longword_ptr;
	unsigned long int longword, magic_bits, charmask;
	unsigned char c;

	c = (unsigned char) c_in;

	/* Handle the first few characters by reading one character at a time.
	Do this until CHAR_PTR is aligned on a longword boundary.  */
	for (char_ptr = (const unsigned char *) s;
		((unsigned long int) char_ptr & (sizeof (longword) - 1)) != 0;
		++char_ptr)
		if (*char_ptr == c || *char_ptr == '\0')
			return (void *) char_ptr;

		/* All these elucidatory comments refer to 4-byte longwords,
		but the theory applies equally well to 8-byte longwords.  */
		longword_ptr = (unsigned long int *) char_ptr;

		/* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
		the "holes."  Note that there is a hole just to the left of
		each byte, with an extra at the end:

		bits:  01111110 11111110 11111110 11111111
		bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

		The 1-bits make sure that carries propagate to the next 0-bit.
		The 0-bits provide holes for carries to fall into.  */
		magic_bits = -1;
		magic_bits = magic_bits / 0xff * 0xfe << 1 >> 1 | 1;

		/* Set up a longword, each of whose bytes is C.  */
		charmask = c | (c << 8);
		charmask |= charmask << 16;
		if (sizeof (longword) > 4)
		/* Do the shift in two steps to avoid a warning if long has 32 bits.  */
			charmask |= (charmask << 16) << 16;
		if (sizeof (longword) > 8)
			abort ();

		/* Instead of the traditional loop which tests each character,
		we will test a longword at a time.  The tricky part is testing
		if *any of the four* bytes in the longword in question are zero.  */
		for (;;)
		{
		/* We tentatively exit the loop if adding MAGIC_BITS to
		LONGWORD fails to change any of the hole bits of LONGWORD.

		1) Is this safe?  Will it catch all the zero bytes?
		Suppose there is a byte with all zeros.  Any carry bits
		propagating from its left will fall into the hole at its
		least significant bit and stop.  Since there will be no
		carry from its most significant bit, the LSB of the
		byte to the left will be unchanged, and the zero will be
		detected.

		2) Is this worthwhile?  Will it ignore everything except
		zero bytes?  Suppose every byte of LONGWORD has a bit set
		somewhere.  There will be a carry into bit 8.  If bit 8
		is set, this will carry into bit 16.  If bit 8 is clear,
		one of bits 9-15 must be set, so there will be a carry
		into bit 16.  Similarly, there will be a carry into bit
		24.  If one of bits 24-30 is set, there will be a carry
		into bit 31, so all of the hole bits will be changed.

		The one misfire occurs when bits 24-30 are clear and bit
		31 is set; in this case, the hole at bit 31 is not
		changed.  If we had access to the processor carry flag,
		we could close this loophole by putting the fourth hole
		at bit 32!

		So it ignores everything except 128's, when they're aligned
		properly.

		3) But wait!  Aren't we looking for C as well as zero?
		Good point.  So what we do is XOR LONGWORD with a longword,
		each of whose bytes is C.  This turns each byte that is C
		into a zero.  */

			longword = *longword_ptr++;

			/* Add MAGIC_BITS to LONGWORD.  */
			if ((((longword + magic_bits)

			/* Set those bits that were unchanged by the addition.  */
				^ ~longword)

			/* Look at only the hole bits.  If any of the hole bits
			are unchanged, most likely one of the bytes was a
			zero.  */
				& ~magic_bits) != 0 ||

			/* That caught zeroes.  Now test for C.  */
				((((longword ^ charmask) + magic_bits) ^ ~(longword ^ charmask))
					& ~magic_bits) != 0)
			{
			/* Which of the bytes was C or zero?
			If none of them were, it was a misfire; continue the search.  */

				const unsigned char *cp = (const unsigned char *) (longword_ptr - 1);

				if (*cp == c || *cp == '\0')
					return (char *) cp;
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
				if (sizeof (longword) > 4)
				{
					if (*++cp == c || *cp == '\0')
						return (char *) cp;
					if (*++cp == c || *cp == '\0')
						return (char *) cp;
					if (*++cp == c || *cp == '\0')
						return (char *) cp;
					if (*++cp == c || *cp == '\0')
						return (char *) cp;
				}
		}
	}

	/* This should never happen.  */
	return NULL;
}

size_t strcspn(const char *str, const char *reject) {
	if ((reject[0] == '\0') || (reject[1] == '\0'))
		return __strchrnul (str, reject [0]) - str;

/* Use multiple small memsets to enable inlining on most targets.  */
	unsigned char table[256];
	unsigned char *p = memset (table, 0, 64);
	memset (p + 64, 0, 64);
	memset (p + 128, 0, 64);
	memset (p + 192, 0, 64);

	unsigned char *s = (unsigned char*) reject;
	unsigned char tmp;
	do
	p[tmp = *s++] = 1;
	while (tmp);

	s = (unsigned char*) str;
	if (p[s[0]]) return 0;
	if (p[s[1]]) return 1;
	if (p[s[2]]) return 2;
	if (p[s[3]]) return 3;

	s = (unsigned char *) PTR_ALIGN_DOWN (s, 4);

	unsigned int c0, c1, c2, c3;
	do {
		s += 4;
		c0 = p[s[0]];
		c1 = p[s[1]];
		c2 = p[s[2]];
		c3 = p[s[3]];
	}
	while ((c0 | c1 | c2 | c3) == 0);

	size_t count = s - (unsigned char *) str;
	return (c0 | c1) != 0 ? count - c0 + 1 : count - c2 + 3;
}

char *strpbrk(const char *s, const char *accept) {
	s += strcspn (s, accept);
	return *s ? (char *)s : NULL;
}

char *strchr(const char *s, int c_in) {
	const unsigned char *char_ptr;
	const unsigned long int *longword_ptr;
	unsigned long int longword, magic_bits, charmask;
	unsigned char c;

	c = (unsigned char) c_in;

	for (char_ptr = (const unsigned char *) s;
		((unsigned long int) char_ptr & (sizeof (longword) - 1)) != 0;
		++char_ptr) {
		if (*char_ptr == c) {
			return (void *) char_ptr;
		} else if (*char_ptr == '\0') {
			return NULL;
		}
	}

	longword_ptr = (unsigned long int *) char_ptr;

	magic_bits = -1;
	magic_bits = magic_bits / 0xff * 0xfe << 1 >> 1 | 1;

	charmask = c | (c << 8);
	charmask |= charmask << 16;
	if (sizeof (longword) > 4) {
		charmask |= (charmask << 16) << 16;
	}
	if (sizeof (longword) > 8) {
		abort ();
	}
	for (;;) {
		longword = *longword_ptr++;

		if ((((longword + magic_bits) ^ ~longword) & ~magic_bits) != 0 ||
				((((longword ^ charmask) + magic_bits) ^ ~(longword ^ charmask))
				& ~magic_bits) != 0) {

			const unsigned char *cp = (const unsigned char *) (longword_ptr - 1);

				if (*cp == c)
					return (char *) cp;
				else if (*cp == '\0')
					return NULL;
				if (*++cp == c)
					return (char *) cp;
				else if (*cp == '\0')
					return NULL;
				if (*++cp == c)
					return (char *) cp;
				else if (*cp == '\0')
					return NULL;
				if (*++cp == c)
					return (char *) cp;
				else if (*cp == '\0')
					return NULL;
				if (sizeof (longword) > 4) {
					if (*++cp == c) {
						return (char *) cp;
					} else if (*cp == '\0')
						return NULL;
					if (*++cp == c) {
						return (char *) cp;
					} else if (*cp == '\0') 
						return NULL;
					if (*++cp == c) {
						return (char *) cp;
					} else if (*cp == '\0') {
						return NULL;
					}
					if (*++cp == c) {
						return (char *) cp;
					} else if (*cp == '\0') {
						return NULL;
					}
				}
			}
	}
	return NULL;
}

