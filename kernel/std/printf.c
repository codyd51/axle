#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>

#include <kernel/drivers/text_mode/text_mode.h>
#include <std/string.h>
#include <kernel/assert.h>

static int normalize(double *val) {
    int exponent = 0;
    double value = *val;

    while (value >= 1.0) {
        value /= 10.0;
        ++exponent;
    }

    while (value < 0.1) {
        value *= 10.0;
        --exponent;
    }
    *val = value;
    return exponent;
}

static void ftoa_fixed(char *buffer, double value) {
    /* carry out a fixed conversion of a double value to a string, with a precision of 5 decimal digits.
     * Values with absolute values less than 0.000001 are rounded to 0.0
     * Note: this blindly assumes that the buffer will be large enough to hold the largest possible result.
     * The largest value we expect is an IEEE 754 double precision real, with maximum magnitude of approximately
     * e+308. The C standard requires an implementation to allow a single conversion to produce up to 512
     * characters, so that's what we really expect as the buffer size.
     */

    int exponent = 0;
    int places = 0;
    static const int width = 4;

    if (value == 0.0) {
        buffer[0] = '0';
        buffer[1] = '\0';
    }

    if (value < 0.0) {
        *buffer++ = '-';
        value = -value;
    }

    exponent = normalize(&value);

    while (exponent > 0) {
        int digit = value * 10;
        *buffer++ = digit + '0';
        value = value * 10 - digit;
        ++places;
        --exponent;
    }

    if (places == 0) {
        *buffer++ = '0';
    }

    *buffer++ = '.';

    while (exponent < 0 && places < width) {
        *buffer++ = '0';
        --exponent;
        ++places;
    }

    while (places < width) {
        int digit = value * 10.0;
        *buffer++ = digit + '0';
        value = value * 10.0 - digit;
        ++places;
    }
    *buffer = '\0';
}

static unsigned int itoa_advanced(int value, unsigned int radix, unsigned int uppercase, unsigned int unsig,
     char *buffer, unsigned int zero_pad) {
    char    *pbuffer = buffer;
    int negative = 0;
    unsigned int    i, len;

    /* No support for unusual radixes. */
    if (radix > 16) {
        return 0;
    }

    if (value < 0 && !unsig) {
        negative = 1;
        value = -value;
    }

    /* This builds the string back to front ... */
    do {
        int digit = value % radix;
        *(pbuffer++) = (digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + digit - 10);
        value /= radix;
    } while (value > 0);

    for (i = (pbuffer - buffer); i < zero_pad; i++) {
        *(pbuffer++) = '0';
    }

    if (negative) {
        *(pbuffer++) = '-';
    }

    *(pbuffer) = '\0';

    /* ... now we reverse it (could do it recursively but will
     * conserve the stack space) */
    len = (pbuffer - buffer);
    for (i = 0; i < len / 2; i++) {
        char j = buffer[i]; buffer[i] = buffer[len-i-1];
        buffer[len-i-1] = j;
    }

    return len;
}

struct mini_buff {
    char *buffer, *pbuffer;
    unsigned int buffer_len;
};

static int buf_putc(int ch, struct mini_buff *b) {
    if ((unsigned int)((b->pbuffer - b->buffer) + 1) >= b->buffer_len) {
        return 0;
    }
    *(b->pbuffer++) = ch;
    *(b->pbuffer) = '\0';
    return 1;
}

static int buf_puts(char *s, unsigned int len, struct mini_buff *b) {
    unsigned int i;

    if (b->buffer_len - (b->pbuffer - b->buffer) - 1 < len) {
        len = b->buffer_len - (b->pbuffer - b->buffer) - 1;
    }

    /* Copy to buffer */
    for (i = 0; i < len; i++) {
        *(b->pbuffer++) = s[i];
    }
    *(b->pbuffer) = '\0';

    return len;
}

