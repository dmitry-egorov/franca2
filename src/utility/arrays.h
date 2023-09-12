// Created by degor on 10.08.2023.
#ifndef FRANCA2_ARRAYS_H
#define FRANCA2_ARRAYS_H

#include <cassert>
#include "primitives.h"
#include "syntax.h"

template <typename t> using init_list = std::initializer_list<t>;

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
        union {
            arrays::arr_view<t> data;
            struct {
                t* bytes;
                size_t count;
            };
        };
        size_t capacity;
    };

    tt array<t> malloc_array(size_t capacity);

    tt t& push(array<t>& s, const t& value);
    tt t    pop (array<t>& s);
    tt t    peek(const array<t>& s);

    tt struct arr_ref {
        size_t offset;
    };

    tt t* ptr(arr_ref<t>, const arr_view<t>&);
    tt arr_ref<t> ref_in(const t*, const arr_view<t>&);

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
        if (count == 0) return {nullptr, 0};

        assert(count <= view.count);
        return {view.data, count};
    }

    tt arr_view<t> sub(const arr_view<t>& view, size_t offset, size_t count) {
        if (count == 0) return {nullptr, 0};

        assert(offset + count <= view.count);
        return {view.data + offset, count};
    }

    tt arr_view<t> sub_past_last(const arr_view<t>& view, const t& item) {
        var last = (size_t)0;
        for (var i = 0u; i < view.count; i += 1) {
            if (view[i] == item) last = i + 1;
        }
        return {view.data, last};
    }

    tt arr_view<t> view(std::initializer_list<t> list) {
        return {(t*)list.begin(), list.size()};
    }

    tt arr_view<t> view(t& item) {
        return {&item, 1};
    }


    tt array<t> malloc_array(size_t capacity) {
        return { { (t*)malloc(capacity * sizeof(t)), 0}, capacity};
    }

    tt t& push(array<t>& s, const t& value) {
        assert(s.count < s.capacity);
        enlarge(s.data);
        s.data[s.count - 1] = value;
        return s.data[s.count - 1];
    }

    tt t pop(array<t>& s) {
        assert(s.count > 0);
        return s.data[--s.count];
    }

    tt arr_view<t> pop(array<t>& s, size_t count) {
        assert(s.count >= count);
        s.count -= count;
        return {s.data.data + s.count, count};
    }

    tt t peek(const array<t>& s) {
        assert(s.count > 0);
        return s.data[s.count - 1];
    }

    tt t* ptr(arr_ref<t> ptr, const arr_view<t>& view) {
        if (ptr.offset < view.count); else return nullptr;
        return view.data + ptr.offset;
    }

    tt arr_ref<t> ref_in(const t* p, const arr_view<t>& v) {
        let offset = (size_t)p - (size_t)v.data;
        if (offset < v.count); else return {(size_t)-1};
        return {offset};
    }
}
#endif
#endif //FRANCA2_IMPLS
