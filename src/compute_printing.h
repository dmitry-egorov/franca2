#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_DEBUG_H
#define FRANCA2_COMPUTE_DEBUG_H

#include "strings.h"
#include "compute_asts.h"

namespace compute_asts {
    inline void print_ast(const ast* ast);

    namespace printing {
        using namespace printing;
        using namespace strings;

        void print_node(const node* node, bool print_top_level_quotes);
    }

    inline void print_ast(const ast* ast) {
        chk(ast->root) else { printf("AST is empty\n"); return; }
        strings ::print(ast->root->prefix);
        printing::print_node(ast->root, true);
        printf("\n");
    }

    namespace printing {
        void print_node(const node* node, const bool print_top_level_quotes) {
            using enum node_type;

            chk(node) else return;
            switch (node->type) {
                case int_literal: {
                    printf("%d", node->int_value);
                    break;
                }
                case str_literal: {
                    let text = node->str_value;
                    if (print_top_level_quotes) printf("\"");
                    printf("%.*s", (int)text.count, text.data);
                    if (print_top_level_quotes) printf("\"");
                    break;
                }
                case list: {
                    let child = node->first_child;
                    if (child && child->type == int_literal && child->int_value == 4) {
                        printf("\"");
                        print_node(child->next_sibling, false);
                        printf("\"");
                        break;
                    }

                    printf("[");
                    if (child) {
                        print(child->prefix);
                        print_node(child, true);
                    }
                    printf("]");
                    break;
                }
            }

            if (node->suffix.count > 0)
                print(node->suffix);

            if (node->next_sibling)
                print_node(node->next_sibling, print_top_level_quotes);
        }
    }
}

#endif //FRANCA2_COMPUTE_DEBUG_H

#pragma clang diagnostic pop