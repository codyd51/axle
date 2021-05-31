#include <ctype.h>
#include <string.h>

#include "utils.h"

bool str_is_whitespace(const char* s) {
	while (*s != '\0') {
		if (!isspace((unsigned char)*s)) {
			return false;
		}
		s++;
	}
	return true;
}

bool str_ends_with(const char* s, uint32_t s_len, const char* t, uint32_t t_len) {
	// Modified from:
	// https://codereview.stackexchange.com/questions/54722/determine-if-one-string-occurs-at-the-end-of-another/54724
	// Check if t can fit in s
    if (s_len >= t_len) {
        // point s to where t should start and compare the strings from there
        return (0 == memcmp(t, s + (s_len - t_len), t_len));
    }
	// t was longer than s
    return 0;
}

int strcicmp(char const* a, char const* b) {
	// https://stackoverflow.com/questions/5820810/case-insensitive-string-comp-in-c
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
}
