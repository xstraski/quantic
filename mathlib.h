// mathlib.h

#ifndef MATHLIB_H
#define MATHLIB_H

#include "common.h"
#include <math.h>

#define PI 3.14159265359f
#define PI_DIV_2 1.57079632679f
#define PI_MUL_2 6.28318530718f

#define EPS 0.0001f

#define square(v) ((v) * (v))
#define cube(v) ((v) * (v) * (v))

#define min(a, b) ((a) <= (b) ? (a) : (b))
#define max(a, b) ((a) >= (b) ? (a) : (b))

#define min3(a, b, c) min(min(a, b), c)
#define max3(a, b, c) max(max(a, b), c)

#define clamp(mn, mx, v) ((v) < (mn) ? (mn) : (v) > (mx) ? (mx) : (v))

// unsigned power
inline unsigned upow(unsigned base, unsigned exp)
{
	unsigned r = 1;

	while (exp) {
		if (exp % 2)
			r *= base;
		exp /= 2;
		base *= base;
	}

	return r;
}

// floating points compare
inline qboolean_t cmpfloats(float a, float b)
{
	return absf(a - b) <= EPS;
}
inline qboolean_t cmpdoubles(double a, double b)
{
	return absl(a - b) <= EPS;
}

#endif // #ifndef MATHLIB_H
