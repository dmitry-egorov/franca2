#ifndef FRANCA2_COMPUTE_CHECKING_H
#define FRANCA2_COMPUTE_CHECKING_H

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#include "utility/strings.h"
#include "utility/wasm_emit.h"

namespace compute_asts {
    void analyze(ast&);

    namespace analysis {
        struct context {
            ast& ast;
        };

        void gather_scopes_and_defs      (scope&, context&);
        void gather_scopes_and_defs_chain(node* first_node, scope&, context&);
        void gather_scopes_and_defs      (node&, scope&, context &);

        void type_check      (scope&, context &);
        void type_check_chain(node* first_node, context &);
        void type_check      (node&, context &);
    }

    void analyze(ast& ast) {
        using namespace analysis;
        using enum type_t;

        var ctx = context {
            .ast = ast
        };

        add_import("env", "print_str", { t_str }, t_void, ast);

        ref macro_main = add_macro("main", nullptr, ast.root, ast);
        macro_main.result_type = t_u32;

        add_fn(macro_main, true, ast);

        if_ref(scope, macro_main.body_scope); else { dbg_fail_return; }

        gather_scopes_and_defs(scope, ctx);
        type_check            (scope, ctx);
    }

    namespace analysis {
        using namespace wasm_emit;
        using enum type_t;
        using enum node::lex_kind_t;
        using enum node::sem_kind_t;
        using enum local::kind_t;

        void gather_scopes_and_defs(scope& scope, context& ctx) {
            gather_scopes_and_defs_chain(scope.body_chain, scope, ctx);
        }

        void gather_scopes_and_defs_chain(node* first_node, scope& scope, context & ctx) {
            for_chain(first_node)
                gather_scopes_and_defs(*it, scope, ctx);
        }

        void gather_scopes_and_defs(node& node, scope& scope, context & ctx) {
            node.parent_scope = &scope;

            if (is_func(node, def_id)) {
                if_var4(id_node, disp_node, type_node, body_node, deref4(node.first_child)); else { dbg_fail_return; }
                var body_chain = is_func(body_node, block_id) ? body_node.first_child : &body_node;

                ref macro = add_macro(id_node.text, node.parent_scope, body_chain, ctx.ast);
                for_chain (disp_node.first_child) {
                    ref prm_node = *it;
                    if (!is_str_literal(prm_node)); else continue;

                    let is_ref  = is_func(prm_node,  ref_id);
                    let is_val  = is_func(prm_node,   in_id);
                    let is_code = is_func(prm_node, code_id);
                    if (is_ref || is_val || is_code); else { dbg_fail_return; }

                    if_var2(prm_id_node, prm_type_node, deref2(prm_node.first_child)); else { dbg_fail_return; }
                    let value_type = primitive_type_by(prm_type_node.text);
                    //TODO: support other kinds
                    let kind = is_ref ? lk_ref : is_val ? lk_value : lk_code;
                    add_param(prm_id_node.text, kind, value_type, macro);
                    prm_node.value_type = value_type;
                }

                macro.result_type = primitive_type_by(type_node.text);

                node.sem_kind     = sk_macro_decl;
                node.decled_macro = &macro;

                gather_scopes_and_defs_chain(body_chain, *macro.body_scope, ctx);
                return;
            }

            gather_scopes_and_defs_chain(node.first_child, scope, ctx);
        }

        void type_check(scope& scope, context& ctx) {
            type_check_chain(scope.body_chain, ctx);
        }

        void type_check_chain(node* first_node, context& ctx) {
            for_chain(first_node)
                type_check(*it, ctx);
        }

