#ifndef MATH_H
#define MATH_H

#define CMP(op, a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	(_a op _b) ? _a : _b; \
})
#define MIN(a, b) CMP(<=, a, b)
#define min(a, b) MIN(a, b)

#define MAX(a, b) CMP(>, a, b)
#define max(a, b) MAX(a, b)

#define abs(val) ((val) < 0) ? -(val) : (val)

#endif