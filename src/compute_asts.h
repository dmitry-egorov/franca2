#ifndef FRANCA2_COMPUTE_AST_H
#define FRANCA2_COMPUTE_AST_H

#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/strings.h"
#include "utility/arenas.h"
#include "utility/iterators.h"
#include "utility/files.h"
#include "compute_asts.h"

namespace compute_asts {
    enum struct node_type {
        int_literal,
        str_literal,
        list,
    };

    struct node {
        node_type type;

        union {
            uint int_value;
            arrays::array_view<char> str_value;
        };

        arrays::array_view<char> prefix;
        arrays::array_view<char> suffix;

        node* parent;
        node* first_child;
        node* next_sibling;
    };

#define loop_over_sequence(name, first_node) for (var name = first_node; name; name = name->next_sibling)

    struct ast_storage {
        arenas::arena node_arena;
        arenas::arena text_arena;
    };

    struct ast {
        node* root;

        ast_storage storage;
    };

    inline node* make_node(ast_storage& storage, const node& node) {
        return alloc_one(storage.node_arena, node);
    }

    inline void release(ast_storage& storage) {
        release(storage.node_arena);
        release(storage.text_arena);
    }

    inline void release(ast& ast) {
        release(ast.storage);
        ast.root = nullptr;
    }
}
#endif //FRANCA2_COMPUTE_AST_H