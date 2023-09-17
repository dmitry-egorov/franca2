#ifndef FRANCA2_ASTS_ANALYSIS_H
#define FRANCA2_ASTS_ANALYSIS_H

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#include "utility/strings.h"
#include "utility/swap_queuess.h"
#include "utility/wasm_emit.h"

namespace asts {
    void analyze(ast&);

    namespace analysis {
        void analyze_chain(node*, scope&, ast&);
        void analyze      (node&, scope&, ast&);
        void gather_param (node&, macro&);

        void complete(node&, node::sem_kind_t, type_t);
        void enqueue(node&, ast&);
    }

    void analyze(ast& ast) {
        using namespace analysis;
        using enum node::stage_t;
        using enum type_t;

        add_import("env", "print_str", { t_str }, t_void, ast);

        ref macro_main = add_macro("main", nullptr, ast.root, ast);
        macro_main.result_type = t_u32;

        add_fn(macro_main, true, ast);

        if_ref(root_scope, macro_main.body_scope); else { dbg_fail_return; }

        analyze_chain(root_scope.chain, root_scope, ast);
        while(true) {
            if_var1(node_p, try_pop(ast.pipeline)); else {
                if (push_queue_is_empty(ast.pipeline))
                    break;

                if (queues_identical(ast.pipeline)) {
                    printf("Unprocessed nodes: \n");
                    for_arr(ast.pipeline.push_queue) {
                        if_ref(node, ast.pipeline.push_queue[i]); else { dbg_fail_return; }
                        printf("%.*s: %.*s\n", (int)node.file_path.count, node.file_path.data, (int)node.text.count, node.text.data);
                    }
                    dbg_fail_return;
                }

                swap(ast.pipeline);
                continue;
            }
            if_ref (node , node_p); else { dbg_fail_return; }
            if_ref (scope, node.parent_scope); else { dbg_fail_return; }

            node.stage = ns_pending;
            analyze(node, scope, ast);
        }
    }

    namespace analysis {
        using namespace swap_queuess;
        using namespace wasm_emit;
        using enum type_t;
        using enum node::lex_kind_t;
        using enum node::sem_kind_t;
        using enum node::stage_t;
        using enum local::kind_t;

        void analyze_chain(node* first_node, scope& scope, ast& ast) {
            for_chain(first_node)
                analyze(*it, scope, ast);
        }

