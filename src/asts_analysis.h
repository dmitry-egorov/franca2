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
        using enum prim_type;

        var ctx = context {
            .ast = ast
        };

        add_import("env", "print_str", { pt_u32, pt_u32 }, { }, ast);

        ref macro_main = add_macro("main", nullptr, ast.root, ast);
        add_result(pt_u32, macro_main);

        add_fn(macro_main, true, ast);

        if_ref(scope, macro_main.body_scope); else { dbg_fail_return; }

        gather_scopes_and_defs(scope, ctx);
        type_check            (scope, ctx);
    }

    namespace analysis {
        using namespace wasm_emit;
        using enum node::type_t;
        using enum node::sem_kind_t;
        using enum local::kind_t;

        void gather_scopes_and_defs(scope& scope, context& ctx) {
            gather_scopes_and_defs_chain(scope.body_chain, scope, ctx);
        }

        void gather_scopes_and_defs_chain(node* first_node, scope& scope, context & ctx) {
            for (var node_p = first_node; node_p; node_p = node_p->next)
                gather_scopes_and_defs(*node_p, scope, ctx);
        }

        void gather_scopes_and_defs(node& node, scope& scope, context & ctx) {
            node.parent_scope = &scope;

            if (is_func(node, def_id)) {
                if_var4(id_node, disp_node, type_node, body_node, deref4(node.first_child)); else { dbg_fail_return; }
                var body_chain = is_func(body_node, block_id) ? body_node.first_child : &body_node;

                ref macro = add_macro(id_node.text, node.parent_scope, body_chain, ctx.ast);
                var prm_node_p = disp_node.first_child;
                for (; prm_node_p; prm_node_p = prm_node_p->next) {
                    ref prm_node = *prm_node_p;
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

                add_result(primitive_type_by(type_node.text), macro);

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
            for (var node_p = first_node; node_p; node_p = node_p->next)
                type_check(*node_p, ctx);
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

            if (node.type == literal) {
                if (node.text_is_quoted) { node.sem_kind = sk_str_lit; node.value_type = pt_str; return; }
                if (node.can_be_uint   ) { node.sem_kind = sk_num_lit; node.value_type = pt_u32; return; }
                if (node.can_be_int    ) { node.sem_kind = sk_num_lit; node.value_type = pt_i32; return; }
                if (node.can_be_float  ) { node.sem_kind = sk_num_lit; node.value_type = pt_f32; return; }
            }

            let wasm_type = find_value_type(node.text);
            if (wasm_type != vt_invalid) {
                node.sem_kind   = sk_wasm_type;
                node.wasm_type  = wasm_type;
                node.value_type = pt_void;
                return;
            }

            let wasm_op = find_op(node.text);
            if (wasm_op != op_invalid) {
                node.sem_kind   = sk_wasm_op;
                node.wasm_op    = wasm_op;
                node.value_type = pt_void;
                if (is_func(node)) {
                    for (var arg_p = node.first_child; arg_p; arg_p = arg_p->next) {
                        ref arg = *arg_p;
                        if (is_uint_literal(arg)); else { printf("Only numeric literals are supported as arguments for opcodes.\n"); node_error(arg); return; }
                        arg.sem_kind   = sk_num_lit;
                        arg.value_type = pt_u32;
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
                if_var1(node_id, get_id(node)); else { dbg_fail_return; }
                var args_p = node.first_child;

                if (node_id == block_id) {
                    node.sem_kind   = sk_block;
                    node.value_type = pt_void;
                    type_check_chain(args_p, ctx);
                    return;
                }

                if (node_id == ret_id) {
                    if_ref(value, args_p); else { printf("'Return' requires a parameter."); dbg_fail_return; }
                    type_check(value, ctx);
                    node.sem_kind   = sk_ret;
                    node.value_type = pt_void;
                    macro.has_returns = true;
                    return;
                }

                if (node_id == decl_local_id) {
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
                    node.value_type   = type;
                    return;
                }

                if (node_id == ref_id) {
                    if_ref (var_id, node.first_child); else { dbg_fail_return; }
                    if_ref (refed_local, find_local(var_id.text, scope)); else { dbg_fail_return; }
                    if (refed_local.kind == lk_ref); else { printf("Only mutable variables can be referenced.\n"); node_error(node); return; }
                    node.sem_kind   = sk_local_ref;
                    node.local      = &refed_local;
                    node.value_type =  refed_local.value_type;
                    return;
                }

                // macro invocation
                {
                    var arg_types = make_arr_dyn<prim_type>(4, ctx.ast.data_arena);
                    var arg_p = args_p;
                    for (; arg_p; arg_p = arg_p->next) {
                        ref arg = *arg_p;
                        type_check(arg, ctx);
                        push(arg_types, arg.value_type);
                    }

                    if_ref(invoked_macro, find_macro(node_id, arg_types.data, scope)) {
                        node.sem_kind    = sk_macro_invoke;
                        node.refed_macro = &invoked_macro;
                        node.value_type  =  invoked_macro.results.data[0];
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
