//
// Created by degor on 11.08.2023.
//

#ifndef FRANCA2_WEB_MATHS_H
#define FRANCA2_WEB_MATHS_H
#include <cmath>

inline float saturate(const float v) { return fmax(fmin(v, 1.0f), 0.0f); }

#endif //FRANCA2_WEB_MATHS_H
