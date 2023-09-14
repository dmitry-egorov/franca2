#ifndef FRANCA2_COMPUTE_CHECKING_H
#define FRANCA2_COMPUTE_CHECKING_H

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#include "utility/strings.h"
#include "utility/swap_queue.h"
#include "utility/wasm_emit.h"

namespace compute_asts {
    void analyze(ast&);

    namespace analysis {
        struct task {
            node * node ;
        };

        void gather_scopes_and_defs      (scope&, ast&);
        void gather_scopes_and_defs_chain(node* , scope&, ast&);
        void gather_scopes_and_defs      (node& , scope&, ast&);
        void gather_param                (node& , macro&);

        void type_check      (scope&, ast &);
        void type_check_chain(node* , ast &);
        void type_check      (node& , ast &);
    }

    void analyze(ast& ast) {
        using namespace swap_queues;
        using namespace analysis;
        using enum type_t;

        add_import("env", "print_str", { t_str }, t_void, ast);

        ref macro_main = add_macro("main", nullptr, ast.root, ast);
        macro_main.result_type = t_u32;

        add_fn(macro_main, true, ast);

        if_ref(root_scope, macro_main.body_scope); else { dbg_fail_return; }

        //var task_queue = make_swap_queue<task>(ast.temp_arena);
        //
        //for_chain(root_scope.body_chain) {
        //    ref node = *it;
        //    node.parent_scope = &root_scope;
        //    push(task_queue, {&node});
        //}
        //
        //while(true) {
        //    if_var1(task, try_pop(task_queue)); else break;
        //    if_ref (node, task.node); else { dbg_fail_return; }
        //}

        gather_scopes_and_defs(root_scope, ast);
        type_check            (root_scope, ast);
    }

    namespace analysis {
        using namespace wasm_emit;
        using enum type_t;
        using enum node::lex_kind_t;
        using enum node::sem_kind_t;
        using enum local::kind_t;

        void gather_scopes_and_defs(scope & scope, ast & ast) {
            gather_scopes_and_defs_chain(scope.body_chain, scope, ast);
        }

        void gather_scopes_and_defs_chain(node* first_node, scope& scope, ast& ast) {
            for_chain(first_node)
                gather_scopes_and_defs(*it, scope, ast);
        }

        void gather_scopes_and_defs(node& node, scope& scope, ast& ast) {
            node.parent_scope = &scope;

            if (node.text == def_id) {
                node.sem_kind      = sk_macro_decl;
                node.decled_macros = make_arr_dyn<macro*>(128, ast.data_arena);

                for_chain(node.first_child) {
                    if_ref(id_node, it); else { dbg_fail_return; }
                    if_chain3(params_node, results_node, body_node, id_node.first_child); else { dbg_fail_return; }

                    let body_chain = body_node.text == list_id ? body_node.first_child : &body_node;
                    ref macro = add_macro(id_node.text, node.parent_scope, body_chain, ast);

                    if (params_node.text == list_id)
                        for_chain2(prm_it, params_node.first_child)
                            gather_param(*prm_it, macro);
                    else
                        gather_param(params_node, macro);

                    if_set1(macro.result_type, primitive_type_by(results_node.text)); else macro.result_type = t_void;

                    push(node.decled_macros, &macro);

                    gather_scopes_and_defs_chain(body_chain, *macro.body_scope, ast);
                }
                return;
            }

            gather_scopes_and_defs_chain(node.first_child, scope, ast);
        }

        void gather_param(node& prm_node, macro& macro) {
            if_chain2(prm_kind_node, prm_type_node, prm_node.first_child); else { dbg_fail_return; }

            let is_ref  = prm_kind_node.text ==  ref_id;
            let is_val  = prm_kind_node.text ==   in_id;
            let is_code = prm_kind_node.text == code_id;
            if (is_ref || is_val || is_code); else { dbg_fail_return; }

            if_var1(value_type, primitive_type_by(prm_type_node.text)); else value_type = t_void;

            //TODO: support other kinds
            let kind = is_ref ? lk_ref : is_val ? lk_value : lk_code;
            add_param(prm_node.text, kind, value_type, macro);
            prm_node.value_type = value_type;
        }

        void type_check(scope & scope, ast & ast) {
            type_check_chain(scope.body_chain, ast);
        }

        void type_check_chain(node* first_node, ast& ast) {
            for_chain(first_node)
                type_check(*it, ast);
        }

