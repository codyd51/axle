#ifndef MATH_H
#define MATH_H

#define MIN(x, y) ({typeof(x) x_ = (x); typeof(y) y_ = (y); (x_ < y_) ? x_ : y_;})
#define MAX(x, y) ({typeof(x) x_ = (x); typeof(y) y_ = (y); (x_ > y_) ? x_ : y_;})

double pow(double x, double pow);
unsigned long factorial(unsigned long x);

double sin(double val);
double arcsin(double val);
double cos(double val);
double arccos(double val);
double arctan(double val);
double atan2(double y, double x);

int abs(int val);

double sqrt(double x);
int round(double x);

#define RAND_MAX 32767
int rand();
void srand(unsigned int seed);

#endif
