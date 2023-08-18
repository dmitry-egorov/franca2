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
#include "compute_definitions.h"

namespace compute_asts {

    namespace execution {
        using namespace iterators;
        using namespace compute_asts;
        using namespace code_views;
        using namespace strings;

        struct context {
            const array_view<definition>& definitions;
            arena& arena;
        };
#define using_exec_ctx ref [defs, arena] = ctx

        struct poly_value {
            enum class type_t {
                integer,
                string
            } type;

            union {
                uint integer;
                array_view<char> string;
            };
        };

        poly_value       execute_node   (const node*, context&);
        poly_value       execute_list   (const node*, context&);
        poly_value       execute_nodes  (const node*, context&);
        array_view<char> execute_istring(const node*, context&);

        ret1<poly_value> execute_func (uint fn_id, const node*, context&);
        array_view<char> execute_print(const node*, context&);
             poly_value  execute_ref  (const node*, context&);
                   uint  execute_plus (const node*, context&);

        uint             get_int_literal_value(const node*);
        array_view<char> get_str_literal_value(const node*);

        poly_value to_poly(uint value);
        poly_value to_poly(const array_view<char>& value);

        void push_uint (arena_array<char>&, uint, arena&);
        void push_value(arena_array<char>&, const poly_value&, arena&);
    }

    void execute(const ast& ast, const arrays::array_view<definition>& defs, arenas::arena& arena) {
        var ctx = execution::context {.definitions = defs, .arena = arena};
        execute_nodes(ast.root, ctx);
    }

    namespace execution {
        using enum node_type;
        using enum poly_value::type_t;

        poly_value execute_node(const node* node, context& ctx) {
            switch (node->type) {
                case int_literal: return to_poly(get_int_literal_value(node));
                case str_literal: return to_poly(get_str_literal_value(node));
                case list       : return execute_list(node, ctx);
                default: { assert(false); return {}; } // unreachable
            }
        }

        ret1<poly_value> execute_func(uint fn_id, const node* args, context& ctx) {
            if (fn_id ==   3) return ret1_ok(execute_ref(args, ctx));
            if (fn_id ==   4) return ret1_ok(to_poly(execute_istring(args, ctx)));
            if (fn_id == 100) return ret1_ok(to_poly(execute_plus   (args, ctx)));
            if (fn_id == 200) return ret1_ok(to_poly(execute_print  (args, ctx)));
            return ret1_fail;
        }

        array_view<char> execute_print(const node* arguments, context& ctx) {
            using_exec_ctx;

            let value  = execute_node(arguments, ctx);
            var result =
                value.type == integer ? make_string(arena, "%d", value.integer) :
                value.type == string  ? value.string :
                                        view_of("");

            print(result);
            printf("\n");

            return result;
        }

        poly_value execute_ref(const node* arguments, context& ctx) {
            using_exec_ctx;

            let id_node = arguments;
            assert(id_node->type == int_literal);

            let id = id_node->int_value;
            chk_var1(define, find(defs, id)) else { return {}; }

            return execute_node(define.value_node, ctx);
        }

        uint execute_plus(const node* arguments, context& ctx) {
            let a_node = arguments;
            let b_node = a_node->next_sibling;

            let a_value = execute_node(a_node, ctx);
            let b_value = execute_node(b_node, ctx);

            assert(a_value.type == integer && b_value.type == integer);

            return a_value.integer + b_value.integer;
        }

        poly_value execute_list(const node* node, context& ctx) {
            assert(node->type == list);
            let child = node->first_child;
            if (child->type == int_literal) {
                var [result, executed] = execute_func(child->int_value, child->next_sibling, ctx);
                if (executed) return result;
            }

            return execute_nodes(child, ctx);
        }

        poly_value execute_nodes(const node* start_node, context& ctx) {
            var result = poly_value {};

            var node = start_node;
            while(node) { defer {node = node->next_sibling; };
                result = execute_node(node, ctx);
            }

            return result;
        }

        uint get_int_literal_value(const node* node) {
            assert(node->type == int_literal);
            return node->int_value;
        }

        array_view<char> execute_istring(const node* nodes, context& ctx) {
            using_exec_ctx;

            var result_storage = arena_array<char>{};
            ensure_capacity_for(result_storage, 16, arena, 1);

            loop_over_sequence(node, nodes) {
                switch (node->type) {
                    case int_literal: {
                        push_uint(result_storage, get_int_literal_value(node), arena);
                        break;
                    }
                    case str_literal: {
                        push(result_storage, get_str_literal_value(node), arena, 1);
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

            return view_of(result_storage);
        }

        array_view<char> get_str_literal_value(const node* node) {
            assert(node->type == str_literal);
            return node->str_value;
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

        poly_value to_poly(uint value) {
            return poly_value { .type = integer, .integer = value };
        }
        poly_value to_poly(const array_view<char>& value) {
            return poly_value { .type = string, .string = value };
        }
    }
}

#endif //FRANCA2_COMPUTE_EXECUTION_H

#pragma clang diagnostic pop
