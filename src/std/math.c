#include "math.h"

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
}

double sin(double x) {
	//approximate taylor series for sin
	double ret = x;
	ret -= (pow(x, 3)/factorial(3));
	ret += (pow(x, 5)/factorial(5));
	ret -= (pow(x, 7)/factorial(7));
	return ret;
}

double cos(double x) {
	//approximate taylor series for cos
	double ret = x;
	ret -= (pow(x, 2)/factorial(2));
	ret += (pow(x, 4)/factorial(4));
	ret -= (pow(x, 6)/factorial(6));
	return ret;
}
