/*
 * math.h - Minimal math library for bare-metal ARM64 kernel
 */

#ifndef _MATH_H_
#define _MATH_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Math constants */
#define M_PI      3.14159265358979323846
#define M_E       2.71828182845904523536

/* Floating-point special values */
#define NAN       (__builtin_nan(""))
#define INFINITY  (__builtin_inf())
#define HUGE_VAL  (__builtin_inf())

/* Trigonometric functions */
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);


/* Exponential and logarithmic functions */
double exp(double x);
double log(double x);
double log10(double x);
double pow(double x, double y);
double sqrt(double x);

/* Rounding functions */
double ceil(double x);
double floor(double x);
double fabs(double x);
double fmod(double x, double y);
double modf(double x, double *intptr);
double fmin(double x, double y);
double fmax(double x, double y);

/* Other functions */
double hypot(double x, double y);
double frexp(double x, int *exp);
double ldexp(double x, int exp);

/* Floating-point classification functions */
int isnan(double x);

#ifdef __cplusplus
}
#endif

#endif /* _MATH_H_ */
