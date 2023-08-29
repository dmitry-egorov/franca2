#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_DEBUG_H
#define FRANCA2_COMPUTE_DEBUG_H

#include "strings.h"
#include "compute_asts.h"

namespace compute_asts {
    inline void print_ast(const ast& ast);

    namespace printing {
        using namespace printing;
        using namespace strings;

        void print_node_chain(const node*);
    }

    inline void print_ast(const ast& ast) {
        if(ast.root); else { printf("AST is empty\n"); return; }
        printing::print_node_chain(ast.root);
        printf("\n");
    }

    namespace printing {
        void print_node_chain(const node* first_node) {
            using enum builtin_func_id;
            var node_p = first_node;
            if (node_p) print(node_p->prefix);
            while (node_p) {
                ref node = *node_p;
                var text = node.text;
                if (node.text_is_quoted) printf("\"");
                printf("%.*s", (int) text.count, text.data);
                if (node.text_is_quoted) printf("\"");

                if (is_func(node)) {
                    printf("[");
                    if_ref(child, node.first_child) {
                        print_node_chain(&child);
                    }
                    printf("]");
                }

                print(node.suffix);
                node_p = node.next;
            }
        }
    }
}

#endif //FRANCA2_COMPUTE_DEBUG_H

#pragma clang diagnostic pop