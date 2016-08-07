#include <kernel/drivers/rtc/clock.h>
#include "math.h"
#include "rand.h"

double pow(double x, double pow) {
	double ret = x;
	for (int i = 0; i < pow; i++) {
		ret *= x;
	}
	return ret;
}

unsigned long factorial(unsigned long x) {
	if (x == 0) return 1;
	return (x * factorial(x - 1));
	/*
	int ret = 1;
	for (int i = 1; i <= x; x++) {
		ret *= i;
	}
	return ret;
	*/
}

double cot(double x) {
		return cos(x)/sin(x);
}

double sec(double x) {
		return 1/cos(x);
}

double csc(double x) {
		return 1/sin(x);
}

double exp(double x) {
	return pow(M_E, x);
}

double sinh(double val) {
		return (pow(M_E, val) - pow(M_E, -val)) / 2;
}

double cosh(double val) {
		return (pow(M_E, val) + pow(M_E, -val)) / 2;
}

double tanh(double val) {
		return sinh(val)/cosh(val);
}

double coth(double val) {
		return cosh(val)/sinh(val);
}

double sech(double val) {
		return 1/cosh(val);
}

double csch(double val) {
		return 1/sinh(val);
}

double arcsin(double x) {
	//taylor series for arcsin
	double ret = x;
	ret += (pow(x, 3))/6;
	ret += (3*pow(x, 5))/40;
	ret += (5*pow(x, 7))/112;
	return ret;
}

double arccos(double val) {
	//arccos is arcsin phase shifted pi/2
	return (M_PI/2) - arcsin(val);
}

double arctan(double x) {
	double ret = x;
	ret -= (pow(x, 3)/3);
	ret += (pow(x, 5)/5);
	ret -= (pow(x, 7)/7);
	return ret;
}

double arccot(double val) {
		//arctan phase shifted pi/2
		return (M_PI/2) - arctan(val);
}

double arcsec(double val) {
		return arccos(1/val);
}

double arccsc(double val) {
		return arcsin(1/val);
}

double atan2(double y, double x) {
	if (x > 0) {
		return arctan(y/x);
	}
	else if (x < 0 && y >= 0) {
		return arctan(y/x) + M_PI;
	}
	else if (x < 0 && y < 0) {
		return arctan(y/x) - M_PI;
	}
	else if (x == 0 && y > 0) {
		return (M_PI/2);
	}
	else if (x == 0 && y < 0) {
		return -(M_PI/2);
	}
	
	//if x and y == 0, undefined
	return -1;
}

int abs(int val) {
	if (val < 0) return -val;
	return val;
}

#define SQRT_MAGIC_F 0x5f3759df 
float sqrt(const float x) {
	const float xhalf = 0.5f*x;
	
	//get bits for floating value
	union {
		float x;
		int i;
	} u;
	u.x = x;
	u.i = SQRT_MAGIC_F - (u.i >> 1); //gives initial guess y0
	return x*u.x*(1.5f - xhalf*u.x*u.x); //Newton step, repeating increases accuracy 
}   

int round(double x) {
	if (x < 0.0) return (int)(x - 0.5);
	return (int)(x + 0.5);
}

static unsigned long int next = 1;
uint32_t rand() {
	//seed rand
	//ensure we always seed with a unique stamp by using time_unique
	srand(time_unique());
/*
	//use LCG to generate pseudorandom values
	next = next * 1103515245 + 12345;
	return (unsigned int)(next / 65536) % RAND_MAX;
*/
	/*
	static unsigned int z1 = 12345, z2 = 12345, z3 = 12345, z4 = 12345;
	unsigned int b;
	b = ((z1 << 6) ^ z1) >> 13;
	z1 = ((z1 & 4294967294U) << 18) ^ b;
	b  = ((z2 << 2) ^ z2) >> 27; 
	z2 = ((z2 & 4294967288U) << 2) ^ b;
	b  = ((z3 << 13) ^ z3) >> 21;
	z3 = ((z3 & 4294967280U) << 7) ^ b;
   	b  = ((z4 << 3) ^ z4) >> 12;
   	z4 = ((z4 & 4294967168U) << 13) ^ b;
   	return (unsigned)(z1 ^ z2 ^ z3 ^ z4);
	*/
	static mtwist* mt;
	if (!mt) {
		mt = mtwist_new();
		mtwist_init(mt, time_unique());
	}

	return mtwist_rand(mt);
}

void srand(unsigned int seed) {
	next = seed;
}
