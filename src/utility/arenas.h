//
// Created by degor on 09.08.2023.
//

#ifndef FRANCA2_WEB_ARENAS_H
#define FRANCA2_WEB_ARENAS_H

#include <cstdlib>
#include <cassert>
#include <initializer_list>
#include "syntax.h"
#include "arrays.h"

namespace arenas {
    using namespace std;
    using namespace arrays;

    struct arena {
        void*  memory;
        size_t size_bytes;
        size_t used_bytes;
    } global_temp_arena;

    inline arena make(const size_t capacity) { return { malloc(capacity), capacity, 0 }; }

    inline void init(arena& arena, const size_t capacity) { arena = make(capacity); }

    inline void release(arena& arena) {
        free(arena.memory);
        arena.memory     = nullptr;
        arena.size_bytes = 0;
        arena.used_bytes = 0;
    }

    inline void reset(arena& arena) { arena.used_bytes = 0; }

    tt array_view<t> free_space_of(arena& arena) {
        return {(t*)((char*)arena.memory + arena.used_bytes), ((arena.size_bytes - arena.used_bytes) / sizeof(t))};
    }

    inline void* next(const arena& arena) {
        return (char*)arena.memory + arena.used_bytes;
    }

    inline size_t remaining_bytes(const arena& arena) {
        return arena.size_bytes - arena.used_bytes;
    }

    inline void* alloc(arena& arena, const size_t size_bytes) {
        assert(arena.used_bytes + size_bytes <= arena.size_bytes);
        void* result = (char*)arena.memory + arena.used_bytes;
        arena.used_bytes += size_bytes;
        return result;
    }

    tt array_view<t> alloc(arena& arena, const size_t count) {
        return {(t*)alloc(arena, count * sizeof(t)), count};
    }

    tt array_view<t> alloc(arena& arena, const initializer_list<t> list) {
        let count = list.size();
        chk(count > 0) else return {0, 0};

        var data = alloc<t>(arena, count);

        var i = 0;
        for(let e: list) {
            data[i] = e;
            i++;
        }

        return data;
    }

    tt t* alloc_one(arena& arena, const t& item) {
        return alloc<t>(arena, {item}).data;
    }

    // gta_ -> global temp arena
    inline void gta_init   (const size_t capacity_bytes) { init   (global_temp_arena, capacity_bytes); }
    inline void gta_release(                           ) { release(global_temp_arena); }
    inline void gta_reset  (                           ) { reset  (global_temp_arena); }

    inline void*     gta_alloc(const size_t size_bytes       ) { return alloc   (global_temp_arena, size_bytes );}
    tt array_view<t> gta_alloc(const size_t count            ) { return alloc<t>(global_temp_arena, count); }
    tt array_view<t> gta_alloc(const initializer_list<t> list) { return alloc<t>(global_temp_arena, list ); }

    tt t* gta_one(const t& item) { return alloc_one<t>(global_temp_arena, item); }

}
#endif //FRANCA2_WEB_ARENAS_H
