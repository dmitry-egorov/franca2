#ifndef FRANCA2_PARSING_H
#define FRANCA2_PARSING_H

#include "arrays.h"
#include "strings.h"
#include "iterators.h"

namespace parsing {
    using namespace arrays;
    using namespace iterators;
    using namespace strings;

    static string whitespaces = view_of(" \t\n\r");

    void skip_whitespaces(string& it) {
        skip_while_any(it, whitespaces);
    }

    string take_whitespaces(string& it) {
        return take_while_any(it, whitespaces);
    }

    ret1<uint8_t> take_digit(string& it) {
        if_var1(c, peek(it)); else return ret1_fail;

        var digit = c - '0';
        if(digit >= 0 && digit <= 9); else return ret1_fail;

        take(it);

        return ret1_ok((uint8_t)digit);
    }

    ret1<uint> take_uint(string& it) {
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

    ret1<string> take_str(string& it) {
        if (take(it, '\"')); else return ret1_fail;
        var data = take_until(it, '\"');
        if (take(it, '\"')); else return ret1_fail;
        return ret1_ok(data);
    }

}
#endif //FRANCA2_PARSING_H
