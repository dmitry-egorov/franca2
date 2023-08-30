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
    tt struct arrayd {
        arrays::arrayv<t> data;
        size_t capacity;
        arenas::arena* arena;
        uint align;
    };

    typedef arrayd<u8> stream;

    tt void ensure_capacity_for(arrayd<t>& s, const size_t extra_count) {
        if_ref(arena, s.arena); else { dbg_fail_return; }

        let required_size = s.data.count + extra_count;
        if (required_size > s.capacity); else return;

        var new_size = s.capacity == 0 ? (size_t)4 : s.capacity * 2;
        while (new_size < required_size) {
            new_size *= 2;
        }

        var new_storage = alloc_g<t>(arena, new_size, s.align);

        memcpy(new_storage.data, s.data.data, s.data.count * sizeof(t));
        memset(new_storage.data + s.data.count, 0, (new_size - s.data.count) * sizeof(t));

        s.data.data = new_storage.data;
        s.capacity  = new_storage.count;
    }


    tt arrayd<t> make_darray(const size_t initial_capacity, arenas::arena& arena = arenas::gta, const uint align = sizeof(size_t)) {
        var arr = arrayd<t> { .arena = &arena, .align = align };
        ensure_capacity_for(arr, initial_capacity);
        return arr;
    }

    inline stream make_stream(const size_t initial_capacity, arenas::arena& arena = arenas::gta, const uint align = sizeof(size_t)) {
        return make_darray<u8>(initial_capacity, arena, align);
    }

    tt t& push(arrayd<t>& s, const t& value) {
        ensure_capacity_for(s, 1);
        s.data[s.data.count] = value;
        s.data.count += 1;
        return s.data[s.data.count - 1];
    }

    tt void push(arrayd<t>& s, const arrays::arrayv<t>& value) {
        ensure_capacity_for(s, value.count);

        memcpy(s.data.data + s.data.count, value.data, value.count * sizeof(t));
        s.data.count += value.count;
    }

    tt void push(arrayd<t>& s, const std::initializer_list<t>& value) {
        push(s, arrays::arrayv<t> {(t*)value.begin(), value.size()});
    }

    tt t pop(arrayd<t>& s) {
        assert(s.data.count > 0);
        return s.data[--s.data.count];
    }

    tt t peek(const arrayd<t>& s) {
        assert(s.data.count > 0);
        return s.data[s.data.count - 1];
    }

    tt void reset(arrayd<t>& s) {
        s.data.count = 0;
    }

    tt t* last_of(arrayd<t>& s) {
        if (s.data.count > 0); else return nullptr;
        return &s.data[s.data.count - 1];
    }
}

#endif //FRANCA2_ARENA_ARRAYS_H
