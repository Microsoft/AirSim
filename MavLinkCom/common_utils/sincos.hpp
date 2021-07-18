// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef commn_utils_sincos_hpp
#define commn_utils_sincos_hpp

#include <cmath>

/*
GCC seem to define sincos already. However there is a bug currently which causes compiler to optimise
sequence of sin cos call in to sincos function which in above case would result in infinite recursion
hence we define above only for VC++ for now.
http://man7.org/linux/man-pages/man3/sincos.3.html
https://android.googlesource.com/platform/bionic/+/master/libm/sincos.c
*/

#ifdef _MSC_VER //GNU/Linux compilers have these functions
//inline here is forced and is hack. Ideally these definitions needs to be in cpp file. http://stackoverflow.com/a/6469881/207661
inline void sincos(double x, double* p_sin, double* p_cos)
{
    *p_sin = sin(x);
    *p_cos = cos(x);
}
inline void sincosf(float x, float* p_sinf, float* p_cosf)
{
    *p_sinf = sinf(x);
    *p_cosf = cosf(x);
}
inline void sincosl(long double x, long double* p_sinl, long double* p_cosl)
{
    *p_sinl = sinl(x);
    *p_cosl = cosl(x);
}
#endif

#endif
