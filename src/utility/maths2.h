//
// Created by degor on 11.08.2023.
//

#ifndef FRANCA2_MATHS2_H
#define FRANCA2_MATHS2_H
#include "maths.h"
#include "syntax.h"
#include "primitives.h"

struct float2 { float x, y; };

constexpr float2 zero2f = { 0, 0 };
inline float2 saturate(const float2& v) { return { saturate(v.x), saturate(v.y) }; }

inline float2 flip_y(const float2& v) { return {v.x, -v.y}; }

inline float2 operator-(const float2& v1) { return { -v1.x, -v1.y }; }

inline float2 operator+(const float2& v, const float s) { return { .x = v.x + s, .y = v.y + s }; }
inline float2 operator-(const float2& v, const float s) { return { .x = v.x - s, .y = v.y - s }; }

inline float2 operator+(const float2& v1, const float2& v2) { return { .x = v1.x + v2.x, .y = v1.y + v2.y }; }
inline float2 operator-(const float2& v1, const float2& v2) { return { .x = v1.x - v2.x, .y = v1.y - v2.y }; }
inline float2 operator*(const float2& v1, const float2& v2) { return { .x = v1.x * v2.x, .y = v1.y * v2.y }; }
inline float2 operator/(const float2& v1, const float2& v2) { return { .x = v1.x / v2.x, .y = v1.y / v2.y }; }

inline void operator+=(float2& v1, const float2& v2) { v1 = v1 + v2; }
inline void operator-=(float2& v1, const float2& v2) { v1 = v1 - v2; }
inline void operator*=(float2& v1, const float2& v2) { v1 = v1 * v2; }
inline void operator/=(float2& v1, const float2& v2) { v1 = v1 / v2; }

inline float2 operator*(const float2& v, const float s) { return { .x = v.x * s, .y = v.y * s }; }
inline float2 operator/(const float2& v, const float s) { return { .x = v.x / s, .y = v.y / s }; }
inline float2 operator*(const float s, const float2& v) { return { .x = s * v.x, .y = s * v.y }; }
inline float2 operator/(const float s, const float2& v) { return { .x = s / v.x, .y = s / v.y }; }

//inline float2 operator*(const float2& v1, const uint2& v2) { return { .x = v1.x * v2.x, .y = v1.y * v2.y }; }

inline float  len_sq       (const float2& v             ) { return v.x * v.x + v.y * v.y; }
inline float  len          (const float2& v             ) { return sqrt(len_sq(v)); }
inline float  length       (const float2& v             ) { return len(v); }
inline float2 normalized   (const float2& v             ) { return v / len(v); }
inline float2 normalized_to(const float2& v, const float c) { return v * (c / len(v)); }

inline float  delta    (const float  & from, const float  & to) { return to - from; }
inline float2 delta    (const float2& from, const float2& to) { return to - from; }
inline float2 direction(const float2& from, const float2& to) { return normalized(delta(from, to)); }

inline float distance(const float2& from, const float2& to) { return len(delta(from, to)); }

inline float dot(const float2& v1, const float2& v2) { return v1.x * v2.x + v1.y * v2.y; }

// rotate in the direction of the coordinate system, e.g. clockwise in the screen space
inline float2 orth_cw (const float2& v) { return { -v.y, v.x }; }
inline float2 orth_ccw(const float2& v) { return { v.y, -v.x }; }

inline float2 lerp (const float2& v0, const float2& v1, const float t) { return v0 * (1.0f - t) + v1 * t; }
inline float2 lerp3(const float2& v0, const float2& v1, const float2& v2, const float2& v3, const float t) {
    let t3 = t * t * t;
    let t2 = t * t;
    return
            v0 * (-    t3 + 3 * t2 - 3 * t + 1) +
            v1 * ( 3 * t3 - 6 * t2 + 3 * t) +
            v2 * (-3 * t3 + 3 * t2) +
            v3 * (t3)
            ;
    /*
    LET o_t  = 1.0f - t;
    LET o_t3 = o_t * o_t * o_t;
    LET o_t2 = o_t * o_t;

    return v0 * o_t3
         + v1 * 3 * t  * o_t2
         + v2 * 3 * t2 * o_t
         + v3 * t3;
    */
    //  P(t)      =  (1-t)^3 * P0 + 3t(1-t)^2 * P1 + 3t^2 (1-t) * P2 + t^3 * P3
    // dP(t) / dt =  -3(1-t)^2 * P0 + 3 (1-t)^2 * P1 - 6t(1-t) * P1 - 3t^2 * P2 + 6t(1-t) * P2 + 3t^2 * P3
}

struct fsize2 {
    union {
        struct {
            union { float x, w, width ; };
            union { float y, h, height; };
        };
        float2 v;
    };
};


struct uint2 { uint x, y; };

struct usize2 {
    union {
        struct {
            union { uint x, w, width ; };
            union { uint y, h, height; };
        };
        uint2 v;
    };
};


inline fsize2 operator*(const usize2& v, const float& s) {
    return {.x = (float)v.w * s, .y = (float)v.h * s };
}

inline usize2 to_usize2(const fsize2 s) {
    return {.w = (uint)s.w, .h = (uint)s.h };
}

inline float2 operator/(const fsize2& vf, const usize2& vu) {
    return {.x = vf.x / (float)vu.w, .y = vf.y / (float)vu.y };
}

inline float aspect_ratio_of(const usize2 s) {
    return (float)s.w / (float)s.h;
}

#endif //FRANCA2_MATHS2_H
