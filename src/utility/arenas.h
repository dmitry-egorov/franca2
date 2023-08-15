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
    } gta; // global temp arena

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

    inline void* next(const arena& arena, const uint align = 4) {
        let offset = (align - (arena.used_bytes % align)) % align;
        return (char*)arena.memory + arena.used_bytes + offset;
    }

    inline size_t remaining_bytes(const arena& arena) {
        return arena.size_bytes - arena.used_bytes;
    }

    inline void* alloc_g(arena& arena, const size_t size_bytes, const uint align = 4) {
        void* next_free_memory = next(arena, align);

        let new_used_bytes = (size_t)next_free_memory - (size_t)arena.memory + size_bytes;
        assert(new_used_bytes <= arena.size_bytes);
        arena.used_bytes += new_used_bytes;
        return next_free_memory;
    }

    tt array_view<t> alloc_g(arena& arena, const size_t count) {
        return {(t*)alloc_g(arena, count * sizeof(t)), count};
    }

    inline void* alloc(arena& arena, const size_t size_bytes, const uint align = 4) {
        let result = alloc_g(arena, size_bytes, align);
        memset(result, 0, size_bytes);
        return result;
    }

    tt array_view<t> alloc(arena& arena, const size_t count) {
        return {(t*)alloc(arena, count * sizeof(t)), count};
    }

    tt array_view<t> alloc(arena& arena, const initializer_list<t> list) {
        let count = list.size();
        chk(count > 0) else return {0, 0};

        var data = alloc_g<t>(arena, count);

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
    inline void gta_init   (const size_t capacity_bytes) { init   (gta, capacity_bytes); }
    inline void gta_release(                           ) { release(gta); }
    inline void gta_reset  (                           ) { reset  (gta); }

    inline void*     gta_alloc_g(const size_t size_bytes       ) { return alloc_g   (gta, size_bytes );}
    tt array_view<t> gta_alloc_g(const size_t count            ) { return alloc_g<t>(gta, count); }

    inline void*     gta_alloc(const size_t size_bytes       ) { return alloc   (gta, size_bytes );}
    tt array_view<t> gta_alloc(const size_t count            ) { return alloc<t>(gta, count); }
    tt array_view<t> gta_alloc(const initializer_list<t> list) { return alloc<t>(gta, list ); }

    tt t* gta_one(const t& item) { return alloc_one<t>(gta, item); }

}
#endif //FRANCA2_WEB_ARENAS_H
