#ifndef MATH_H
#define MATH_H

#define MIN(x, y) ({typeof(x) x_ = (x); typeof(y) y_ = (y); (x_ < y_) ? x_ : y_;})
#define MAX(x, y) ({typeof(x) x_ = (x); typeof(y) y_ = (y); (x_ > y_) ? x_ : y_;})

#define M_PI 3.1415926536
#define M_E 2.7182818285

double pow(double x, double pow);
unsigned long factorial(unsigned long x);

//trigonometric functions
double sin(double val);
double cos(double val);
double tan(double val);
double cot(double val);
double sec(double val);
double csc(double val);
double exp(double x);

//hyperbolic functions
double sinh(double val);
double cosh(double val);
double tanh(double val);
double coth(double val);
double sech(double val);
double csch(double val);

//inverse trigonometric functions
double arcsin(double val);
double arccos(double val);
double arctan(double val);
double arccot(double val);
double arcsec(double val);
double arccsc(double val);
double atan2(double y, double x);

int abs(int val);

double sqrt(double x);
int round(double x);

#define RAND_MAX 32767
int rand();
void srand(unsigned int seed);

#endif
