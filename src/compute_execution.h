#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_EXECUTION_H
#define FRANCA2_COMPUTE_EXECUTION_H

#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/arena_arrays.h"
#include "utility/iterators.h"
#include "utility/strings.h"
#include "code_views.h"
#include "compute_asts.h"
#include "compute_storage.h"

namespace compute_asts {
    uint execute(const ast& ast, arena& arena = gta);

    namespace execution {
        using namespace iterators;
        using namespace compute_asts;
        using namespace code_views;
        using enum builtin_func_id;
        
        struct exec_result {
            poly_value value;
            bool is_returning;
        };

        struct context {
            storage& storage;
            arena  & arena;
        };
#define using_exec_ctx ref [storage, arena] = ctx
#define bop_lambda(op) [](uint a, uint b) { return (uint)(a op b); }

        auto execute_nodes_chain(node*, context&) -> exec_result;
        auto execute_node       (node&, context&) -> exec_result;

        auto execute_block    (node*, context&) -> exec_result;
        auto execute_decl_var (node*, context&) -> exec_result;
        auto execute_decl_func(node*, context&) -> void       ;
        auto execute_var_ref  (node*, context&) -> poly_value ;
        auto execute_return   (node*, context&) -> exec_result;
        auto execute_if       (node*, context&) -> exec_result;
        auto execute_loop     (node*, context&) -> exec_result;
        auto execute_op_assign(node*, context&) -> exec_result;
        auto execute_bop      (node *, uint (*f)(uint, uint), context &) -> exec_result;
        auto execute_istring  (node*, context&) -> exec_result;
        auto execute_fn_print (node*, context&) -> exec_result;

        auto get_literal_value(node&) -> poly_value;

        void push_uint (arena_array<char>&, uint, arena&);
        void push_value(arena_array<char>&, const poly_value&, arena&);

        auto to_res(poly_value value) -> exec_result;
        auto to_res(uint value) -> exec_result;
        auto to_res(const string& value) -> exec_result;
    }

    uint execute(const ast& ast, arenas::arena& arena) {
        var storage = make_storage(arena);
        var ctx = execution::context {.storage = storage, .arena = arena};
        var r = execute_nodes_chain(ast.root, ctx);
        if_var1(i, get_uint(r.value)); else return 0xffffffff;
        return i;
    }

    namespace execution {
        using enum node::type_t;
        using enum poly_value::type_t;

        exec_result execute_nodes_chain(node* chain, context& ctx) {
            var result = exec_result {};

            for(var node = chain; node && !result.is_returning; node = node->next)
                result = execute_node(*node, ctx);

            return result;
        }

