//
// Created by degor on 11.08.2023.
//

#ifndef FRANCA2_WEB_TRANSFORMS_H
#define FRANCA2_WEB_TRANSFORMS_H
#include <cmath>
#include "trigs.h"
#include "matrices.h"

namespace transforms {
    using namespace matrices;

    struct transform2 {
        union {
            mat3x2f matrix, mat, m;
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

    [[nodiscard]] inline transform2 identity() { return {
        .r0 = { .x = 1, .y = 0, .z = 0 },
        .r1 = { .x = 0, .y = 1, .z = 0 },
    };}

    [[nodiscard]] inline transform2 operator>>(const transform2& t0, const transform2& t1) { return {.m = t1.m * t0.m}; }
    inline transform2 operator>>=(transform2& t0, const transform2& t1) { return t0 = t0 >> t1; }

    [[nodiscard]] inline transform2 uv_to_clip() { return {
        .r0 = { .x = 2, .y =  0, .z = -1 },
        .r1 = { .x = 0, .y = -2, .z =  1 },
    };}

    [[nodiscard]] inline transform2 clip_to_uv() { return {
        .r0 = { .x = 0.5f, .y =   0  , .z = 0.5f },
        .r1 = { .x =  0  , .y = -0.5f, .z = 0.5f },
    };}

    [[nodiscard]] inline transform2 translation(const float2& offset) { return {
        .r0 = { .x = 1, .y = 0, .z = offset.x },
        .r1 = { .x = 0, .y = 1, .z = offset.y },
    };}

    [[nodiscard]] inline transform2 translation(const float offset) { return {
        .r0 = { .x = 1, .y = 0, .z = offset },
        .r1 = { .x = 0, .y = 1, .z = offset },
    };}

    [[nodiscard]] inline transform2 rotation(const turns angle) {
        let s = sinf(2.0f * static_cast<float>(M_PI) * angle.v);
        let c = cosf(2.0f * static_cast<float>(M_PI) * angle.v);

        return {
            .r0 = { .x = c, .y = -s, .z = 0 },
            .r1 = { .x = s, .y =  c, .z = 0 },
        };
    }

    [[nodiscard]] inline transform2 rotation(const turns angle, const float2 center) { return translation(-center) >> rotation(angle) >> translation(center); }
    [[nodiscard]] inline transform2 rotation(const turns angle, const float  center) { return translation(-center) >> rotation(angle) >> translation(center); }

    [[nodiscard]] inline transform2 scaling(const float2& scale) { return {
        .r0 = { .x = scale.x, .y = 0      , .z = 0 },
        .r1 = { .x = 0      , .y = scale.y, .z = 0 }
    };}

    [[nodiscard]] inline transform2 scaling(const float scale) { return {
        .r0 = { .x = scale, .y = 0    , .z = 0 },
        .r1 = { .x = 0    , .y = scale, .z = 0 }
    };}

    [[nodiscard]] inline transform2 scaling(const float2& scale, const float2& center) { return translation(-center) >> scaling(scale) >> translation(center); }
    [[nodiscard]] inline transform2 scaling(const float   scale, const float2& center) { return translation(-center) >> scaling(scale) >> translation(center); }
    [[nodiscard]] inline transform2 scaling(const float2& scale, const float   center) { return translation(-center) >> scaling(scale) >> translation(center); }
    [[nodiscard]] inline transform2 scaling(const float   scale, const float   center) { return translation(-center) >> scaling(scale) >> translation(center); }

    [[nodiscard]] inline transform2 x_flip() { return {
        .r0 = { .x = -1, .y = 0, .z = 0 },
        .r1 = { .x =  0, .y = 0, .z = 0 }
    };}

    [[nodiscard]] inline transform2 y_flip() { return {
        .r0 = { .x = 1, .y =  0, .z = 0 },
        .r1 = { .x = 0, .y = -1, .z = 0 }
    };}

    // Note: uniform uv is a space [-ar, ar] x [0, 1] where ar is aspect ratio

    [[nodiscard]] inline transform2 uniform_uv_to_uv(const float aspect_ratio) { return scaling({ 1 / aspect_ratio, 1}, 0.5f); }
    [[nodiscard]] inline transform2 uv_to_uniform_uv(const float aspect_ratio) { return scaling({     aspect_ratio, 1}, 0.5f); }

    [[nodiscard]] inline transform2 uniform_clip_to_clip(const float aspect_ratio) { return scaling({ 1 / aspect_ratio, 1}); }
    [[nodiscard]] inline transform2 clip_to_uniform_clip(const float aspect_ratio) { return scaling({     aspect_ratio, 1}); }

    [[nodiscard]] inline transform2 uniform_uv_to_clip(const float aspect_ratio) { return uniform_uv_to_uv(aspect_ratio) >> uv_to_clip(); }
    [[nodiscard]] inline transform2 clip_to_uniform_uv(const float aspect_ratio) { return clip_to_uv() >> uv_to_uniform_uv(aspect_ratio); }

    [[nodiscard]] inline transform2 rect_to_uv(const fsize2& size) { return scaling(1 / size.v); }
    [[nodiscard]] inline transform2 uv_to_rect(const fsize2& size) { return scaling(    size.v); }

    [[nodiscard]] inline transform2 rect_to_uniform_uv(const fsize2& size) { return rect_to_uv(size) >> uv_to_uniform_uv(size.w / size.h); }
    [[nodiscard]] inline transform2 uniform_uv_to_rect(const fsize2& size) { return uniform_uv_to_uv(size.w / size.h) >> uv_to_rect(size); }

    [[nodiscard]] inline float2 apply(const transform2& t, const float3& v) { return {
        t._00 * v.x + t._01 * v.y + t._02 * v.z,
        t._10 * v.x + t._11 * v.y + t._12 * v.z,
    };}

    [[nodiscard]] inline float2 apply_to_point(const transform2& t, const float2& p) { return {
        t._00 * p.x + t._01 * p.y + t._02,
        t._10 * p.x + t._11 * p.y + t._12,
    };}

    [[nodiscard]] inline float2 apply_to_vector(const transform2& t, const float2& v) { return {
        t._00 * v.x + t._01 * v.y,
        t._10 * v.x + t._11 * v.y,
    };}
}

#endif //FRANCA2_WEB_TRANSFORMS_H
