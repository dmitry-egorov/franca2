// Created by degor on 10.08.2023.
#ifndef FRANCA2_ARRAYS_H
#define FRANCA2_ARRAYS_H

#include <cassert>
#include "primitives.h"
#include "syntax.h"

namespace arrays {
    tt struct arrayv {
        t*     data;
        size_t count;

              t& operator[](size_t index);
        const t& operator[](size_t index) const;
    };
    tt void enlarge (arrayv<t>& v);
    tt void advance (arrayv<t>& v);
    tt bool contains(const arrayv<t>& view, const t& item);
    tt arrayv<t> view_of(const std::initializer_list<t>& list);

    tt struct array {
        arrayv<t> data;
        size_t capacity;
    };

    tt void push(array<t>& s, const t& value);
    tt t    pop (array<t>& s);
    tt t    peek(const array<t>& s);
}
#endif //FRANCA2_ARRAYS_H

#ifdef FRANCA2_IMPLS
#ifndef FRANCA2_ARRAYS_I
#define FRANCA2_ARRAYS_I
namespace arrays {
    tt const t &arrayv<t>::operator[](const size_t index) const { return data[index]; }

    tt t &arrayv<t>::operator[](const size_t index) { return data[index]; }

    tt void enlarge   (arrayv<t>& v) { v.count += 1; }
    tt void enlarge_by(arrayv<t>& v, const size_t count) { v.count += count; }

    tt void advance(arrayv<t>& v) {
        assert(v.count > 0);
        v.count -= 1;
        v.data  += 1;
    }

    tt bool contains(const arrayv<t>& view, const t &item) {
        for (var i = 0u; i < view.count; i += 1) {
            if (view[i] == item) return true;
        }
        return false;
    }

    tt arrayv<t> view_of(const std::initializer_list<t>& list) {
        return {(t*)list.begin(), list.size()};
    }

    tt void push(array<t>& s, const t& value) {
        assert(s.data.count < s.capacity);
        s.data[s.data.count] = value;
        enlarge(s.data);
    }

    tt t pop(array<t>& s) {
        assert(s.data.count > 0);
        return s.data[--s.data.count];
    }

    tt t peek(const array<t>& s) {
        assert(s.count > 0);
        return s.data[s.data.count - 1];
    }
}
#endif
#endif //FRANCA2_IMPLS