        exec_result execute_node(node& node, context& ctx) {
            using_exec_ctx;

            switch (node.type) {
                case literal: return to_res(get_literal_value(node));
                case func   : {
                    var fn_id = node.text;
                    var args_node_p = node.first_child;

                    //if (fn_id == (uint)inactive  ) return {};
                    if (fn_id ==  block_id) return execute_block    (args_node_p, ctx);
                    if (fn_id == decl_var_id) return execute_decl_var (args_node_p, ctx);
                    if (fn_id ==    def_id) { execute_decl_func(args_node_p, ctx); return {}; }
                    if (fn_id ==    ref_id) return to_res(execute_var_ref(args_node_p, ctx));
                    if (fn_id ==    ret_id) return execute_return   (args_node_p, ctx);
                    if (fn_id ==     if_id) return execute_if       (args_node_p, ctx);
                    if (fn_id ==  while_id) return execute_loop     (args_node_p, ctx);
                    if (fn_id ==   istr_id) return execute_istring  (args_node_p, ctx);
                    if (fn_id == assign_id) return execute_op_assign(args_node_p, ctx);
                    if (fn_id ==    add_id) return execute_bop(args_node_p, bop_lambda(+) , ctx);
                    if (fn_id ==    sub_id) return execute_bop(args_node_p, bop_lambda(-) , ctx);
                    if (fn_id ==     eq_id) return execute_bop(args_node_p, bop_lambda(==), ctx);
                    if (fn_id ==    neq_id) return execute_bop(args_node_p, bop_lambda(!=), ctx);
                    if (fn_id ==    lte_id) return execute_bop(args_node_p, bop_lambda(<=), ctx);
                    if (fn_id ==    gte_id) return execute_bop(args_node_p, bop_lambda(>=), ctx);
                    if (fn_id ==     lt_id) return execute_bop(args_node_p, bop_lambda(<) , ctx);
                    if (fn_id ==     gt_id) return execute_bop(args_node_p, bop_lambda(>) , ctx);
                    if (fn_id ==  print_id) return execute_fn_print (args_node_p, ctx);
                    if (fn_id ==   show_id) return {}; // todo: implement

                    if_ref(fn, find_func(storage, fn_id)); else { printf("func %.*s not found!\n", (int)fn_id.count, fn_id.data); dbg_fail_return {}; }
                    if_ref(display, fn.display)          ; else { dbg_fail_return {}; }
                    if_ref(   body, fn.body   )          ; else { dbg_fail_return {}; }

                    tmp_scope(storage);

                    //TODO: support evaluating signature?
                    var sig_node_p = display.first_child;
                    
                    // bind arguments
                    while(sig_node_p) {
                        ref sig_node = *sig_node_p;
                        if (sig_node.type == func) {
                            assert(is_func(sig_node, view_of("$")));
                            if_ref(arg_node, args_node_p); else { dbg_fail_return {}; }
                            if_ref(param_id_node, sig_node.first_child); else { dbg_fail_return {}; }
                            var id = param_id_node.text;
                            var name = id;
                            if_ref(param_name_node, param_id_node.next) { name = param_name_node.text;}
                            let arg_res = execute_node(arg_node, ctx);
                            if (arg_res.is_returning) return arg_res;

                            push_var(storage, {id, name, arg_res.value});

                            args_node_p = arg_node.next;
                        }

                        sig_node_p = sig_node.next;
                    }

                    var result = execute_node(body, ctx);
                    result.is_returning = false;
                    return result;
                }
                default: { dbg_fail_return {}; } // unreachable
            }
        }

        exec_result execute_block(node* args, context& ctx) {
            using_exec_ctx;

            tmp_scope(storage);
            return execute_nodes_chain(args, ctx);
        }

        exec_result execute_decl_var(node* args, context& ctx) {
            using_exec_ctx;

            if_ref(id_node, args); else { dbg_fail_return {}; }

            var id   = id_node.text;
            var name = id;

            var initial_value = poly_value {};
            if_ref(init_node, id_node.next) {
                let r = execute_node(init_node, ctx);
                if (r.is_returning) return r;
                initial_value = r.value;

                if_ref(name_node, init_node.next) {
                    name = name_node.text;
                }
            }

            var v = variable {
                .id = id,
                .display_name = name,
                .value = initial_value
            };

            push_var(storage, v);

            return to_res(v.value);
        }

        void execute_decl_func(node* args, context& ctx) {
            using_exec_ctx;

            if_var3(id_node, disp_node, body_node, deref_list3(args)); else { dbg_fail_return; }
            var id = id_node.text;

            push_func(storage, compute_asts::func { id, &disp_node, &body_node });
        }

        poly_value execute_var_ref(node* args, context& ctx) {
            using_exec_ctx;

            if_ref(id_node , args); else { dbg_fail_return {}; }
            var id = id_node.text;
            if_ref(variable, find_var(storage, id)); else { dbg_fail_return {}; }

            return variable.value;
        }

        exec_result execute_return(node* args, context& ctx) {
            var r = execute_nodes_chain(args, ctx);
            if (r.is_returning) return r;
            r.is_returning = true;
            return r;
        }

