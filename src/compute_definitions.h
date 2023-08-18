#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_DEFINES_H
#define FRANCA2_COMPUTE_DEFINES_H

#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/iterators.h"
#include "utility/strings.h"
#include "code_views.h"
#include "compute_asts.h"

namespace compute_asts {
    struct definition {
        uint id;
        arrays::array_view<char> name;
        node* value_node;
    };

    static arrays::array_view<definition> gather_definitions(ast* ast, arenas::arena* arena = &arenas::gta);
    ret1<arrays::array_view<char>> find_name(arrays::array_view<definition> definitions, uint id);

    static arrays::array_view<definition> gather_definitions(ast* ast, arenas::arena* arena) {
        using enum node_type;

        var definitions = alloc_array<definition>(*arena, 1024);

        var node = ast->root;
        while (node) { defer {node = node->next_sibling; };
            chk(node->type == list) else continue;
            let arguments_node = node->first_child;

            // let
            chk(arguments_node->type == int_literal && arguments_node->int_value == 1) else continue;

            let id_node = arguments_node->next_sibling;
            assert(id_node->type == int_literal);
            let name_node = id_node->next_sibling;
            assert(name_node->type == str_literal);
            let value_node = name_node->next_sibling;

            let id   =   id_node->int_value;
            let name = name_node->str_value;

            push(definitions, {id, name, value_node});
        }

        return view_of(definitions);
    }

    ret1<arrays::array_view<char>> find_name(const arrays::array_view<definition> definitions, const uint id) {
        for (var i = 0u; i < definitions.count; ++i) {
            let definition = definitions[i];
            if (definition.id == id) return ret1_ok(definition.name);
        }

        return ret1_fail;
    }

    ret1<definition> find(const arrays::array_view<definition> definitions, const uint id) {
        for (var i = 0u; i < definitions.count; ++i) {
            let definition = definitions[i];
            if (definition.id == id) return ret1_ok(definition);
        }

        return ret1_fail;
    }
}

#endif //FRANCA2_COMPUTE_DEFINES_H

#pragma clang diagnostic pop