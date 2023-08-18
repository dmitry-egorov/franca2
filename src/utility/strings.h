#ifndef FRANCA2_STRINGS_H
#define FRANCA2_STRINGS_H

#include <cstdio>
#include <cstring>
#include "syntax.h"
#include "arrays.h"

namespace strings {
    using namespace arrays;
    using namespace arenas;

    template<typename... Args> array_view<char> make_string(arena& arena, const char* format, Args... args) {
        let [data, free_count] = arenas::free_space_of<char>(arena, 1);
        let count  = snprintf(data, free_count - 1, format, args...) + 1;
        var result = arenas::alloc_g<char>(arena, count, 1);
        result.count -= 1; // remove the null terminator
        return result;
    }

    inline arrays::array_view<char> view_of(const char* s) {
        return {.data = (char*)s, .count = strlen(s)};
    }

    void print(const arrays::array_view<char>& s) {
        printf("%.*s", (int)s.count, s.data);
    }

}

#endif //FRANCA2_STRINGS_H
