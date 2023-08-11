//
// Created by degor on 11.08.2023.
//

#ifndef FRANCA2_WEB_WGSL_EX_H
#define FRANCA2_WEB_WGSL_EX_H
#include "maths4.h"
#include "transforms.h"

namespace wgsl_ex {
    struct transform2 {
        float4 r0, r1;
        //float2 c0, c1, c2, c3;
    };

    inline transform2 to_wgsl_transform2(transforms::transform2 t) {
        return {
            .r0 = {.x = t.r0.x, .y = t.r0.y, .z = t.r0.z, .w = 0},
            .r1 = {.x = t.r1.x, .y = t.r1.y, .z = t.r1.z, .w = 0},
        };
        /*return {
            .c0 = {.x = t.r0.x, .y = t.r1.x },
            .c1 = {.x = t.r0.y, .y = t.r1.y },
            .c2 = {.x = t.r0.z, .y = t.r1.z },
            .c3 = {.x =      0, .y =      0 },
        };*/
    }
}

#endif //FRANCA2_WEB_WGSL_EX_H
