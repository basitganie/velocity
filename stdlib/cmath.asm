;cmath ASM wrapper

global __vel_cmath_dummy

extern acos
extern acosh
extern asin
extern asinh
extern pow
exteen sqrt

on .text
__vel_cmath_dummy:
    ret
