#ifndef FRANCA2_PARSING_H
#define FRANCA2_PARSING_H

#include "arrays.h"
#include "strings.h"
#include "iterators.h"

namespace parsing {
    using namespace arrays;
    using namespace iterators;
    using namespace strings;

    static string whitespaces = view(" \t\n\r");

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

        if (is_empty(it_copy)); else return ret1_fail;

        it = it_copy;
        return ret1_ok(result);
    }

    ret1<int> take_int(string& it) {
        var it_copy = it;

        var negative = false;
        negative = take(it_copy, '-');
        if_var1(uint_value, take_uint(it_copy)); else return ret1_fail;
        if (uint_value <= INT_MAX); else return ret1_fail;

        let int_value = negative ? -(int)uint_value : (int)uint_value;
        it = it_copy;
        return ret1_ok(int_value);
    }

    ret1<float> take_float(string& it) {
        var it_copy = it;

        var negative = false;
        negative = take(it_copy, '-');

        if_var1(digit0, take_digit(it_copy)); else return ret1_fail;

        var uint_result = (uint)digit0;
        while(true) {
            if_var1(digit, take_digit(it_copy)); else break;
            uint_result = 10 * uint_result + digit;
        }

        var result = (float)uint_result;
        if (take(it_copy, '.')) {
            var fraction = 0.1f;
            while(true) {
                if_var1(digit, take_digit(it_copy)); else break;
                result += (float)digit * fraction;
                fraction /= 10;
            }
        }

        if (is_empty(it_copy)); else return ret1_fail;

        it = it_copy;
        return ret1_ok(negative ? -result : result);
    }

    ret1<string> take_str(string& it) {
        if (take(it, '\"')); else return ret1_fail;
        var data = take_until(it, '\"');
        if (take(it, '\"')); else return ret1_fail;
        return ret1_ok(data);
    }

}
#endif //FRANCA2_PARSING_H
