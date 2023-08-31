#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_INTERPRETATION_H
#define FRANCA2_COMPUTE_INTERPRETATION_H

#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/arr_dyns.h"
#include "utility/iterators.h"
#include "utility/strings.h"
#include "code_views.h"
#include "compute_asts.h"
#include "compute_storage.h"

namespace compute_asts {
    uint interpret(const ast& ast, arena& arena = gta);

    namespace interpretation {
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
            arr_view<u8> memory;
            arena  & arena;
        };
#define using_exec_ctx ref [storage, memory, arena] = ctx
#define bop_lambda(op) [](uint a, uint b) { return (uint)(a op b); }

        auto interpret_nodes_chain(node*, context&) -> exec_result;
        auto interpret_node       (node&, context&) -> exec_result;

        auto gather_defs(node&, context&) -> void;

        auto interpret_block    (node*, context&) -> exec_result;
        auto interpret_decl_local (node *, context &) -> exec_result;
        auto interpret_var_ref  (node*, context&) -> poly_value ;
        auto interpret_return   (node*, context&) -> exec_result;
        auto interpret_as       (node*, context&) -> exec_result;
        auto interpret_if       (node*, context&) -> exec_result;
        auto interpret_loop     (node*, context&) -> exec_result;
        auto interpret_store8   (node*, context&) -> exec_result;
        auto interpret_op_assign(node*, context&) -> exec_result;
        auto interpret_minus    (node*, context&) -> exec_result;
        auto interpret_bop      (node*, uint (*f)(uint, uint), context&) -> exec_result;
        auto interpret_bop_a    (node*, uint (*f)(uint, uint), context&) -> exec_result;
        auto interpret_istr     (node*, context&) -> exec_result;
        auto interpret_chr      (node*, context&) -> exec_result;
        auto interpret_fn_print (node*, context&) -> exec_result;

        auto get_literal_value(node&) -> poly_value;

        void push_uint (dstring&, uint);
        void push_value(dstring&, const poly_value&);

