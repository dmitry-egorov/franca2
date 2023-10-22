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
        void analyze_chain(node*, lex_scope&, ast&);
        void analyze      (node&, lex_scope&, ast&);
        void gather_param (node&, macro&);

        void complete(node&, node::sem_kind_t, type_t);
        void enqueue (node&, ast&);

        void expand      (fn&, ast&);
        auto expand_chain(node*, node* parent, expansion&, ast&) -> node*;
        auto expand      (node&, node* parent, expansion&, ast&) -> node*;
    }

    void analyze(ast& ast) {
        using namespace analysis;
        using enum node::stage_t;
        using enum type_t;

        add_import("env", "print_str", { t_str }, t_void, ast);

        ref main_macro = add_macro("main", nullptr, ast.root_chain, ast);
        main_macro.result_type = t_u32;

        if_ref(root_scope, main_macro.body_scope); else { dbg_fail_return; }
        analyze_chain(root_scope.chain, root_scope, ast);

        while(true) {
            if_var1(node_p, try_pop(ast.pipeline)); else {
                if (push_queue_is_empty(ast.pipeline))
                    break;

                if (queues_identical(ast.pipeline)) {
                    printf("Unprocessed nodes: \n");
                    for_arr(ast.pipeline.push_queue) {
                        if_ref(node, ast.pipeline.push_queue[i]); else { dbg_fail_return; }
                        let macro_id = node.scope->macro->id;
                        let macro_id_text = text_of(macro_id, ast);

                        printf("%.*s: %.*s (%.*s)\n",
                           (int)node.file_path.count,
                           node.file_path.data,
                           (int)node.text.count,
                           node.text.data,
                           (int)macro_id_text.count,
                           macro_id_text.data
                       );
                    }
                    dbg_fail_return;
                }

                swap(ast.pipeline);
                continue;
            }
            if_ref(node , node_p    ); else { dbg_fail_return; }
            if_ref(scope, node.scope); else { dbg_fail_return; }

            node.queued = false;
            analyze(node, scope, ast);
        }

        ref main_fn = add_fn(main_macro, true, ast);
        expand(main_fn, ast);
    }

    namespace analysis {
        using namespace swap_queuess;
        using namespace wasm_emit;
        using enum type_t;
        using enum node::lex_kind_t;
        using enum node::sem_kind_t;
        using enum node::stage_t;
        using enum local::kind_t;

        void analyze_chain(node* first_node, lex_scope& scope, ast& ast) {
            for_chain(first_node)
                analyze(*it, scope, ast);
        }

        void analyze(node& node, lex_scope& scope, ast& ast) {
            if (!node.queued); else return;

            ref stage = node.stage;

            if (stage == ns_scoping) {
                node.scope = &scope;
                node.stage = ns_analysis;
            }

            if (stage == ns_analysis); else return;

            if (node.is_string) {
                node.str_data_offset = add_data(node.text, ast);
                complete(node, sk_lit, t_str);
                return;
            }
            if (node.can_be_uint ) { complete(node, sk_lit, t_u32); return; }
            if (node.can_be_int  ) { complete(node, sk_lit, t_i32); return; }
            if (node.can_be_float) { complete(node, sk_lit, t_f32); return; }

            if (node.id == bi_list_id) {
                analyze_chain(node.child_chain, scope, ast);
                complete(node, sk_list, t_void);
                return;
            }

            if (node.id == bi_scope_id) {
                if_ref(macro, scope.macro); else { dbg_fail_return; }
                ref new_scope = add_scope(macro, &scope, node.child_chain, ast);
                analyze_chain(node.child_chain, new_scope, ast);
                complete(node, sk_scope, t_void);
                return;
            }

            if (node.id == bi_def_id) {
                node.decled_macros = make_arr_dyn<asts::macro*>(128, ast.data_arena);

                for_chain(node.child_chain) {
                    if_ref(id_node, it); else { dbg_fail_return; }
                    if_chain3(params_node, results_node, body_node, id_node.child_chain); else { dbg_fail_return; }

                    ref decl_macro = add_macro(id_node.id, node.scope, &body_node, ast);

                    for_chain2(prm_it, params_node.child_chain)
                        gather_param(*prm_it, decl_macro);

                    if_set1(decl_macro.result_type, find_type(results_node.id)); else decl_macro.result_type = t_void;

                    push(node.decled_macros, &decl_macro);

                    analyze_chain(&body_node, *decl_macro.body_scope, ast);
                }

                complete(node, sk_macro_decl, t_void);
                return;
            }

            if (node.id == bi_ret_id) {
                analyze_chain(node.child_chain, scope, ast);

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
                analyze_chain(node.child_chain, scope, ast);

                for_chain(node.child_chain) {
                    ref arg = *it;
                    if (arg.sem_kind == sk_lit && arg.value_type == t_u32); else { printf("Only u32 literals are supported as arguments for opcodes.\n"); node_error(arg); return; }
                    complete(arg, sk_lit, t_u32);
                }

                node.wasm_op = wasm_op;
                complete(node, sk_wasm_op, t_void);
                return;
            }

            if_ref(refed_local, find_local(node.id, scope)) {
                let sem_kind = refed_local.kind == lk_code ? sk_code_embed : sk_local_get;
                node.refed_local = &refed_local;
                complete(node, sem_kind, refed_local.value_type);
                return;
            }

            if (node.id == bi_decl_local_id) {
                if_chain2(id_node, type_node, node.child_chain); else { dbg_fail_return; }
                if_var1  (type, find_type(type_node.id)); else { dbg_fail_return; }
                let init_node_p = type_node.next;

                if_ref(init_node, init_node_p)
                    analyze(init_node, scope, ast); //TODO: check if types are compatible

                ref local         = add_local(id_node.id, lk_value, type, scope);
                node.decled_local = &local;
                node.init_node    = init_node_p;
                complete(node, sk_local_decl, type);
                return;
            }

            if (node.id == bi_ref_id) {
                if_ref(id_node, node.child_chain); else { dbg_fail_return; }
                if_ref(local , find_local(id_node.id, scope)); else { dbg_fail_return; }
                if (local.kind == lk_ref); else { printf("Only mutable variables can be referenced.\n"); node_error(node); return; }
                node.refed_local = &local;
                complete(node, sk_local_ref, local.value_type);
                return;
            }

            if (node.id == bi_chr_id) {
                if_ref(value_node, node.child_chain); else { dbg_fail_return; }
                complete(value_node, sk_id, t_void);
                node.chr_value = (u8)value_node.text[0];
                complete(node, sk_chr, t_u32);  // TODO: t_u8
                return;
            }

            if (node.id == bi_sub_id) {
                if_chain2(index_node, sub_node, node.child_chain); else { dbg_fail_return; }
                if_var1  (idx, get_uint(index_node)); else { dbg_fail_return; }
                analyze(index_node, scope, ast);
                if (index_node.sem_kind == sk_lit); else { printf("Only literals can be used as sub-of.\n"); node_error(node); return; }

                analyze(sub_node, scope, ast);
                if (sub_node.sem_kind == sk_local_get); else { printf("Only local variables can be used as sub-of.\n"); node_error(node); return; }

                let size = size_32_of(sub_node.value_type);
                if (idx < size); else { printf("Index %u is out of bounds for type %s.\n", idx, sub_node.text.data); node_error(node); return; }

                node.sub_index = index_node.uint_value;
                node.sub_node  = &sub_node;
                complete(node, sk_sub, t_u32);
                return;
            }

            if (node.id == bi_varargs_id) {
                if_chain2(list_node, body_node, node.child_chain); else { dbg_fail_return; }
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
                var has_arg_of_type_any = false;
                var arg_types = make_arr_dyn<type_t>(4, ast.data_arena);
                for_chain(node.child_chain) {
                    ref arg = *it;
                    analyze(arg, scope, ast);
                    if (arg.value_type == t_any)
                        has_arg_of_type_any = true;
                    push(arg_types, arg.value_type);
                }

                if_ref(invoked_macro, find_macro(node.id, arg_types.data, scope)) {
                    node.refed_macro = &invoked_macro;
                    complete(node, sk_macro_expand, invoked_macro.result_type);
                    return;
                }

                if_ref(called_fn, find_fn(node.id, arg_types.data, ast)) {
                    node.refed_fn = &called_fn;
                    complete(node, sk_fn_call, called_fn.result_type);
                    return;
                }

                if (has_arg_of_type_any) {
                    complete(node, sk_macro_expand, t_any);
                    return;
                }
            }

            // could not analyze yet, queue for later
            enqueue(node, ast);
            //printf("BAM!!! Queued: %.*s\n", (int)node.text.count, node.text.data);
        }

        void gather_param(node& prm_node, macro& macro) {
            if_chain2(prm_kind_node, prm_type_node, prm_node.child_chain); else { dbg_fail_return; }

            let is_ref  = prm_kind_node.id == bi_ref_id;
            let is_val  = prm_kind_node.id == bi_in_id;
            let is_code = prm_kind_node.id == bi_code_id;
            if (is_ref || is_val || is_code); else { dbg_fail_return; }

            if_var1(value_type, find_type(prm_type_node.id)); else value_type = t_void;

            //TODO: support other kinds
            let kind = is_ref ? lk_ref : is_val ? lk_value : lk_code;
            add_param(prm_node.id, kind, value_type, macro);
            prm_node.value_type = value_type;
        }

        void complete(node& node, node::sem_kind_t kind, type_t type) {
            node.sem_kind   = kind;
            node.value_type = type;
            node.stage      = ns_complete;
        }

        void enqueue(node& node, ast& ast) {
            assert(!node.queued);
            node.queued = true;
            push(ast.pipeline, &node);
        }

        void expand(fn& fn, ast& ast) {
            if_ref(macro, fn.macro); else { dbg_fail_return; }
            ref exp = add_expansion(fn, macro, nullptr, ast);
            fn.expansion = &exp;
            bind_fn_params(exp);

            if_ref(scope, macro.body_scope); else { dbg_fail_return; }
            exp.generated_chain = expand_chain(scope.chain, nullptr, exp, ast);
        }

        auto expand_chain(node* chain, node* parent_p, expansion& exp, ast& ast) -> node* {
            var first_node = (node*)nullptr;
            var  prev_node = (node*)nullptr;

            for_chain(chain) {
                if_ref(node, expand(*it, parent_p, exp, ast)); else continue;

                if (prev_node) prev_node->next = &node;
                else first_node = &node;

                prev_node = &node;
            }

            return first_node;
        }

        auto expand(node& src_node, node* parent_p, expansion& exp, ast& ast) -> node* {
            if (src_node.exp == nullptr); else { dbg_fail_return nullptr; }
            if (src_node.sem_kind != sk_macro_decl); else return nullptr;

            ref node = add_node(src_node, ast);

            node.parent      = parent_p;
            node.next        = nullptr;
            node.child_chain = nullptr;
            node.exp         = &exp;

            switch (src_node.sem_kind) {
                case sk_ret: {
                    if_ref(scope, node.scope); else { node_error(node); break; }
                    // TODO: calculate depth here
                    node.child_chain = expand_chain(src_node.child_chain, &node, exp, ast);
                    break;
                }
                case sk_local_decl: {
                    if_ref(loc, node.decled_local); else { node_error(node); break; }
                    node.decl_local_index_in_fn = add_and_bind_fn_local(loc.value_type, exp);
                    if_ref(init_node, node.init_node); else { node_error(node); break; }
                    node.init_node = expand(init_node, &node, exp, ast);
                    break;
                }
                case sk_local_get:
                case sk_local_ref: {
                    if_ref(local, node.refed_local); else { node_error(node); break; }
                    node.local_index_in_fn = find_local_index(local.index_in_macro, exp);
                    break;
                }
                case sk_code_embed: {
                    if_ref(local, node.refed_local); else { node_error(node); break; }
                    if_ref(code, find_code_node(local.index_in_macro, exp)); else {
                        printf("Failed to find code param %u\n", local.index_in_macro);
                        print_exp_bindings(exp, ast);
                        node_error(node);
                        break;
                    }

                    // We embed the code in the context of parent expansion.
                    if_ref(parent_exp, exp.parent); else { node_error(node); break; }
                    node.child_chain = expand(code, &node, parent_exp, ast);
                    break;
                }
                case sk_macro_expand: {
                    // bind arguments and embed the function
                    if_ref(refed_macro, src_node.refed_macro); else {
                        //TODO: find generic macro
                        node_error(src_node); break;
                    }

                    if_ref (fn, exp.fn); else { node_error(node); return nullptr; }
                    ref new_exp = add_expansion(fn, refed_macro, &exp, ast);

                    let params = params_of(refed_macro);
                    var arg_node_p = src_node.child_chain;
                    for_arr(params) {
                        defer { arg_node_p = arg_node_p->next; };

                        let param = params[i];
                        if_ref(arg_node, arg_node_p); else { node_error(src_node); break; }

                        if (param.kind == lk_code) {
                            bind(&arg_node, new_exp);
                            continue;
                        }

                        if (arg_node.sem_kind == sk_local_get && param.kind != lk_code) {
                            if_ref(refed_local, arg_node.refed_local); else { node_error(src_node); break; }
                            let fn_index = find_local_index(refed_local.index_in_macro, exp);
                            bind(fn_index, new_exp);
                            continue;
                        }

                        if (arg_node.sem_kind != sk_local_get && param.kind == lk_value) {
                            if_ref(exp_arg_node, expand(arg_node, parent_p, exp, ast)); else { node_error(src_node); break; }
                            let type = arg_node.value_type;
                            add_and_bind_fn_local(type, new_exp);
                            last_of(new_exp.bindings)->init = &exp_arg_node;
                            continue;
                        }

                        node_error(src_node); break;
                    }

                    if_ref(body_scope, refed_macro.body_scope); else { node_error(src_node); break; }
                    node.macro_expansion = &new_exp;
                    new_exp.source_node = &node;
                    new_exp.generated_chain = expand_chain(body_scope.chain, parent_p, new_exp, ast);
                    break;
                }
                case sk_sub: {
                    if_ref(n, node.sub_node); else { node_error(node); break; }
                    if_ref(l, n.refed_local); else { node_error(node); break; }
                    node.sub_fn_offset = find_local_offset(l.index_in_macro, exp) + node.sub_index;
                    break;
                }

                default: {
                    node.child_chain = expand_chain(src_node.child_chain, &node, exp, ast);
                    break;
                }
            }

            return &node;
        }
    }
}

#pragma clang diagnostic pop

#endif //FRANCA2_ASTS_ANALYSIS_H
