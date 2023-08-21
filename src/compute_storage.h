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
    using enum builtin_func_id;

    struct variable {
        uint id;
        bool is_mutable;
        arrays::array_view<char> name;
        poly_value value;
    };

    static arrays::array_view<variable> gather_variables(ast& ast, arenas::arena& arena = arenas::gta);
    ret1<arrays::array_view<char>> find_name(const arrays::array_view<variable>& storage, uint id);

    namespace storage {
        using namespace arenas;
        void gather_from_list(const node* node, array<variable>& cells);
    }

    static arrays::array_view<variable> gather_variables(ast& ast, arenas::arena& arena) {
        using namespace storage;

        var cells = alloc_array<variable>(arena, 1024);
        gather_from_list(ast.root, cells);
        return cells.data;
    }

    ret1<arrays::array_view<char>> find_name(const arrays::array_view<variable>& storage, const uint id) {
        for (var i = 0u; i < storage.count; ++i) {
            let definition = storage[i];
            if (definition.id == id) return ret1_ok(definition.name);
        }

        return ret1_fail;
    }

    variable* find(arrays::array_view<variable> definitions, const uint id) {
        for (var i = 0u; i < definitions.count; ++i) {
            let definition = &definitions[i];
            if (definition->id == id) return definition;
        }

        return nullptr;
    }

    namespace storage {
        void gather_from_list(const node* node, array<variable>& cells) {
            using enum node::type_t;
            while (node) { defer {node = node->next; };
                ref n = *node;
                if (n.type == list); else continue;
                if_ref(children, n.first_child); else continue;
                if (value_is(children, (uint)decl_var));
                    else {
                        gather_from_list(&children, cells);
                        continue;
                    }

                if_var3(id_node, mut_node, name_node, deref_list3(children.next)); else { dbg_fail_continue; }

                if_var1(id  , get_int(  id_node)     ); else { dbg_fail_continue; }
                if_var1(name, get_str(name_node)     ); else { dbg_fail_continue; }
                if_var1(is_mutable, get_int(mut_node)); else { dbg_fail_continue; }

                push(cells, {id, is_mutable != 0, name, {}});
            }
        }

    }
}

#endif //FRANCA2_COMPUTE_DEFINES_H

#pragma clang diagnostic pop