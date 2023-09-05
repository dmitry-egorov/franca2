// Created by degor on 10.08.2023.
#ifndef FRANCA2_ARRAYS_H
#define FRANCA2_ARRAYS_H

#include <cassert>
#include "primitives.h"
#include "syntax.h"

namespace arrays {
    tt struct arr_view {
        t*     data;
        size_t count;

              t& operator[](size_t index);
        const t& operator[](size_t index) const;
    };
    tt void enlarge (arr_view<t>& v);
    tt void advance (arr_view<t>& v);
    tt bool contains(const arr_view<t>& view, const t& item);
    tt size_t count(const arr_view<t>& view, const t& item);
    tt arr_view<t> sub(const arr_view<t>& view, size_t count);
    tt arr_view<t> sub_past_last(const arr_view<t>& view, const t& item);


    tt struct array {
        arr_view<t> data;
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
    tt const t& arr_view<t>::operator[](const size_t index) const { assert(index < count); return data[index]; }
    tt t& arr_view<t>::operator[](const size_t index) { assert(index < count); return data[index]; }

    tt void enlarge   (arr_view<t>& v) { v.count += 1; }
    tt void enlarge_by(arr_view<t>& v, const size_t count) { v.count += count; }

    tt void advance(arr_view<t>& v) {
        assert(v.count >= 1);
        v.count -= 1;
        v.data  += 1;
    }

    tt void advance(arr_view<t>& v, size_t count) {
        assert(v.count >= count);
        v.count -= count;
        v.data  += count;
    }

    tt bool contains(const arr_view<t>& view, const t &item) {
        for (var i = 0u; i < view.count; i += 1) {
            if (view[i] == item) return true;
        }
        return false;
    }

    tt size_t count(const arr_view<t>& view, const t& item) {
        var count = (size_t)0;
        for (var i = 0u; i < view.count; i += 1) {
            if (view[i] == item) count++;
        }
        return count;
    }
    tt arr_view<t> sub(const arr_view<t>& view, size_t count) {
        assert(count <= view.count);
        return {view.data, count};
    }

    tt arr_view<t> sub_past_last(const arr_view<t>& view, const t& item) {
        var last = (size_t)0;
        for (var i = 0u; i < view.count; i += 1) {
            if (view[i] == item) last = i + 1;
        }
        return {view.data, last};
    }

    tt arr_view<t> view(const std::initializer_list<t>& list) {
        return {(t*)list.begin(), list.size()};
    }

    tt arr_view<t> view(t& item) {
        return {&item, 1};
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
