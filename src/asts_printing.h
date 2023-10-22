#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_ASTS_PRINTING_H
#define FRANCA2_ASTS_PRINTING_H

#include "strings.h"
#include "asts.h"

namespace asts {
    inline void print_ast (const ast&);
    inline void print_node(const node&);
    inline void print_exp (const expansion&, const ast&);

    namespace printing {
        void print_node_chain(const node*);

        void print_exp_node_chain(const node*, uint level, const ast&);
        void print_exp_node      (const node&, uint level, const ast&);
    }

    inline void print_ast(const ast& ast) {
        if(ast.root_chain); else { printf("AST is empty\n"); return; }
        printf("AST:\n");
        printing::print_node_chain(ast.root_chain);
        printf("\n");
    }

    inline void print_node(const node& node) {
        var text = node.text;
        if (node.is_string) printf("\"");
        printf("%.*s", (int) text.count, text.data);
        if (node.is_string) printf("\"");

        if (is_func(node)) {
            printf("[");
            if_ref(child, node.child_chain) {
                printing::print_node_chain(&child);
            }
            printf("]");
        }
    }

    inline void print_exp(const expansion& exp, const ast& ast) {
        using namespace printing;

        if_ref(macro, exp.macro); else { dbg_fail_return; }
        let macro_id = text_of(macro.id, ast);
        printf("%.*s\n", (int) macro_id.count, macro_id.data);
        print_exp_node_chain(exp.generated_chain, 1, ast);
    }

    namespace printing {
        using namespace printing;
        using namespace strings;
        using enum asts::node::sem_kind_t;
        using enum asts::local::kind_t;

        void print_node_chain(const node* first_node) {
            var node_p = first_node;
            if (node_p) printf(node_p->prefix);
            while (node_p) {
                ref node = *node_p;
                print_node(node);
                printf(node.suffix);
                node_p = node.next;
            }
        }

        void print_exp_node_chain(const node* chain, uint level, const ast& ast) {
            for_chain(chain)
                print_exp_node(*it, level, ast);
        }

        void print_exp_node(const node& node, uint level, const ast& ast) {
            //assert(node.expansion);
            assert(node.sem_kind != sk_macro_decl);
            printf("%*c", level * 4, ' ');
            if (node.is_string) printf("\"");
            printf("%.*s", (int) node.text.count, node.text.data);
            if (node.is_string) printf("\"");

            switch (node.sem_kind) {
                case sk_ret: {
                    if_ref(scope, node.scope); else { dbg_fail_return; }
                    //printf("^%u", scope.depth);
                    printf("\n");
                    if_ref(ret_node, node.child_chain)
                        print_exp_node(ret_node, level + 1, ast);
                    break;
                }
                case sk_macro_expand: {
                    printf(" ->\n");
                    if_ref(exp  , node.macro_expansion); else { dbg_fail_return; }
                    if_ref(macro, exp.macro); else { dbg_fail_return; }

                    for_rng(0u, macro.params_count) {
                        ref local = macro.locals[i];
                        if (local.kind == lk_value); else continue;
                        ref b = exp.bindings[i];
                        if_ref(init_node, b.init); else continue; //NOTE: we're using the passed local directly 
                        let local_name = text_of(local.id, ast);
                        printf("%*carg %.*s $%u:\n", (level + 1) * 4, ' ', (int)local_name.count, local_name.data, b.index_in_fn);
                        print_exp_node(init_node, level + 2, ast);
                    }
                    
                    print_exp_node_chain(exp.generated_chain, level + 1, ast);
                    break;
                }
                case sk_code_embed: {
                    printf(" <-\n");
                    print_exp_node_chain(node.child_chain, level + 1, ast);
                    break;
                }
                case sk_local_decl: {
                    if_ref(loc, node.decled_local); else { dbg_fail_return; }
                    let id = text_of(loc.id, ast);
                    printf(" %.*s $%u\n", (int) id.count, id.data, node.decl_local_index_in_fn);
                    print_exp_node(*node.init_node, level + 1, ast);
                    break;
                }
                case sk_local_get: {
                    printf(" $%u\n", node.local_index_in_fn);
                    break;
                }
                case sk_local_ref: {
                    if_ref(loc, node.refed_local); else { dbg_fail_return; }
                    let id = text_of(loc.id, ast);
                    printf(" %.*s $%u\n", (int) id.count, id.data, node.local_index_in_fn);
                    break;
                }
                default: {
                    printf("\n");
                    print_exp_node_chain(node.child_chain, level + 1, ast);
                    break;
                }
            }
        }
    }
}

#endif //FRANCA2_ASTS_PRINTING_H

#pragma clang diagnostic pop