        void type_check(node& node, context& ctx) {
            if_ref(scope, node .parent_scope); else { dbg_fail_return; }
            if_ref(macro, scope.parent_macro); else { dbg_fail_return; }

            if (node.sem_kind == sk_macro_decl) {
                if_ref (decl_fn, node   .decled_macro); else { dbg_fail_return; }
                if_ref (body   , decl_fn.body_scope  ); else { dbg_fail_return; }
                type_check(body, ctx);
                return;
            }

            if (node.lex_kind == lk_leaf) {
                if (node.text_is_quoted) { node.sem_kind = sk_lit; node.value_type = t_str; return; }
                if (node.can_be_uint   ) { node.sem_kind = sk_lit; node.value_type = t_u32; return; }
                if (node.can_be_int    ) { node.sem_kind = sk_lit; node.value_type = t_i32; return; }
                if (node.can_be_float  ) { node.sem_kind = sk_lit; node.value_type = t_f32; return; }
            }

            let wasm_type = find_value_type(node.text);
            if (wasm_type != vt_invalid) {
                node.sem_kind   = sk_wasm_type;
                node.wasm_type  = wasm_type;
                node.value_type = t_void;
                return;
            }

            let wasm_op = find_op(node.text);
            if (wasm_op != op_invalid) {
                node.sem_kind   = sk_wasm_op;
                node.wasm_op    = wasm_op;
                node.value_type = t_void;
                if (is_func(node)) {
                    for (var arg_p = node.first_child; arg_p; arg_p = arg_p->next) {
                        ref arg = *arg_p;
                        if (is_uint_literal(arg)); else { printf("Only numeric literals are supported as arguments for opcodes.\n"); node_error(arg); return; }
                        arg.sem_kind   = sk_lit;
                        arg.value_type = t_u32;
                    }
                }
                return;
            }

            if_ref(local, find_local(node.text, scope)) {
                node.sem_kind   = local.kind == lk_code ? sk_code_embed : sk_local_get;
                node.local      = &local;
                node.value_type = local.value_type;
                return;
            }

            if (is_func(node)) {
                var args_p = node.first_child;

                if (node.text == block_id) {
                    node.sem_kind   = sk_block;
                    node.value_type = t_void;
                    type_check_chain(args_p, ctx);
                    return;
                }

                if (node.text == ret_id) {
                    for_chain(args_p)
                        type_check(*it, ctx);

                    node.sem_kind   = sk_ret;
                    node.value_type = t_void;
                    macro.has_returns = true;
                    return;
                }

                if (node.text == decl_local_id) {
                    if_var2(id_node, type_node, deref2(args_p)); else { dbg_fail_return; }
                    let type = primitive_type_by(type_node.text);
                    ref loc  = add_local(id_node.text, lk_value, type, scope);
                    if_ref(init_node, type_node.next) {
                        type_check(init_node, ctx);
                        //TODO: check if types are compatible
                    }
                    node.sem_kind     = sk_local_decl;
                    node.decled_local = &loc;
                    node.init_node    = &init_node;
                    node.value_type = type;
                    return;
                }

                if (node.text == ref_id) {
                    if_ref (var_id, node.first_child); else { dbg_fail_return; }
                    if_ref (refed_local, find_local(var_id.text, scope)); else { dbg_fail_return; }
                    if (refed_local.kind == lk_ref); else { printf("Only mutable variables can be referenced.\n"); node_error(node); return; }
                    node.sem_kind   = sk_local_ref;
                    node.local      = &refed_local;
                    node.value_type =  refed_local.value_type;
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
                    if_var1(idx, get_uint(sub_of_index)); else { dbg_fail_return; }

                    type_check(sub_of_node, ctx);

                    let size = size_32_of(sub_of_node.value_type);
                    if (idx < size); else { printf("Index %u is out of bounds for type %s.\n", idx, sub_of_node.text.data); node_error(node); return; }

                    node.sem_kind    = sk_sub_of;
                    node.sub_index   = sub_of_index.uint_value;
                    node.sub_of_node = &sub_of_node;
                    node.value_type  = t_u32;
                    return;
                }

                // macro invocation or function call
                {
                    var arg_types = make_arr_dyn<type_t>(4, ctx.ast.data_arena);
                    var arg_p = args_p;
                    for (; arg_p; arg_p = arg_p->next) {
                        ref arg = *arg_p;
                        type_check(arg, ctx);
                        push(arg_types, arg.value_type);
                    }

                    if_ref(invoked_macro, find_macro(node.text, arg_types.data, scope)) {
                        node.sem_kind    = sk_macro_invoke;
                        node.refed_macro = &invoked_macro;
                        node.value_type = invoked_macro.result_type;
                        return;
                    }

                    if_ref(called_fn, find_fn(node.text, arg_types.data, ctx.ast)) {
                        node.sem_kind   = sk_fn_call;
                        node.refed_fn   = ref_in(&called_fn, ctx.ast.fns.data);
                        node.value_type = called_fn.result_type;
                        return;
                    }
                }
            }

            printf("Undeclared identifier %.*s\n", (int)node.text.count, node.text.data); dbg_fail_return;
        }
    }
}

#pragma clang diagnostic pop

#endif //FRANCA2_COMPUTE_CHECKING_H
