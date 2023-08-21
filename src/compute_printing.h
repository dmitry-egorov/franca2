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

        void print_list(const node* node_list, bool print_top_level_quotes);
        void print_list(const node& node, bool print_top_level_quotes);
    }

    inline void print_ast(const ast& ast) {
        if(ast.root); else { printf("AST is empty\n"); return; }
        strings ::print(ast.root->prefix);
        printing::print_list(ast.root, true);
        printf("\n");
    }

    namespace printing {
        void print_list(const node& node, const bool print_top_level_quotes) {
            using enum node::type_t;
            using enum builtin_func_id;

            switch (node.type) {
                case literal: {
                    if_var1(i, get_int(node))
                        printf("%d", i);
                    else { if_var1 (text, get_str(node)) {
                        if (print_top_level_quotes) printf("\"");
                        printf("%.*s", (int)text.count, text.data);
                        if (print_top_level_quotes) printf("\"");
                    }
                    else {
                        assert(false); // unreachable
                    }}
                    break;
                }
                case list: {
                    if_ref(child, node.first_child) {
                        var [func_id, ok] = get_int(child);
                        if (ok && func_id == (uint)istring) {
                            printf("\"");
                            print_list(child.next, false);
                            printf("\"");
                            break;
                        }
                        else {
                            printf("[");
                            print(child.prefix);
                            print_list(&child, true);
                            printf("]");
                        }
                    }
                    else
                        printf("[]");

                    break;
                }
            }

            if (node.suffix.count > 0)
                print(node.suffix);

            if (node.next)
                print_list(node.next, print_top_level_quotes);
        }

        void print_list(const node* node_list, const bool print_top_level_quotes) {
            if_ref(node, node_list); else return;
            print_list(node, print_top_level_quotes);
        }
    }
}

#endif //FRANCA2_COMPUTE_DEBUG_H

#pragma clang diagnostic pop