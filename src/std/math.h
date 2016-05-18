#ifndef MATH_H
#define MATH_H

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

double pow(double x, double pow);
unsigned long factorial(unsigned long x);

double sin(double val);
double arcsin(double val);
double cos(double val);
double arccos(double val);

int abs(int val);

double sqrt(double x);

#define RAND_MAX 32767
int rand();
void srand(unsigned int seed);

#endif
