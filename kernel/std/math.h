#ifndef STD_MATH_H
#define STD_MATH_H

#include <stdint.h>
#include "std_base.h"
#include "sincostan.h"

__BEGIN_DECLS

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

#define M_PI 3.1415926536
#define M_E 2.7182818285

STDAPI double pow(double x, double pow);
STDAPI unsigned long factorial(unsigned long x);

//trigonometric functions
STDAPI double cot(double val);
STDAPI double sec(double val);
STDAPI double csc(double val);
STDAPI double exp(double x);

//hyperbolic functions
STDAPI double sinh(double val);
STDAPI double cosh(double val);
STDAPI double tanh(double val);
STDAPI double coth(double val);
STDAPI double sech(double val);
STDAPI double csch(double val);

//inverse trigonometric functions
STDAPI double arcsin(double val);
STDAPI double arccos(double val);
STDAPI double arctan(double val);
STDAPI double arccot(double val);
STDAPI double arcsec(double val);
STDAPI double arccsc(double val);
STDAPI double atan2(double y, double x);

STDAPI float sqrt(const float x);
STDAPI int round(double x);
STDAPI double floor(double x);

#define RAND_MAX 32767
STDAPI uint32_t rand();
STDAPI void srand(unsigned int seed);

//linear interpolation
STDAPI float lerp(float a, float b, float c);

double log10(double x);
double ln(double x);

int ceil(float num);

__END_DECLS

#endif // STD_MATH_H
