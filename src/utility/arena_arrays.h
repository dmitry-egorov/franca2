//
// Created by degor on 18.08.2023.
//

#ifndef FRANCA2_ARENA_ARRAYS_H
#define FRANCA2_ARENA_ARRAYS_H

#include "syntax.h"
#include "arrays.h"
#include "arenas.h"

tt struct arena_array {
    arrays::array_view<t> storage;
    size_t count;
};

tt void ensure_capacity_for(arena_array<t>& s, const size_t extra_count, arenas::arena& arena, const uint align = sizeof(size_t)) {
    let required_size = s.count + extra_count;
    chk(required_size > s.storage.count) else return;

    var new_size = s.storage.count == 0 ? 4 : s.storage.count * 2;
    while (new_size < required_size) {
        new_size *= 2;
    }

    var new_storage = alloc_g<t>(arena, new_size, align);

    memcpy(new_storage.data, s.storage.data, s.count * sizeof(t));
    memset(new_storage.data + s.count, 0, (new_size - s.count) * sizeof(t));

    s.storage = new_storage;
}

tt void push(arena_array<t>& s, const t& value, arenas::arena& arena, const uint align = sizeof(size_t)) {
    ensure_capacity_for(s, 1, arena, align);
    s.data[s.count] = value;
    s.count += 1;
}

tt void push(arena_array<t>& s, const arrays::array_view<t>& value, arenas::arena& arena, const uint align = sizeof(size_t)) {
    ensure_capacity_for(s, value.count, arena, align);

    memcpy(s.storage.data + s.count, value.data, value.count * sizeof(t));
    s.count += value.count;
}

tt t pop(arena_array<t>& s) {
    assert(s.count > 0);
    return s.data[--s.count];
}

tt t peek(const arena_array<t>& s) {
    assert(s.count > 0);
    return s.data[s.count - 1];
}

tt arrays::array_view<t> view_of(const arena_array<t>& s) {
    return { s.storage.data, s.count };
}


#endif //FRANCA2_ARENA_ARRAYS_H
