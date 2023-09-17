#ifndef FRANCA2_INT_MATHS_H
#define FRANCA2_INT_MATHS_H

#include "syntax.h"
#include "primitives.h"

inline uint pow(uint value, uint exponent) {
    var result = 1u;
    for_rng(0u, exponent) result *= value;
    return result;
}

#endif //FRANCA2_INT_MATHS_H
