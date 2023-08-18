//
// Created by degor on 11.08.2023.
//

#ifndef FRANCA2_MATHS3_H
#define FRANCA2_MATHS3_H

#include "maths2.h"

struct float3 {
    union {
        struct { float  x, y, z; };
        struct { float2 xy; float  _z; };
        struct { float  _x; float2 yz; };
    };
};
struct uint3 {
    union {
        struct { uint  x, y, z; };
        struct { uint2 xy; uint  _z; };
        struct { uint  _x; uint2 yz; };
    };
};

#endif //FRANCA2_WEB_MATHS3_H
