//
// Created by degor on 18.08.2023.
//

#ifndef FRANCA2_ARENA_ARRAYS_H
#define FRANCA2_ARENA_ARRAYS_H

#include "syntax.h"
#include "arrays.h"
#include "arenas.h"

tt struct arena_array {
    arrays::array_view<t> data;
    size_t capacity;
};

tt void ensure_capacity_for(arena_array<t>& s, const size_t extra_count, arenas::arena& arena, const uint align = sizeof(size_t)) {
    let required_size = s.data.count + extra_count;
    if (required_size > s.capacity); else return;

    var new_size = s.capacity == 0 ? 4 : s.capacity * 2;
    while (new_size < required_size) {
        new_size *= 2;
    }

    var new_storage = alloc_g<t>(arena, new_size, align);

    memcpy(new_storage.data, s.data.data, s.data.count * sizeof(t));
    memset(new_storage.data + s.data.count, 0, (new_size - s.data.count) * sizeof(t));

    s.data.data = new_storage.data;
    s.capacity  = new_storage.count;
}

tt void push(arena_array<t>& s, const t& value, arenas::arena& arena, const uint align = sizeof(size_t)) {
    ensure_capacity_for(s, 1, arena, align);
    s.data[s.data.count] = value;
    s.data.count += 1;
}

tt void push(arena_array<t>& s, const arrays::array_view<t>& value, arenas::arena& arena, const uint align = sizeof(size_t)) {
    ensure_capacity_for(s, value.count, arena, align);

    memcpy(s.data.data + s.data.count, value.data, value.count * sizeof(t));
    s.data.count += value.count;
}

tt t pop(arena_array<t>& s) {
    assert(s.data.count > 0);
    return s.data[--s.data.count];
}

tt t peek(const arena_array<t>& s) {
    assert(s.data.count > 0);
    return s.data[s.data.count - 1];
}

#endif //FRANCA2_ARENA_ARRAYS_H
