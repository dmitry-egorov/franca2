//
// Created by degor on 11.08.2023.
//

#ifndef FRANCA2_TRIGS_H
#define FRANCA2_TRIGS_H

struct turns {
    union { float value, v; };
};

inline turns operator-(const turns& t) { return { -t.v}; }
inline turns operator+(const turns& t0, const turns& t1) { return { t0.v + t1.v }; }
inline turns operator-(const turns& t0, const turns& t1) { return { t0.v - t1.v }; }

inline turns operator+=(turns& t0, const turns& t1) { return t0 = t0 + t1; }
inline turns operator-=(turns& t0, const turns& t1) { return t0 = t0 - t1; }

#endif //FRANCA2_WEB_TRIGS_H
