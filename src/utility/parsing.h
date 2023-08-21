#ifndef FRANCA2_PARSING_H
#define FRANCA2_PARSING_H

#include "arrays.h"
#include "strings.h"
#include "iterators.h"

namespace parsing {
    using namespace arrays;
    using namespace iterators;
    using namespace strings;

    void skip_whitespaces(array_view<char>& it) {
        static let whitespaces = view_of(" \t\n\r");
        skip_while_any(it, whitespaces);
    }

    array_view<char> take_whitespaces(array_view<char>& it) {
        static let whitespaces = view_of(" \t\n\r");
        return take_while_any(it, whitespaces);
    }

    ret1<uint8_t> take_digit(array_view<char>& it) {
        if_var1(c, peek(it)); else return ret1_fail;

        var digit = c - '0';
        if(digit >= 0 && digit <= 9); else return ret1_fail;

        take(it);

        return ret1_ok((uint8_t)digit);
    }

    ret1<uint> take_integer(array_view<char>& it) {
        var it_copy = it;
        if_var1(digit0, take_digit(it_copy)); else return ret1_fail;

        var result = (uint)digit0;
        while(true) {
            if_var1(digit, take_digit(it_copy)); else break;
            result = 10 * result + digit;
        }

        it = it_copy;
        return ret1_ok(result);
    }
}
#endif //FRANCA2_PARSING_H
