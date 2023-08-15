#ifndef FRANCA2_WEB_STRINGS_H
#define FRANCA2_WEB_STRINGS_H

#include <cstring>
#include "syntax.h"
#include "arrays.h"

inline arrays::array_view<char> view_of(const char* s) {
    return {.data = (char*)s, .count = strlen(s)};
}

#endif //FRANCA2_WEB_STRINGS_H
