//
// Created by degor on 18.08.2023.
//

#ifndef FRANCA2_ARENA_ARRAYS_H
#define FRANCA2_ARENA_ARRAYS_H

#include <cstring>
#include "syntax.h"
#include "arrays.h"
#include "arenas.h"

tt struct arena_array {
    arrays::array_view<t> data;
    size_t capacity;
    arenas::arena* arena;
    uint align;
};

tt void ensure_capacity_for(arena_array<t>& s, const size_t extra_count) {
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



tt arena_array<t> make_arena_array(const size_t initial_capacity, arenas::arena& arena = arenas::gta, const uint align = sizeof(size_t)) {
    var arr = arena_array<t> { .arena = &arena, .align = align };
    ensure_capacity_for(arr, initial_capacity);
    return arr;
}

tt t& push(arena_array<t>& s, const t& value) {
    ensure_capacity_for(s, 1);
    s.data[s.data.count] = value;
    s.data.count += 1;
    return s.data[s.data.count - 1];
}

tt void push(arena_array<t>& s, const arrays::array_view<t>& value) {
    ensure_capacity_for(s, value.count);

    memcpy(s.data.data + s.data.count, value.data, value.count * sizeof(t));
    s.data.count += value.count;
}

tt void push(arena_array<t>& s, const std::initializer_list<t>& value) {
    push(s, arrays::array_view<t> {(t*)value.begin(), value.size()});
}

tt t pop(arena_array<t>& s) {
    assert(s.data.count > 0);
    return s.data[--s.data.count];
}

tt t peek(const arena_array<t>& s) {
    assert(s.data.count > 0);
    return s.data[s.data.count - 1];
}

tt void reset(arena_array<t>& s) {
    s.data.count = 0;
}

#endif //FRANCA2_ARENA_ARRAYS_H
