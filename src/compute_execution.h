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

    namespace execution {
        using namespace iterators;
        using namespace compute_asts;
        using namespace code_views;
        using namespace strings;
        using enum builtin_func_id;

        struct context {
            const array_view<variable>& storage;
            arena& arena;
        };
#define using_exec_ctx ref [defs, arena] = ctx

        poly_value       execute_node   (const node*, context&);
        poly_value       execute_list   (const node*, context&);
        poly_value       execute_nodes_chain  (const node*, context&);

        ret1<poly_value> execute_func (uint fn_id, const node*, context&);

             poly_value  execute_decl_var (const node*, context&);
             poly_value  execute_var_ref  (const node*, context&);
             poly_value  execute_op_assign(const node*, context&);
                   uint  execute_op_plus  (const node*, context&);
        array_view<char> execute_istring  (const node*, context&);
        array_view<char> execute_fn_print (const node*, context&);

        poly_value get_literal_value(const node*);

        void push_uint (arena_array<char>&, uint, arena&);
        void push_value(arena_array<char>&, const poly_value&, arena&);
    }

    void execute(const ast& ast, const arrays::array_view<variable>& defs, arenas::arena& arena) {
        var ctx = execution::context {.storage = defs, .arena = arena};
        execute_nodes_chain(ast.root, ctx);
    }

    namespace execution {
        using enum node::type_t;
        using enum poly_value::type_t;

        poly_value execute_node(const node* node, context& ctx) {
            switch (node->type) {
                case literal: return get_literal_value(node);
                case list   : return execute_list(node, ctx);
                default: { dbg_fail_return {}; } // unreachable
            }
        }

        ret1<poly_value> execute_func(uint fn_id, const node* args, context& ctx) {
            if (fn_id == (uint)inactive ) return ret1_ok({});
            if (fn_id == (uint)decl_var ) return ret1_ok(execute_decl_var(args, ctx));
            if (fn_id == (uint)var_ref  ) return ret1_ok(execute_var_ref (args, ctx));
            if (fn_id == (uint)istring  ) return ret1_ok(to_poly(execute_istring  (args, ctx)));
            if (fn_id == (uint)op_assign) return ret1_ok(execute_op_assign(args, ctx));
            if (fn_id == (uint)op_plus  ) return ret1_ok(to_poly(execute_op_plus  (args, ctx)));
            if (fn_id == (uint)fn_print ) return ret1_ok(to_poly(execute_fn_print (args, ctx)));
            return ret1_fail;
        }

        array_view<char> execute_fn_print(const node* arguments, context& ctx) {
            using_exec_ctx;

            let value  = execute_node(arguments, ctx);

            var result = array_view<char> {};
            if_var1(i, get_int(value)) { result = make_string(arena, "%d", i); } else {
            if_var1(s, get_str(value)) { result = s; }
                else { assert(false); result = view_of(""); }}

            print(result);
            printf("\n");

            return result;
        }

        poly_value execute_decl_var(const node* args, context& ctx) {
            using_exec_ctx;

            if_var3(id_node, mut_node, name_node, deref_list3(args)); else { dbg_fail_return {}; }
            if_var1(id, get_int(id_node)    ); else { dbg_fail_return {}; }
            if_ref (variable, find(defs, id)); else { dbg_fail_return {}; }

            if_ref(value_node, name_node.next); else return {};
            return variable.value = execute_node(&value_node, ctx);
        }

        poly_value execute_var_ref(const node* args, context& ctx) {
            using_exec_ctx;

            if_ref (id_node , args)            ; else { dbg_fail_return {}; }
            if_var1(id      , get_int(id_node)); else { dbg_fail_return {}; }
            if_ref (variable, find(defs, id))  ; else { dbg_fail_return {}; }

            return variable.value;
        }

        poly_value  execute_op_assign(const node* args, context& ctx) {
            using_exec_ctx;

            if_ref(id_node, args); else { dbg_fail_return {}; }
            if_ref(value_node, id_node.next); else { dbg_fail_return {}; }

            if_ref(variable, find(defs, id_node.value.integer)); else { dbg_fail_return {}; }
            if   (variable.is_mutable); else { dbg_fail_return {}; }

            return variable.value = execute_node(&value_node, ctx);
        }

        uint execute_op_plus(const node* args, context& ctx) {
            if_ref(a_node, args       ); else { dbg_fail_return {}; }
            if_ref(b_node, a_node.next); else { dbg_fail_return {}; }

            let a_value = execute_node(&a_node, ctx);
            let b_value = execute_node(&b_node, ctx);

            if(a_value.type == integer && b_value.type == integer); else { dbg_fail_return {}; }

            return a_value.integer + b_value.integer;
        }

        poly_value execute_list(const node* node, context& ctx) {
            assert(node->type == list);
            if_ref(child, node->first_child) {
                if_var1(fn_id, get_int(child)) {
                    var [result, executed] = execute_func(fn_id, child.next, ctx);
                    if (executed) return result;
                }
            }

            return execute_nodes_chain(&child, ctx);
        }

        poly_value execute_nodes_chain(const node* chain, context& ctx) {
            var result = poly_value {};

            var node = chain;
            while(node) { defer {node = node->next; };
                result = execute_node(node, ctx);
            }

            return result;
        }

        array_view<char> execute_istring(const node* nodes, context& ctx) {
            using_exec_ctx;

            var result_storage = arena_array<char>{};
            ensure_capacity_for(result_storage, 16, arena, 1);

            loop_over_sequence(node, nodes) {
                switch (node->type) {
                    case literal: {
                        push_value(result_storage, get_literal_value(node), arena);
                        break;
                    }
                    case list: {
                        let value = execute_list(node, ctx);
                        push_value(result_storage, value, arena);
                        break;
                    }
                    default: { assert(false); } // unreachable
                }
            }

            return result_storage.data;
        }

        poly_value get_literal_value(const node* node) {
            assert(node->type == literal);
            if (is_int_literal(node)) return to_poly(node->value.integer);
            if (is_str_literal(node)) return to_poly(node->value.string );
            assert(false); // unreachable
        }

        void push_uint(arena_array<char>& storage, const uint value, arena& arena) {
            push(storage, make_string(arena, "%d", value), arena, 1);
        }

        void push_value(arena_array<char>& storage, const poly_value& value, arena& arena) {
            switch (value.type) {
                case poly_value::type_t::integer: {
                    var text = make_string(arena, "%d", value.integer);
                    push(storage, text, arena, 1);
                    break;
                }
                case poly_value::type_t::string: {
                    push(storage, value.string, arena, 1);
                    break;
                }
            }
        }
    }
}

#endif //FRANCA2_COMPUTE_EXECUTION_H

#pragma clang diagnostic pop
