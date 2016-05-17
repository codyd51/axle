#include "math.h"

//todo replace this with proper define
double pi = 3.1415926536;

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

double arcsin(double x) {
	//taylor series for arcsin
	double ret = x;
	ret += (pow(x, 3))/6;
	ret += (3*pow(x, 5))/40;
	ret += (5*pow(x, 7))/112;
	return ret;
}

double cos(double x) {
	//approximate taylor series for cos
	double ret = 1;
	ret -= (pow(x, 2)/factorial(2));
	ret += (pow(x, 4)/factorial(4));
	ret -= (pow(x, 6)/factorial(6));
	return ret;
}

double arccos(double x) {
	//arccos is arcsin phase shifted pi/2
	return (pi/2) - arcsin(x);
}
