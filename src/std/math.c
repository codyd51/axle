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

double arctan(double x) {
	double ret = x;
	ret -= (pow(x, 3)/3);
	ret += (pow(x, 5)/5);
	ret -= (pow(x, 7)/7);
	return ret;
}

double atan2(double y, double x) {
	if (x > 0) {
		return arctan(y/x);
	}
	else if (x < 0 && y >= 0) {
		return arctan(y/x) + pi;
	}
	else if (x < 0 && y < 0) {
		return arctan(y/x) - pi;
	}
	else if (x == 0 && y > 0) {
		return (pi/2);
	}
	else if (x == 0 && y < 0) {
		return -(pi/2);
	}
	
	//if x and y == 0, undefined
	return -1;
}

int abs(int val) {
	if (val < 0) return -val;
	return val;
}

double sqrt(double val) {
	//TODO handle this case
	if (val < 0) return -1;

	double a = 1;
	double b = val;
	double epsilon = 0.001;

	while (abs(a - b) > epsilon) {
		a = (a+b) / 2;
		b = val/a;
	}
	return a;
}

int round(double x) {
	if (x < 0.0) return (int)(x - 0.5);
	return (int)(x + 0.5);
}

static unsigned long int next = 1;
int rand() {
	next = next * 1103515245 + 12345;
	return (unsigned int)(next / 65536) % RAND_MAX;
}

void srand(unsigned int seed) {
	next = seed;
}