        exec_result execute_if(node* args, context& ctx) {
            using_exec_ctx;
            if_var2(cond_node, body_node, deref_list2(args)); else { dbg_fail_return {}; }

            let cond_r = execute_node(cond_node, ctx);
            if (cond_r.is_returning) return cond_r;

            if_var1(cond, get_uint(cond_r.value)); else { dbg_fail_return {}; }

            if (cond) {
                tmp_scope(storage);
                return execute_node(body_node, ctx);
            }

            return {};
        }

        exec_result execute_loop(node* args, context& ctx) {
            using_exec_ctx;
            if_var2(cond_node, body_node, deref_list2(args)); else { dbg_fail_return {}; }
            var last_result = exec_result {};

            while (true) {
                let cond_r = execute_node(cond_node, ctx);
                if (cond_r.is_returning) return cond_r;

                if_var1(cond, get_uint(cond_r.value)); else { dbg_fail_return {}; }
                if (!cond)
                    break;

                tmp_scope(storage);
                last_result = execute_node(body_node, ctx);
                if (last_result.is_returning) break;
            }

            return last_result;
        }

        exec_result execute_op_assign(node* args, context& ctx) {
            using_exec_ctx;

            if_ref(id_node, args); else { dbg_fail_return {}; }
            if_ref(init_node, id_node.next); else { dbg_fail_return {}; }
            var id = id_node.text;

            if_ref(variable, find_var(storage, id)); else { dbg_fail_return {}; }
            var r = execute_node(init_node, ctx);
            if (!r.is_returning)
                variable.value = r.value;
            return r;
        }

        exec_result execute_bop(node * args, uint (*f)(uint, uint), context & ctx) {
            if_ref(a_node, args       ); else { dbg_fail_return {}; }
            if_ref(b_node, a_node.next); else { dbg_fail_return {}; }

            let a_r = execute_node(a_node, ctx);
            if (a_r.is_returning) return a_r;
            let b_r = execute_node(b_node, ctx);
            if (b_r.is_returning) return b_r;

            let a_value = a_r.value;
            let b_value = b_r.value;

            if (a_value.type == poly_integer && b_value.type == poly_integer); else { dbg_fail_return {}; }

            return to_res(f(a_value.integer, b_value.integer));
        }

        exec_result execute_istring(node* nodes, context& ctx) {
            using_exec_ctx;

            var result_storage = make_arena_array<char>(16, arena, 1);

            for (var node = nodes; node; node = node->next) {
                let r = execute_node(*node, ctx);
                if (r.is_returning) return r;
                push_value(result_storage, r.value, arena);
            }

            return to_res(result_storage.data);
        }

        exec_result execute_fn_print(node* args, context& ctx) {
            using_exec_ctx;
            if_ref(text_node, args); else { dbg_fail_return {}; }
            let r = execute_node(text_node, ctx);
            if (r.is_returning) return r;

            let text = r.value;

            var result_str = string {};
            if_var1(i, get_uint(text)) { result_str = make_string(arena, "%d", i); } else {
            if_var1(s, get_str (text)) { result_str = s; }
                else { result_str = view_of(""); assert(false); }}

            print(result_str);
            printf("\n");

            return to_res(result_str);
        }

        poly_value get_literal_value(node& node) {
            assert(node.type == literal);
            if (node.can_be_uint) return to_poly(node.uint_value);
            return to_poly(node.text);
        }

        void push_uint(arena_array<char>& storage, const uint value, arena& arena) {
            push(storage, make_string(arena, "%d", value));
        }

        void push_value(arena_array<char>& storage, const poly_value& value, arena& arena) {
            switch (value.type) {
                case poly_value::type_t::poly_integer: {
                    var text = make_string(arena, "%d", value.integer);
                    push(storage, text);
                    break;
                }
                case poly_value::type_t::poly_string: {
                    push(storage, value.string);
                    break;
                }
            }
        }

        exec_result to_res(poly_value value) { return { .value = value }; }
        exec_result to_res(uint value) { return to_res(to_poly(value)); }
        exec_result to_res(const string& value) { return to_res(to_poly(value));  }
    }
}

#endif //FRANCA2_COMPUTE_EXECUTION_H

#pragma clang diagnostic pop
