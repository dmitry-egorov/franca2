//
// Created by degor on 11.08.2023.
//

#ifndef FRANCA2_WEB_WGSL_EX_H
#define FRANCA2_WEB_WGSL_EX_H
#include "maths4.h"
#include "transforms.h"

namespace wgsl_ex {
    struct gpu_transform2 {
        float4 r0, r1;
    };

    inline gpu_transform2 to_gpu_transform2(transforms::transform2 t) { return {
        .r0 = {.x = t.r0.x, .y = t.r0.y, .z = t.r0.z, .w = 0},
        .r1 = {.x = t.r1.x, .y = t.r1.y, .z = t.r1.z, .w = 0},
    };}
}

#endif //FRANCA2_WEB_WGSL_EX_H
