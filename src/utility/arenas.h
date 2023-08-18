#ifndef FRANCA2_ARENAS_H
#define FRANCA2_ARENAS_H

#include <cassert>
#include <initializer_list>
#include "syntax.h"
#include "arrays.h"

namespace arenas {
    using namespace arrays;

    struct arena {
        void* memory;
        size_t size_bytes;
        size_t used_bytes;
    };

    inline arena gta;// global temp arena

    inline arena make   (size_t capacity);
    inline void  init   (arena&, size_t capacity);
    inline void  release(arena&);

    inline void reset(arena&);
    inline size_t alignment_offset(const arena& arena, uint align);

    inline size_t next_offset(const arena&, uint align = sizeof(size_t));
    inline void*  next       (const arena&, uint align = sizeof(size_t));
    inline size_t bytes_left (const arena&, uint align = sizeof(size_t));

    tt array_view<t> free_space_of(arena&, uint align = sizeof(size_t));
    inline void*     alloc_g(arena&, size_t size_bytes, uint align = sizeof(size_t));
    tt array_view<t> alloc_g(arena&, size_t count, uint align = sizeof(size_t));

    inline void*     alloc(arena&, size_t size_bytes, uint align = sizeof(size_t));
    tt array_view<t> alloc(arena&, size_t count, uint align = sizeof(size_t));
    tt array_view<t> alloc(arena& arena, std::initializer_list<t> list, uint align = sizeof(size_t));

    tt t* alloc_one(arena& arena, const t& item, uint align = sizeof(size_t));

    inline void gta_init   (size_t capacity_bytes);
    inline void gta_release();
    inline void gta_reset  ();

    inline void*     gta_alloc_g(size_t size_bytes, uint align = sizeof(size_t));
    tt array_view<t> gta_alloc_g(size_t count, uint align = sizeof(size_t));

    inline void*     gta_alloc(size_t size_bytes, uint align = sizeof(size_t));
    tt array_view<t> gta_alloc(size_t count, uint align = sizeof(size_t));
    tt array_view<t> gta_alloc(std::initializer_list<t> list, uint align = sizeof(size_t));

    tt t* gta_one(const t& item, uint align = sizeof(size_t));

    tt array<t> alloc_array(arena& arena, const size_t capacity, uint align = sizeof(size_t));
}
#endif //FRANCA2_ARENAS_H

#ifdef FRANCA2_ARENAS_IMPL
#ifndef FRANCA2_ARENAS_I
#define FRANCA2_ARENAS_I

namespace arenas {
    arena make(const size_t capacity) { return { malloc(capacity), capacity, 0 }; }
    void  init(arena& arena, const size_t capacity) { arena = make(capacity); }

    void release(arena& arena) {
        free(arena.memory);
        arena.memory = nullptr;
        arena.size_bytes = 0;
        arena.used_bytes = 0;
    }

    void reset(arena &arena) { arena.used_bytes = 0; }

    size_t alignment_offset(const arena &arena, const uint align) {
        return (align - (arena.used_bytes % align)) % align;
    }

    size_t next_offset(const arena &arena, const uint align) {
        return arena.used_bytes + alignment_offset(arena, align);
    }

    void *next(const arena &arena, const uint align) {
        return (char*)arena.memory + next_offset(arena, align);
    }

    size_t bytes_left(const arena &arena, uint align) {
        return arena.size_bytes - next_offset(arena, align);
    }

    tt array_view<t> free_space_of(arena &arena, const uint align) {
        let offset = next_offset(arena, align);
        return { (t*)((char*)arena.memory + offset), (uint)((arena.size_bytes - offset) / sizeof(t)) };
    }

    void *alloc_g(arena &arena, const size_t size_bytes, const uint align) {
        let offset         = next_offset(arena, align);
        let new_used_bytes = offset + size_bytes;
        assert(new_used_bytes <= arena.size_bytes);
        arena.used_bytes = new_used_bytes;
        return (char*)arena.memory + offset;
    }

    tt array_view<t> alloc_g(arena &arena, const size_t count, const uint align) {
        return { (t*)alloc_g(arena, count * sizeof(t), align), (uint)count };
    }

    void *alloc(arena &arena, const size_t size_bytes, const uint align) {
        let result = alloc_g(arena, size_bytes, align);
        memset(result, 0, size_bytes);
        return result;
    }

    tt array_view<t> alloc  (arena &arena, const size_t count, const uint align) {
        return { (t*)alloc(arena, count * sizeof(t), align), (uint)count };
    }

    tt array_view<t> alloc(arena& arena, const std::initializer_list<t> list, const uint align) {
        let count = list.size();
        chk(count > 0) else return { 0, 0 };

        var data = alloc_g<t>(arena, count, align);

        var i = 0;
        for (let e : list) {
            data[i] = e;
            i++;
        }

        return data;
    }

    tt t* alloc_one(arena& arena, const t& item, const uint align) {
        return alloc<t>(arena, { item }, align).data;
    }

    tt array<t> alloc_array(arena& arena, const size_t capacity, const uint align) {
        return { alloc<t>(arena, capacity, align), 0 };
    }

    // gta_ -> global temp arena
    void gta_init(const size_t capacity_bytes) { init(gta, capacity_bytes); }
    void gta_release() { release(gta); }
    void gta_reset() { reset(gta); }

    void* gta_alloc_g(const size_t size_bytes, const uint align) { return alloc_g(gta, size_bytes, align); }
    tt array_view<t> gta_alloc_g(const size_t count, const uint align) { return alloc_g<t>(gta, count, align); }

    void* gta_alloc(const size_t size_bytes, const uint align) { return alloc(gta, size_bytes, align); }
    tt array_view<t> gta_alloc(const size_t count, const uint align) { return alloc<t>(gta, count, align); }
    tt array_view<t> gta_alloc(const std::initializer_list<t> list, const uint align) { return alloc<t>(gta, list, align); }

    tt t* gta_one(const t& item, const uint align) { return alloc_one<t>(gta, item, align); }
}

#endif //FRANCA2_ARENAS_I
#endif //FRANCA2_ARENAS_IMPL