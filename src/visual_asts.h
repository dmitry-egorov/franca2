#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_VISUAL_ASTS_H
#define FRANCA2_VISUAL_ASTS_H

#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/arenas.h"
#include "utility/arrays.h"

namespace visual_asts {
    struct ast ;
    struct node;

    namespace {
        using namespace arrays;
        using namespace arenas;

        void print_ast_for(const node* node);
    }

    struct ast_storage {
        arena node_arena;
        arena text_arena;
    };

    struct ast {
        node* root;
        ast_storage storage;
    };

    struct node {
        palette_color color_id;

        array_view<char> prefix_text;

        node* parent      ;
        node* first_child ;
        node* next_sibling;
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

    inline void print_ast(const ast* ast) {
        print_ast_for(ast->root);
        printf("\n");
    }

    namespace {
        void print_ast_for(const node* node) {
            printf("%.*s", (int)node->prefix_text.count, node->prefix_text.data);

            var child = node->first_child;
            if (child) {
                printf("[");
                if (child->color_id != palette_color::regulars)
                    printf("%u", (uint8_t)child->color_id);
                print_ast_for(child);
                printf("]");
            }

            if (node->next_sibling)
                print_ast_for(node->next_sibling);
        }
    }
}
#endif //FRANCA2_VISUAL_ASTS_H

#pragma clang diagnostic pop