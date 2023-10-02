//
// Created by degor on 18.08.2023.
//

#ifndef FRANCA2_ARENA_ARRAYS_H
#define FRANCA2_ARENA_ARRAYS_H

#include <cstring>
#include "syntax.h"
#include "arrays.h"
#include "arenas.h"

namespace arrays {
    // dynamic size array, that uses an arena for allocation
    tt struct arr_dyn {
        union {
            arrays::arr_view<t> data;
            struct {
                t* bytes;
                size_t count;
            };
        };
        size_t capacity;
        arenas::arena* arena;
        uint align;

              t& operator[](size_t index);
        const t& operator[](size_t index) const;
    };

    typedef arr_dyn<u8> stream;

    tt void ensure_capacity_for(arr_dyn<t>& s, const size_t extra_count) {
        if_ref(arena, s.arena); else { dbg_fail_return; }

        let required_count = s.count + extra_count;
        if (required_count > s.capacity); else return;

        var new_capacity = (size_t)((s.capacity == 0) ? (size_t)4 : (s.capacity * (size_t)2u));
        while (new_capacity < required_count)
            new_capacity *= 2;

        var new_storage = alloc_g<t>(arena, new_capacity, s.align);

        memcpy(new_storage.data, s.bytes, s.count * sizeof(t));
        memset(new_storage.data + s.count, 0, (new_capacity - s.count) * sizeof(t));

        s.bytes = new_storage.data;
        s.capacity  = new_storage.count;
    }

    tt arr_dyn<t> make_arr_dyn(const size_t initial_capacity, arenas::arena& arena = arenas::gta, const uint align = sizeof(size_t)) {
        var arr = arr_dyn<t> { .arena = &arena, .align = align };
        ensure_capacity_for(arr, initial_capacity);
        return arr;
    }

    inline stream make_stream(const size_t initial_capacity, arenas::arena& arena = arenas::gta, const uint align = sizeof(size_t)) {
        return make_arr_dyn<u8>(initial_capacity, arena, align);
    }

    tt t& push(arr_dyn<t>& s, const t& value) {
        ensure_capacity_for(s, 1);
        s.count += 1;
        ref r = s.data[s.count - 1];
        s.data[s.count - 1] = value;
        return r;
    }

    tt void push(arr_dyn<t>& s, const arrays::arr_view<t>& values) {
        ensure_capacity_for(s, values.count);

        memcpy(s.bytes + s.count, values.data, values.count * sizeof(t));
        s.count += values.count;
    }

    tt void push(arr_dyn<t>& s, const init_list<t>& value) {
        push(s, arrays::arr_view<t> {(t*)value.begin(), value.size()});
    }

    tt t pop(arr_dyn<t>& s) {
        assert(s.count > 0);
        let r = s.data[s.count - 1];
        s.count -= 1;
        return r;
    }

    tt arr_view<t> pop(arr_dyn<t>& s, size_t count) {
        assert(s.count >= count);
        s.count -= count;
        return {s.bytes + s.count, count};
    }

    tt t peek(const arr_dyn<t>& s) {
        assert(s.count > 0);
        return s.data[s.count - 1];
    }

    tt void clear(arr_dyn<t>& s) {
        s.count = 0;
    }

    tt t* last_of(arr_dyn<t>& s) {
        if (s.count > 0); else return nullptr;
        return &s.data[s.count - 1];
    }

    tt arr_view<t> last_of(arr_dyn<t>& s, size_t count) {
        if (s.count >= count); else return {nullptr, 0};
        return {s.bytes + s.count - count, count};
    }

    tt size_t count_of(const arr_dyn<t>& s) {
        return s.count;
    }

    tt const t& arr_dyn<t>::operator[](const size_t index) const { assert(index < data.count); return data[index]; }
    tt       t& arr_dyn<t>::operator[](const size_t index)       { assert(index < data.count); return data[index]; }

    tt t* ptr(arr_ref<t> p, const arr_dyn<t>& a) {
        if (p.offset < a.count); else return nullptr;
        return a.bytes + p.offset;
    }

    tt arr_ref<t> last_ref_of(arr_dyn<t>& s) {
        if (s.count > 0); else return {(size_t)-1};
        return {s.count - 1};
    }

    tt bool equals(const arr_dyn<t>& a0, const arr_dyn<t>& a1) {
        if (a0.count == a1.count); else return false;
        return memcmp(a0.bytes, a1.bytes, a0.count * sizeof(t)) == 0;
    }

}

#endif //FRANCA2_ARENA_ARRAYS_H