        void type_check(node& node, ast& ast) {
            if_ref(scope, node .parent_scope); else { dbg_fail_return; }

            if (node.sem_kind == sk_macro_decl) {
                for_arr(node.decled_macros) {
                    if_ref(decl_macro, node.decled_macros[i]); else { dbg_fail_return; }
                    if_ref(body      , decl_macro.body_scope); else { dbg_fail_return; }
                    type_check(body, ast);
                }
                return;
            }

            if (node.is_string     ) { node.sem_kind = sk_lit; node.value_type = t_str; return; }
            if (node.can_be_uint   ) { node.sem_kind = sk_lit; node.value_type = t_u32; return; }
            if (node.can_be_int    ) { node.sem_kind = sk_lit; node.value_type = t_i32; return; }
            if (node.can_be_float  ) { node.sem_kind = sk_lit; node.value_type = t_f32; return; }

            if (node.text == ret_id) {
                for_chain(node.first_child)
                    type_check(*it, ast);

                if_ref(macro, scope.parent_macro); else { dbg_fail_return; }
                node.sem_kind     = sk_ret;
                node.value_node   = node.first_child;
                node.value_type   = t_void;
                macro.has_returns = true  ;
                return;
            }

            if_var1(wasm_type, find_value_type(node.text)) {
                node.sem_kind   = sk_wasm_type;
                node.wasm_type  = wasm_type;
                node.value_type = t_void;
                return;
            }

            if_var1(wasm_op, find_op(node.text)) {
                node.sem_kind   = sk_wasm_op;
                node.wasm_op    = wasm_op;
                node.value_type = t_void;
                if (is_func(node)) {
                    for_chain(node.first_child) {
                        ref arg = *it;
                        if (is_uint_literal(arg)); else { printf("Only numeric literals are supported as arguments for opcodes.\n"); node_error(arg); return; }
                        arg.sem_kind   = sk_lit;
                        arg.value_type = t_u32;
                    }
                }
                return;
            }

            if_ref(local, find_local(node.text, scope)) {
                node.sem_kind    = local.kind == lk_code ? sk_code_embed : sk_local_get;
                node.refed_local = &local;
                node.value_type  = local.value_type;
                return;
            }

            if (node.text == list_id) {
                node.sem_kind   = sk_block;
                node.value_type = t_void;
                type_check_chain(node.first_child, ast);
                return;
            }

            if (node.text == decl_local_id) {
                if_chain2(id_node, type_node, node.first_child); else { dbg_fail_return; }
                if_var1(type, primitive_type_by(type_node.text)); else { dbg_fail_return; }

                ref loc  = add_local(id_node.text, lk_value, type, scope);
                if_ref(init_node, type_node.next) {
                    type_check(init_node, ast);
                    //TODO: check if types are compatible
                }
                node.sem_kind     = sk_local_decl;
                node.decled_local = &loc;
                node.init_node    = &init_node;
                node.value_type   = type;
                return;
            }

            if (node.text == ref_id) {
                if_ref(var_id, node.first_child); else { dbg_fail_return; }
                if_ref(refed_local, find_local(var_id.text, scope)); else { dbg_fail_return; }
                if (refed_local.kind == lk_ref); else { printf("Only mutable variables can be referenced.\n"); node_error(node); return; }
                node.sem_kind    = sk_local_ref;
                node.refed_local = &refed_local;
                node.value_type  =  refed_local.value_type;
                return;
            }

            if (node.text == chr_id) {
                if_ref(value_node, node.first_child); else { dbg_fail_return; }
                node.sem_kind   = sk_chr;
                node.chr_value  = (u8)value_node.text[0];
                node.value_type = t_u32; // TODO: t_u8
                return;
            }

            if (node.text == sub_of_id) {
                if_chain2(sub_of_index, sub_of_node, node.first_child); else { dbg_fail_return; }
                if_var1  (idx, get_uint(sub_of_index)); else { dbg_fail_return; }

                type_check(sub_of_node, ast);

                let size = size_32_of(sub_of_node.value_type);
                if (idx < size); else { printf("Index %u is out of bounds for type %s.\n", idx, sub_of_node.text.data); node_error(node); return; }

                node.sem_kind    = sk_sub_of;
                node.sub_index   = sub_of_index.uint_value;
                node.sub_of_node = &sub_of_node;
                node.value_type  = t_u32;
                return;
            }

            if (node.text == each_id) {
                if_chain2(list_node, body_node, node.first_child); else { dbg_fail_return; }
                type_check(list_node, ast);
                if (list_node.sem_kind == sk_code_embed); else { printf("Only code vars can be used as arguments for 'each'.\n"); node_error(node); return; }
                if_ref(l, list_node.refed_local); else { dbg_fail_return; }
                if (l.kind == lk_code); else { printf("Only code vars can be used as arguments for 'each'.\n"); node_error(node); return; }

                node.sem_kind        = sk_each;
                node.each_list_local = list_node.refed_local;
                node.each_body_node  = &body_node;
                node.value_type      = t_void;
                return;
            }

            // macro invocation or function call
            {
                var arg_types = make_arr_dyn<type_t>(4, ast.data_arena);
                for_chain(node.first_child) {
                    ref arg = *it;
                    type_check(arg, ast);
                    push(arg_types, arg.value_type);
                }

                if_ref(invoked_macro, find_macro(node.text, arg_types.data, scope)) {
                    node.sem_kind    = sk_macro_invoke;
                    node.refed_macro = &invoked_macro;
                    node.value_type  =  invoked_macro.result_type;
                    return;
                }

                if_ref(called_fn, find_fn(node.text, arg_types.data, ast)) {
                    node.sem_kind   = sk_fn_call;
                    node.refed_fn   = ref_in(&called_fn, ast.fns.data);
                    node.value_type = called_fn.result_type;
                    return;
                }
            }

            printf("Undeclared identifier %.*s\n", (int)node.text.count, node.text.data); dbg_fail_return;
        }
    }
}

#pragma clang diagnostic pop

#endif //FRANCA2_COMPUTE_CHECKING_H
