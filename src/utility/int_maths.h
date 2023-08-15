#ifndef FRANCA2_WEB_INT_MATHS_H
#define FRANCA2_WEB_INT_MATHS_H

#include "primitives.h"

inline uint pow(uint value, uint exponent) {
    var result = 1u;
    for (var i = 0u; i < exponent; i++) result *= value;
    return result;
}

#endif //FRANCA2_WEB_INT_MATHS_H
