//
// Created by degor on 11.08.2023.
//

#ifndef FRANCA2_WEB_MATRICES_H
#define FRANCA2_WEB_MATRICES_H

#include "maths3.h"

namespace matrices {
    struct mat3x2f {
        union {
            struct {
                float _00, _01, _02;
                float _10, _11, _12;
            };

            struct {
                float3 r0, r1;
            };

            float  cells[2][3], _[2][3];
            float3 rows [2]   , r[2];
        };
    };


    [[nodiscard]] inline mat3x2f operator*(const mat3x2f& t0, const mat3x2f& t1) { return {
        // |t0._00, t0._01, t0._02|   |t1._00, t1._01, t1._02|
        // |t0._10, t0._11, t0._12| x |t1._10, t1._11, t1._12|
        // |  0   ,   0   ,   1   |   |  0   ,   0   ,   1   |
        // multiply rows of t0 by columns of t1
        .r0 = {
            .x = t0._00 * t1._00 + t0._01 * t1._10, // 0
            .y = t0._00 * t1._01 + t0._01 * t1._11, // 0
            .z = t0._00 * t1._02 + t0._01 * t1._12 + t0._02
        },
        .r1 = {
            .x = t0._10 * t1._00 + t0._11 * t1._10, // 0
            .y = t0._10 * t1._01 + t0._11 * t1._11, // 0
            .z = t0._10 * t1._02 + t0._11 * t1._12 + t0._12
        }
            //   0  0  1
    };}
}

#endif //FRANCA2_WEB_MATRICES_H
