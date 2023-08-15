//
// Created by degor on 16.08.2023.
//

#ifndef FRANCA2_WEB_ITERATORS_H
#define FRANCA2_WEB_ITERATORS_H

#include "syntax.h"
#include "results.h"
#include "arrays.h"
#include "arenas.h"

namespace iterators {
    using namespace arenas;
    using namespace arrays;

    tt struct iterator {
        array_view<t> view;
    };

    tt iterator<t> iterate(const array_view<t>& v) { return {.view = v}; }

    tt bool finished(const iterator<t>& it) { return it.view.count == 0; }

    tt ret1<t> peek(iterator<t>& it) {
        chk (it.view.count != 0) else return ret1_fail;
        return ret1_ok(it.view[0]);
    }

    tt ret1<t> take(iterator<t>& it) {
        chk(it.view.count != 0) else return ret1_fail;

        let result = it.view[0];

        advance(it.view);

        return ret1_ok(result);
    }

    tt bool take_until(iterator<t>& it, t target) {
        while (true) {
            chk_1(c, peek(it)) else return false;
            if (c == target) return true;
            take(it);
        }
    }

    ret1<uint8_t> take_digit(iterator<char>& it) {
        chk_1(c, peek(it)) else return ret1_fail;

        var digit = c - '0';
        chk(digit >= 0 && digit <= 9) else return ret1_fail;

        take(it);

        return ret1_ok((uint8_t)digit);
    }
}

#endif //FRANCA2_WEB_ITERATORS_H
