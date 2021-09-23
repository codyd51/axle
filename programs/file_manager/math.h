#ifndef MATH_H
#define MATH_H

#ifndef CMP
#define CMP(op, a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	(_a op _b) ? _a : _b; \
})
#endif

#ifndef MIN
#define MIN(a, b) CMP(<=, a, b)
#endif

#ifndef min
#define min(a, b) MIN(a, b)
#endif

#ifndef MAX
#define MAX(a, b) CMP(>, a, b)
#endif

#ifndef max
#define max(a, b) MAX(a, b)
#endif

#ifndef abs
#define abs(val) ((val) < 0) ? -(val) : (val)
#endif

#endif