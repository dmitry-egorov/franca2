// Created by degor on 10.08.2023.
#ifndef FRANCA2_ARRAYS_H
#define FRANCA2_ARRAYS_H

#include <cassert>
#include "primitives.h"
#include "syntax.h"

namespace arrays {
    tt struct array_view {
        t*     data;
        size_t count;

              t& operator[](size_t index);
        const t& operator[](size_t index) const;
    };
    tt void enlarge (array_view<t>& v);
    tt void advance (array_view<t>& v);
    tt bool contains(const array_view<t>& view, const t& item);

    tt struct array {
        array_view<t> data;
        size_t count;
    };

    tt void push(array<t>& s, const t& value);
    tt t    pop (array<t>& s);
    tt t    peek(const array<t>& s);
    tt array_view<t> view_of(const array<t>& s);
}
#endif //FRANCA2_ARRAYS_H

#ifdef FRANCA2_ARRAYS_IMPL
#ifndef FRANCA2_ARRAYS_I
#define FRANCA2_ARRAYS_I
namespace arrays {
    tt const t &array_view<t>::operator[](const size_t index) const { return data[index]; }

    tt t &array_view<t>::operator[](const size_t index) { return data[index]; }

    tt void enlarge   (array_view<t>& v) { v.count += 1; }
    tt void enlarge_by(array_view<t>& v, const size_t count) { v.count += count; }

    tt void advance(array_view<t>& v) {
        assert(v.count > 0);
        v.count -= 1;
        v.data  += 1;
    }

    tt bool contains(const array_view<t>& view, const t &item) {
        for (var i = 0u; i < view.count; i += 1) {
            if (view[i] == item) return true;
        }
        return false;
    }

    tt void push(array<t>& s, const t& value) {
        assert(s.count < s.data.count);
        s.data[s.count++] = value;
    }

    tt t pop(array<t>& s) {
        assert(s.count > 0);
        return s.data[--s.count];
    }

    tt t peek(const array<t>& s) {
        assert(s.count > 0);
        return s.data[s.count - 1];
    }

    tt array_view<t> view_of(const array<t>& s) {
        return { s.data.data, s.count };
    }
}
#endif
#endif //FRANCA2_ARRAYS_IMPL
