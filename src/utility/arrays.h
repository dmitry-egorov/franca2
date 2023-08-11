// Created by degor on 10.08.2023.
#ifndef FRANCA2_WEB_ARRAYS_H
#define FRANCA2_WEB_ARRAYS_H

#include "primitives.h"
#include "syntax.h"
namespace arrays {
    tt struct array_view {
        t*     data;
        size_t count;

        t& operator[](const size_t index) { return data[index]; }
    };
}

#endif //FRANCA2_WEB_ARRAYS_H