        auto to_res(poly_value value) -> exec_result;
        auto to_res(uint value) -> exec_result;
        auto to_res(const string& value) -> exec_result;
    }

    uint interpret(const ast& ast, arena& arena) {
        var storage = make_storage(arena);
        var memory  = alloc<u8>(arena, 64 * 1024);
        var ctx = interpretation::context {.storage = storage, .memory = memory, .arena = arena};
        var r = interpret_nodes_chain(ast.root, ctx);
        if_var1(i, get_uint(r.value)); else return 0xffffffff;
        return i;
    }

    namespace interpretation {
        using enum node::type_t;
        using enum poly_value::type_t;

        exec_result interpret_nodes_chain(node* chain, context& ctx) {
            var result = exec_result {};

            for(var node = chain; node && !result.is_returning; node = node->next)
                gather_defs(*node, ctx);

            for(var node = chain; node && !result.is_returning; node = node->next)
                result = interpret_node(*node, ctx);

            return result;
        }

        exec_result interpret_node(node& node, context& ctx) {
            using_exec_ctx;

            switch (node.type) {
                case literal: return to_res(get_literal_value(node));
                case func   : {
                    var fn_id = node.text;
                    var args_node_p = node.first_child;

                    //if (fn_id == (uint)inactive  ) return {};
                    if (fn_id ==      block_id) return interpret_block     (args_node_p, ctx);
                    if (fn_id == decl_local_id) return interpret_decl_local(args_node_p, ctx);
                    if (fn_id ==        def_id) return {};
                    if (fn_id ==        ref_id) return to_res(interpret_var_ref(args_node_p, ctx));
                    if (fn_id ==        ret_id) return interpret_return   (args_node_p, ctx);
                    if (fn_id ==         as_id) return interpret_as       (args_node_p, ctx);
                    if (fn_id ==         if_id) return interpret_if       (args_node_p, ctx);
                    if (fn_id ==      while_id) return interpret_loop     (args_node_p, ctx);
                    if (fn_id ==       istr_id) return interpret_istr     (args_node_p, ctx);
                    if (fn_id ==        chr_id) return interpret_chr      (args_node_p, ctx);
                    if (fn_id ==   store_u8_id) return interpret_store8   (args_node_p, ctx);
                    if (fn_id ==     assign_id) return interpret_op_assign(args_node_p, ctx);
                    if (fn_id ==        sub_id) return interpret_minus    (args_node_p, ctx);
                    if (fn_id ==        add_id) return interpret_bop      (args_node_p, bop_lambda(+) , ctx);
                    if (fn_id ==        mul_id) return interpret_bop      (args_node_p, bop_lambda(*) , ctx);
                    if (fn_id ==        div_id) return interpret_bop      (args_node_p, bop_lambda(/) , ctx);
                    if (fn_id ==        rem_id) return interpret_bop      (args_node_p, bop_lambda(%) , ctx);
                    if (fn_id ==      add_a_id) return interpret_bop_a    (args_node_p, bop_lambda(+) , ctx);
                    if (fn_id ==      sub_a_id) return interpret_bop_a    (args_node_p, bop_lambda(-) , ctx);
                    if (fn_id ==      mul_a_id) return interpret_bop_a    (args_node_p, bop_lambda(*) , ctx);
                    if (fn_id ==      div_a_id) return interpret_bop_a    (args_node_p, bop_lambda(/) , ctx);
                    if (fn_id ==      rem_a_id) return interpret_bop_a    (args_node_p, bop_lambda(%) , ctx);
                    if (fn_id ==         eq_id) return interpret_bop      (args_node_p, bop_lambda(==), ctx);
                    if (fn_id ==         ne_id) return interpret_bop      (args_node_p, bop_lambda(!=), ctx);
                    if (fn_id ==         le_id) return interpret_bop      (args_node_p, bop_lambda(<=), ctx);
                    if (fn_id ==         ge_id) return interpret_bop      (args_node_p, bop_lambda(>=), ctx);
                    if (fn_id ==         lt_id) return interpret_bop      (args_node_p, bop_lambda(<) , ctx);
                    if (fn_id ==         gt_id) return interpret_bop      (args_node_p, bop_lambda(>) , ctx);
                    if (fn_id ==      print_id) return interpret_fn_print (args_node_p, ctx);
                    if (fn_id ==       show_id) return {};

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
                            assert(is_func(sig_node, view("$")));
                            if_ref(arg_node, args_node_p); else { dbg_fail_return {}; }
                            if_ref(param_id_node, sig_node.first_child); else { dbg_fail_return {}; }
                            var id = param_id_node.text;
                            var name = id;
                            if_ref(param_name_node, param_id_node.next) { name = param_name_node.text;}
                            let arg_res = interpret_node(arg_node, ctx);
                            if (arg_res.is_returning) return arg_res;

                            push_var(storage, {id, name, arg_res.value});

                            args_node_p = arg_node.next;
                        }

                        sig_node_p = sig_node.next;
                    }

                    var result = interpret_node(body, ctx);
                    result.is_returning = false;
                    return result;
                }
                default: { dbg_fail_return {}; } // unreachable
            }
        }

        void gather_defs(node& node, context& ctx) {
            using_exec_ctx;

            if (is_func(node, def_id)); else return;

            if_var4(id_node, disp_node, type_node, body_node, deref4(node.first_child)); else { dbg_fail_return; }
            var id = id_node.text;

            push_func(storage, { id, &disp_node, &type_node, &body_node });
        }

        exec_result interpret_block(node* args, context& ctx) {
            using_exec_ctx;

            tmp_scope(storage);
            return interpret_nodes_chain(args, ctx);
        }

        exec_result interpret_decl_local(node * args, context & ctx) {
            using_exec_ctx;

            if_var2(id_node, type_node, deref2(args)); else { dbg_fail_return {}; }

            var id   = id_node.text;
            var name = id;

            var initial_value = poly_value {};
            if_ref(init_node, type_node.next) {
                let r = interpret_node(init_node, ctx);
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

        poly_value interpret_var_ref(node* args, context& ctx) {
            using_exec_ctx;

            if_ref(id_node , args); else { dbg_fail_return {}; }
            var id = id_node.text;
            if_ref(variable, find_var(storage, id)); else { dbg_fail_return {}; }

            return variable.value;
        }

        exec_result interpret_return(node* args, context& ctx) {
            var r = interpret_nodes_chain(args, ctx);
            if (r.is_returning) return r;
            r.is_returning = true;
            return r;
        }

        exec_result interpret_as(node* args, context& ctx) {
            if_ref(value_node, args); else { dbg_fail_return {}; }
            return interpret_node(value_node, ctx);
        }

        exec_result interpret_if(node* args, context& ctx) {
            using_exec_ctx;
            if_var2(cond_node, body_node, deref2(args)); else { dbg_fail_return {}; }

            let cond_r = interpret_node(cond_node, ctx);
            if (cond_r.is_returning) return cond_r;

            if_var1(cond, get_uint(cond_r.value)); else { dbg_fail_return {}; }

            if (cond) {
                tmp_scope(storage);
                return interpret_node(body_node, ctx);
            }

            return {};
        }

        exec_result interpret_loop(node* args, context& ctx) {
            using_exec_ctx;
            if_var2(cond_node, body_node, deref2(args)); else { dbg_fail_return {}; }
            var last_result = exec_result {};

            while (true) {
                let cond_r = interpret_node(cond_node, ctx);
                if (cond_r.is_returning) return cond_r;

                if_var1(cond, get_uint(cond_r.value)); else { dbg_fail_return {}; }
                if (!cond)
                    break;

                tmp_scope(storage);
                last_result = interpret_node(body_node, ctx);
                if (last_result.is_returning) break;
            }

            return last_result;
        }

        exec_result interpret_store8(node* args, context& ctx) {
            using_exec_ctx;

            if_var2(mem_offset_node, value_node, deref2(args)); else { dbg_fail_return {}; }
            let mem_offset_r = interpret_node(mem_offset_node, ctx);
            if (mem_offset_r.is_returning) return mem_offset_r;
            if_var1(mem_offset, get_uint(mem_offset_r.value)); else { dbg_fail_return {}; }

            var val_r = interpret_node(value_node, ctx);
            if (val_r.is_returning) return val_r;
            if_var1(value, get_uint(val_r.value)); else { dbg_fail_return {}; }

            ctx.memory[mem_offset] = (u8)value;
            return {};
        }

        exec_result interpret_op_assign(node* args, context& ctx) {
            using_exec_ctx;

            if_ref(id_node, args); else { dbg_fail_return {}; }
            if_ref(init_node, id_node.next); else { dbg_fail_return {}; }
            var id = id_node.text;

            if_ref(variable, find_var(storage, id)); else { dbg_fail_return {}; }
            var r = interpret_node(init_node, ctx);
            if (!r.is_returning)
                variable.value = r.value;
            return r;
        }

        exec_result interpret_minus(node * args, context & ctx) {
            if_ref(a_node, args       ); else { dbg_fail_return {}; }
            let a_r = interpret_node(a_node, ctx);
            if (a_r.is_returning) return a_r;
            let a_value = a_r.value;
            if (a_value.type == poly_integer); else { dbg_fail_return {}; }

            if_ref(b_node, a_node.next); else {
                return to_res(-a_r.value.integer);
            }

            let b_r = interpret_node(b_node, ctx);
            if (b_r.is_returning) return b_r;

            let b_value = b_r.value;
            if (b_value.type == poly_integer); else { dbg_fail_return {}; }

            return to_res(a_value.integer - b_value.integer);
        }

        exec_result interpret_bop(node * args, uint (*f)(uint, uint), context & ctx) {
            if_ref(a_node, args       ); else { dbg_fail_return {}; }
            let a_r = interpret_node(a_node, ctx);
            if (a_r.is_returning) return a_r;

            if_ref(b_node, a_node.next); else { dbg_fail_return {}; }
            let b_r = interpret_node(b_node, ctx);
            if (b_r.is_returning) return b_r;

            let a_value = a_r.value;
            let b_value = b_r.value;

            if (a_value.type == poly_integer && b_value.type == poly_integer); else { dbg_fail_return {}; }

            return to_res(f(a_value.integer, b_value.integer));
        }

        exec_result interpret_bop_a(node* args, uint (*f)(uint, uint), context& ctx) {
            using_exec_ctx;

            if_ref(id_node, args); else { dbg_fail_return {}; }
            if_ref(init_node, id_node.next); else { dbg_fail_return {}; }
            var id = id_node.text;

            if_ref(variable, find_var(storage, id)); else { dbg_fail_return {}; }
            if (variable.value.type == poly_integer); else { dbg_fail_return {}; }
            var r = interpret_node(init_node, ctx);
            if (r.value.type == poly_integer); else { dbg_fail_return {}; }
            if (r.is_returning) return r;
            variable.value = to_poly(f(variable.value.integer, r.value.integer));
            return r;
        }

        exec_result interpret_istr(node* nodes, context& ctx) {
            using_exec_ctx;

            var result_storage = make_string_builder(16, arena);

            for (var node = nodes; node; node = node->next) {
                let r = interpret_node(*node, ctx);
                if (r.is_returning) return r;
                push_value(result_storage, r.value);
            }

            return to_res(result_storage.data);
        }

        exec_result interpret_chr(node* args, context& ctx) {
            using_exec_ctx;

            if_ref(value_node, args); else { dbg_fail_return {}; }
            return to_res(sub(value_node.text, 1));
        }

        exec_result interpret_fn_print(node* args, context& ctx) {
            using_exec_ctx;
            let r = interpret_istr(args, ctx);
            if (r.is_returning) return r;

            let text = r.value;

            var result_str = string {};
            if_var1(i, get_uint(text)) { result_str = make_string(arena, "%u", i); } else {
            if_var1(s, get_str (text)) { result_str = s; }
                else { result_str = view(""); assert(false); }}

            print(result_str);
            printf("\n");

            return to_res(result_str);
        }

        poly_value get_literal_value(node& node) {
            assert(node.type == literal);
            if (node.can_be_uint) return to_poly(node.uint_value);
            return to_poly(node.text);
        }

        void push_uint(dstring& storage, const uint value) {
            push(storage, make_string(*storage.arena, "%u", value));
        }

        void push_value(dstring& storage, const poly_value& value) {
            switch (value.type) {
                case poly_value::type_t::poly_integer: {
                    var text = make_string(*storage.arena, "%u", value.integer);
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

#endif //FRANCA2_COMPUTE_INTERPRETATION_H

#pragma clang diagnostic pop