int vsnprintf(char *buffer, unsigned int buffer_len, const char *fmt, va_list va) {
    struct mini_buff b;
    // buffer for storing format arguments
    // C standard expects float formats to be as long as 512 chars
    // so that's the max size of this buffer
    char bf[24];
    char ch;

    b.buffer = buffer;
    b.pbuffer = buffer;
    b.buffer_len = buffer_len;

    while ((ch=*(fmt++))) {
        if ((unsigned int)((b.pbuffer - b.buffer) + 1) >= b.buffer_len) {
            break;
        }
        if (ch!='%') {
            buf_putc(ch, &b);
        }
        else {
            char zero_pad = 0;
            char *ptr;
            unsigned int len;

            ch=*(fmt++);

            /* Zero padding requested */
            if (ch=='0') {
                ch=*(fmt++);
                if (ch == '\0') {
                    goto end;
                }
                if (ch >= '0' && ch <= '9') {
                    zero_pad = ch - '0';
                }
                ch=*(fmt++);
            }

            switch (ch) {
                case 0:
                    goto end;

                case 'u':
                case 'd':
                    len = itoa_advanced(va_arg(va, unsigned int), 10, 0, (ch=='u'), bf, zero_pad);
                    buf_puts(bf, len, &b);
                    break;

                case 'x':
                case 'X':
                    len = itoa_advanced(va_arg(va, unsigned int), 16, (ch=='X'), 1, bf, zero_pad);
                    buf_puts(bf, len, &b);
                    break;

                case 'c' :
                    buf_putc((char)(va_arg(va, int)), &b);
                    break;
                
                // Nonstandard extension to print a string-with-length
                case '*':
                    len = va_arg(va, uint32_t);
                    ptr = va_arg(va, char*);
                    buf_puts(ptr, len, &b);
                    break;

                case 's' :
                    ptr = va_arg(va, char*);
                    buf_puts(ptr, strlen(ptr), &b);
                    break;

                case 'f' :
                    //no-op because we can't have a declaration directly after a label
                    do {} while (0);
                    //double because float gets promoted to double
                    double float_val = va_arg(va, double);
                    ftoa_fixed(bf, float_val);
                    buf_puts(bf, strlen(bf), &b);
                    break;

                default:
                    buf_putc(ch, &b);
                    break;
            }
        }
    }
end:
    return b.pbuffer - b.buffer;
}

int snprintf(char* buffer, unsigned int buffer_len, const char *fmt, ...) {
    int ret;
    va_list va;
    va_start(va, fmt);
    ret = vsnprintf(buffer, buffer_len, fmt, va);
    va_end(va);

    return ret;
}

typedef enum {
    PRINT_DESTINATION_TEXT_MODE,
    PRINT_DESTINATION_SERIAL,
} print_destination;

static int print_common(print_destination dest, const char* fmt, va_list va) {
    if (dest != PRINT_DESTINATION_TEXT_MODE && dest != PRINT_DESTINATION_SERIAL) {
        assert(0, "print_common called with bad args");
        return -1;
    }
    if (dest == PRINT_DESTINATION_TEXT_MODE) {
        assert(dest != PRINT_DESTINATION_TEXT_MODE, "Deprecated");
        return -1;
    }

    int ret;
    char buf[512];

    ret = vsnprintf((char*)buf, sizeof(buf), fmt, va);
    switch (dest) {
        case PRINT_DESTINATION_SERIAL:
        default:
            serial_puts(buf);
            break;
    }

    return ret;
}

int printf(const char* format, ...) {
    va_list arg_list;
    va_start(arg_list, format);
    int ret = print_common(PRINT_DESTINATION_SERIAL, format, arg_list);
    va_end(arg_list);
    return ret;
}

int printk(const char* format, ...) {
    va_list arg_list;
    va_start(arg_list, format);
    int ret = print_common(PRINT_DESTINATION_SERIAL, format, arg_list);
    va_end(arg_list);
    return ret;

}

static int print_annotated_common(print_destination dest, const char* prefix, const char* suffix, const char* format, va_list va) {
    int total_len = 0;

    total_len += print_common(dest, prefix, NULL);
    total_len += print_common(dest, format, va);
    total_len += print_common(dest, suffix, NULL);

    return total_len;
}

// TODO(PT): Drop printf() or printk() as the variants now do the same thing
int printf_dbg(const char* format, ...) {
    va_list va;
    va_start(va, format);
    int ret = print_annotated_common(PRINT_DESTINATION_SERIAL, "[debug ", "]\n", format, va);
    va_end(va);
    return ret;
}

int printk_dbg(const char* format, ...) {
    va_list va;
    va_start(va, format);
    int ret = print_annotated_common(PRINT_DESTINATION_SERIAL, "[debug ", "]\n", format, va);
    va_end(va);
    return ret;
}

int printf_info(const char* format, ...) {
    va_list va;
    va_start(va, format);
    int ret = print_annotated_common(PRINT_DESTINATION_SERIAL, "[info ", "]\n", format, va);
    va_end(va);
    return ret;
}

int printk_info(const char* format, ...) {
    va_list va;
    va_start(va, format);
    int ret = print_annotated_common(PRINT_DESTINATION_SERIAL, "[info ", "]\n", format, va);
    va_end(va);
    return ret;
}

int printf_err(const char* format, ...) {
    va_list va;
    va_start(va, format);
    int ret = print_annotated_common(PRINT_DESTINATION_SERIAL, "[error ", "]\n", format, va);
    va_end(va);
    return ret;
}

int printk_err(const char* format, ...) {
    va_list va;
    va_start(va, format);
    int ret = print_annotated_common(PRINT_DESTINATION_SERIAL, "[error ", "]\n", format, va);
    va_end(va);
    return ret;
}


//unimplemented functions
int vprintf() {
    NotImplemented();
}
int putchar(char ch) {
    printf("%c", ch);
    //NotImplemented();
}
int sprintf() {
    NotImplemented();
}
int output() {
    NotImplemented();
}
