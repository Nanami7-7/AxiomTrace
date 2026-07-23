#ifndef HOST_BSP_MATHACL_H
#define HOST_BSP_MATHACL_H

#include <math.h>

static inline float mathacl_sqrtf(float x) { return sqrtf(x); }
static inline float mathacl_atan2f(float y, float x) { return atan2f(y, x); }
static inline float mathacl_asinf(float x) { return asinf(x); }
static inline void mathacl_sincosf(float x, float *s, float *c)
{
    *s = sinf(x);
    *c = cosf(x);
}

#endif