        void analyze(node& node, scope& scope, ast& ast) {
            if (node.stage == ns_pending); else return;

            node.parent_scope = &scope;

            if (node.is_string   ) { complete(node, sk_lit, t_str); return; }
            if (node.can_be_uint ) { complete(node, sk_lit, t_u32); return; }
            if (node.can_be_int  ) { complete(node, sk_lit, t_i32); return; }
            if (node.can_be_float) { complete(node, sk_lit, t_f32); return; }

            if (node.text == def_id) {
                node.decled_macros = make_arr_dyn<asts::macro*>(128, ast.data_arena);

                for_chain(node.first_child) {
                    if_ref(id_node, it); else { dbg_fail_return; }
                    if_chain3(params_node, results_node, body_node, id_node.first_child); else { dbg_fail_return; }

                    let body_chain = body_node.text == list_id ? body_node.first_child : &body_node;
                    ref decl_macro = add_macro(id_node.text, node.parent_scope, body_chain, ast);

                    for_chain2(prm_it, params_node.first_child)
                        gather_param(*prm_it, decl_macro);

                    if_set1(decl_macro.result_type, find_type(results_node.text)); else decl_macro.result_type = t_void;

                    push(node.decled_macros, &decl_macro);

                    analyze_chain(body_chain, *decl_macro.body_scope, ast);
                }

                complete(node, sk_macro_decl, t_void);
                return;
            }

            if (node.text == ret_id) {
                analyze_chain(node.first_child, scope, ast);

                if_ref(macro, scope.macro); else { dbg_fail_return; }
                macro.has_returns = true;

                complete(node, sk_ret, t_void);
                return;
            }

            if_var1(wasm_type, find_value_type(node.text)) {
                node.wasm_type = wasm_type;
                complete(node, sk_wasm_type, t_void);
                return;
            }

            if_var1(wasm_op, find_op(node.text)) {
                analyze_chain(node.first_child, scope, ast);

                for_chain(node.first_child) {
                    ref arg = *it;
                    if (arg.sem_kind == sk_lit && arg.value_type == t_u32); else { printf("Only u32 literals are supported as arguments for opcodes.\n"); node_error(arg); return; }
                    complete(arg, sk_lit, t_u32);
                }

                node.wasm_op = wasm_op;
                complete(node, sk_wasm_op, t_void);
                return;
            }

            if_ref(refed_local, find_local(node.text, scope)) {
                let sem_kind = refed_local.kind == lk_code ? sk_code_embed : sk_local_get;
                node.refed_local = &refed_local;
                complete(node, sem_kind, refed_local.value_type);
                return;
            }

            if (node.text == list_id) {
                if_ref(macro, scope.macro); else { dbg_fail_return; }
                ref new_scope = add_scope(macro, &scope, node.first_child, ast);
                analyze_chain(node.first_child, new_scope, ast);
                complete(node, sk_block, t_void);
                return;
            }

            if (node.text == decl_local_id) {
                if_chain2 (id_node, type_node, node.first_child)   ; else { dbg_fail_return; }
                if_var1   (type, find_type(type_node.text)); else { dbg_fail_return; }
                let init_node_p = type_node.next;

                if_ref(init_node, init_node_p)
                    analyze(init_node, scope, ast); //TODO: check if types are compatible

                ref local         = add_local(id_node.text, lk_value, type, scope);
                node.decled_local = &local;
                node.init_node    = init_node_p;
                complete(node, sk_local_decl, type);
                return;
            }

            if (node.text == ref_id) {
                if_ref(var_id, node.first_child); else { dbg_fail_return; }
                if_ref(local, find_local(var_id.text, scope)); else { dbg_fail_return; }
                if (local.kind == lk_ref); else { printf("Only mutable variables can be referenced.\n"); node_error(node); return; }
                node.refed_local = &local;
                complete(node, sk_local_ref, local.value_type);
                return;
            }

            if (node.text == chr_id) {
                if_ref(value_node, node.first_child); else { dbg_fail_return; }
                node.chr_value = (u8)value_node.text[0];
                complete(node, sk_chr, t_u32);  // TODO: t_u8
                return;
            }

            if (node.text == sub_of_id) {
                if_chain2(sub_of_index, sub_of_node, node.first_child); else { dbg_fail_return; }
                if_var1  (idx, get_uint(sub_of_index)); else { dbg_fail_return; }

                analyze(sub_of_node, scope, ast);

                let size = size_32_of(sub_of_node.value_type);
                if (idx < size); else { printf("Index %u is out of bounds for type %s.\n", idx, sub_of_node.text.data); node_error(node); return; }

                node.sub_index   = sub_of_index.uint_value;
                node.sub_of_node = &sub_of_node;
                complete(node, sk_sub_of, t_u32);
                return;
            }

            if (node.text == each_id) {
                if_chain2(list_node, body_node, node.first_child); else { dbg_fail_return; }
                analyze(list_node, scope, ast);

                if (list_node.sem_kind == sk_code_embed); else { printf("Only code vars can be used as arguments for 'each'.\n"); node_error(node); return; }
                if_ref(l, list_node.refed_local); else { dbg_fail_return; }
                if (l.kind == lk_code); else { printf("Only code vars can be used as arguments for 'each'.\n"); node_error(node); return; }

                node.each_list_local = list_node.refed_local;
                node.each_body_node  = &body_node;
                complete(node, sk_each, t_void);
                return;
            }

            // macro invocation or function call
            {
                var arg_types = make_arr_dyn<type_t>(4, ast.data_arena);
                for_chain(node.first_child) {
                    ref arg = *it;
                    analyze(arg, scope, ast);
                    push(arg_types, arg.value_type);
                }

                if_ref(invoked_macro, find_macro(node.text, arg_types.data, scope)) {
                    node.refed_macro = &invoked_macro;
                    complete(node, sk_macro_expand, invoked_macro.result_type);
                    return;
                }

                if_ref(called_fn, find_fn(node.text, arg_types.data, ast)) {
                    node.refed_fn = &called_fn;
                    complete(node, sk_fn_call, called_fn.result_type);
                    return;
                }
            }

            // could not analyze yet, queue for later
            enqueue(node, ast);
            printf("BAM!!! Queued: %.*s\n", (int)node.text.count, node.text.data);
        }

        void gather_param(node& prm_node, macro& macro) {
            if_chain2(prm_kind_node, prm_type_node, prm_node.first_child); else { dbg_fail_return; }

            let is_ref  = prm_kind_node.text ==  ref_id;
            let is_val  = prm_kind_node.text ==   in_id;
            let is_code = prm_kind_node.text == code_id;
            if (is_ref || is_val || is_code); else { dbg_fail_return; }

            if_var1(value_type, find_type(prm_type_node.text)); else value_type = t_void;

            //TODO: support other kinds
            let kind = is_ref ? lk_ref : is_val ? lk_value : lk_code;
            add_param(prm_node.text, kind, value_type, macro);
            prm_node.value_type = value_type;
        }

        void complete(node& node, node::sem_kind_t kind, type_t type) {
            node.sem_kind   = kind;
            node.value_type = type;
            node.stage = ns_analyzed;
        }

        void enqueue(node& node, ast& ast) {
            assert(node.stage == ns_pending);
            node.stage = ns_queued;
            push(ast.pipeline, &node);
        }
    }
}

#pragma clang diagnostic pop

#endif //FRANCA2_ASTS_ANALYSIS_H
