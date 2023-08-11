//
// Created by degor on 11.08.2023.
//

#ifndef FRANCA2_WEB_MATHS3_H
#define FRANCA2_WEB_MATHS3_H

#include "maths2.h"

struct float3 {
    union {
        struct { float x, y, z; };
        struct { float2 xy; float _; };
        struct { float __; float2 yz; };
    };
};

#endif //FRANCA2_WEB_MATHS3_H
