
//
// Created by degor on 20.09.2023.
//

#ifndef FRANCA2_STRING_TABLES_H
#define FRANCA2_STRING_TABLES_H

#include "syntax.h"
#include "arrays.h"
#include "arenas.h"
#include "strings.h"

namespace string_tables {
    using namespace arrays;
    using namespace strings;
    
    typedef uint string_id;
    typedef uint string_hash;

    struct string_table {
        arr_buck<string> strings_in_order;
        string_id*   ids; // index in the strings_in_order array
        string_hash* hashes;
        size_t size;

        arena* arena;

        static const uint load_factor = 70;
    };

    struct hash_table_slot {
        string_id id;
        uint hash_index;
    };

    auto make_string_table(size_t size, arena& = gta) -> string_table;
    auto get_or_add       (string, string_table&) -> string_id;
    auto hash_str         (string) -> string_hash;
    auto find_slot        (string, string_hash, string_table&) -> hash_table_slot;
    void expand           (string_table&);
}
#endif //FRANCA2_STRING_TABLES_H

#ifdef FRANCA2_IMPLS
#ifndef FRANCA2_ASTS_SYMBOLS_I
#define FRANCA2_ASTS_SYMBOLS_I

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

namespace string_tables {
    auto make_string_table(size_t size, arena& arena) -> string_table {
        var tbl = string_table {
            .strings_in_order = make_arr_buck<string>(size, arena),
            .ids    = alloc_g<string_id  >(arena, size).data,
            .hashes = alloc_g<string_hash>(arena, size).data,
            .size   = size,
            .arena  = &arena
        };

        memset(tbl.hashes, 0xff, size * sizeof(uint     ));
        memset(tbl.ids   , 0xff, size * sizeof(string_id));

        return tbl;
    }

    string_id get_or_add(string str, string_table& table) {
        let hash = hash_str(str);
        var [id, hash_index] = find_slot(str, hash, table);
        if (id != 0xffffffff) return id;

        // not found, add
        let count = count_of(table.strings_in_order);
        if (count * 100 > string_table::load_factor * table.size) {
            // expand and rehash
            expand(table);
            return get_or_add(str, table);
        }

        id = count_of(table.strings_in_order);
        table.hashes[hash_index] = hash;
        table.ids   [hash_index] = id;
        push(str    , table.strings_in_order);
        return id;
    }

    auto hash_str(string view) -> string_hash {
        var hash = 0u;
        for_arr(view)
            hash = hash * 397 + (uint) view[i];
        return hash;
    }

    auto find_slot(string str, string_hash hash, string_table& table) -> hash_table_slot {
        var result = hash_table_slot {};
        ref [id, hash_index] = result;

        hash_index = hash % table.size;
        var probing_offset = 1 + hash % (table.size - 1);

        while (true) {
            id = table.ids[hash_index];
            if (id == 0xffffffff)
                break;

            if (table.hashes[hash_index] == hash && table.strings_in_order[id] == str)
                return result;

            hash_index = (hash_index + probing_offset) % table.size;
            probing_offset += 1;
        }

        return result;
    }

    void expand(string_table& table) {
        ref arena    = *table.arena;
        let new_size = table.size * 2;

        var new_table = string_table {
            .strings_in_order = table.strings_in_order,
            .ids    = alloc_g<string_id  >(arena, new_size).data,
            .hashes = alloc_g<string_hash>(arena, new_size).data,
            .size   = new_size,
            .arena  = &arena
        };

        memset(new_table.ids   , 0xff, new_size * sizeof(string_id  ));
        memset(new_table.hashes, 0xff, new_size * sizeof(string_hash));

        for_arr_buck_begin(table.strings_in_order, str, id) {
            let hash = hash_str(str);
            let [_, hash_index] = find_slot(str, hash, new_table);

            new_table.ids   [hash_index] = id;
            new_table.hashes[hash_index] = hash;
        } for_arr_buck_end

        table = new_table;
    }
}
#pragma clang diagnostic pop

#endif //FRANCA2_ASTS_SYMBOLS_I
#endif //FRANCA2_IMPLS

