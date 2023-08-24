#ifndef FRANCA2_COMPUTE_AST_H
#define FRANCA2_COMPUTE_AST_H

#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/strings.h"
#include "utility/arenas.h"
#include "utility/iterators.h"
#include "utility/files.h"
#include "compute_asts.h"

namespace compute_asts {
    using namespace arenas;
    using namespace arrays;
    using namespace strings;
    using namespace code_views;

    struct poly_value {
        enum class type_t {
            poly_integer,
            poly_string
        } type;

        union {
            uint integer;
            string string;
        };
    };

    struct node {
        enum struct type_t {
            literal,
            func,
        } type;

        poly_value value;

        string prefix;
        string suffix;

        node* parent;
        node* first_child;
        node*  last_child;
        node* next;
    };

#define loop_over_chain(name, first_node) for (var name = first_node; name; name = name->next)

    struct ast_storage {
        arena node_arena;
        arena text_arena;
    };

    struct ast {
        node* root;

        ast_storage storage;
    };

    enum class builtin_func_id {
        inactive      =   0,
        block         =   1,
        decl_var      =   2,
        decl_param    =   3,
        decl_func     =   4,
        ref_var       =   5,
        ctr_return    =  10,
        ctr_if        =  11,
        ctr_loop      =  12,
        istring       =  50,
        op_assign     = 100,
        op_add        = 110,
        op_sub        = 111,
        op_eq         = 120,
        op_neq        = 121,
        op_lte        = 122,
        op_gte        = 123,
        op_lt         = 124,
        op_gt         = 125,
        fn_print      = 200,
        macro_show_as = 300,
    };

    poly_value to_poly(uint value) { return poly_value { .type = poly_value::type_t::poly_integer, .integer = value }; }
    poly_value to_poly(const string& value) { return poly_value { .type = poly_value::type_t::poly_string, .string = value }; }

    inline auto is_func       (const node& node) -> bool { return node.type == node::type_t::func; }
    inline auto is_int_literal(const node* node) -> bool { return node->type == node::type_t::literal && node->value.type == poly_value::type_t::poly_integer; }
    inline auto is_str_literal(const node* node) -> bool { return node->type == node::type_t::literal && node->value.type == poly_value::type_t::poly_string ; }
    inline auto is_int_literal(const node& node) -> bool { return is_int_literal(&node); }
    inline auto is_str_literal(const node& node) -> bool { return is_str_literal(&node); }

    inline auto get_int(const poly_value& v) -> ret1<uint>   { if (v.type == poly_value::type_t::poly_integer) return ret1_ok(v.integer); else return ret1_fail; }
    inline auto get_str(const poly_value& v) -> ret1<string> { if (v.type == poly_value::type_t::poly_string ) return ret1_ok(v.string ); else return ret1_fail; }
    inline auto get_int(const node& node   ) -> ret1<uint>   { return get_int(node.value); }
    inline auto get_str(const node& node   ) -> ret1<string> { return get_str(node.value); }


    inline bool is_func(const node& node, uint value) {
        if     (is_func(node)); else return false;
        if_var1(fn_id, get_int(node)); else return false;
        return fn_id == value;
    }

    inline bool is_func(const node* n, uint value) {
        if_ref (node , n); else return false;
        return is_func(node, value);
    }


    inline bool parent_is_func(const node& node, uint value) {
        if_ref(parent, node.parent); else return false;
        return is_func(parent, value);
    }

    inline bool value_is(const node* node, uint value) { return is_int_literal(node) && node->value.integer == value; }

    inline node* make_node(ast_storage& storage, const node& node) { return alloc_one(storage.node_arena, node); }

    inline node* make_func_node   (ast_storage& storage, poly_value id, node* first_child, node* last_child) { return make_node(storage, { .type = node::type_t::func, .value = id, .first_child = first_child, .last_child = last_child }); }
    inline node* make_literal_node(ast_storage& storage, poly_value id, node* next_sibling = nullptr) { return make_node(storage, { .type = node::type_t::literal, .value = id, .next = next_sibling }); }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-dereference"
    inline ret2<node&, node&> deref_list2(node* n) {
        using namespace compute_asts;
        static let fail = ret2<node&, node&>{*((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        return {node0, node1, true};
    }

    inline ret3<node&, node&, node&> deref_list3(node* n) {
        using namespace compute_asts;
        static let fail = ret3<node&, node&, node&>{*((node*) nullptr), *((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        if_ref(node2, node1.next); else return fail;
        return {node0, node1, node2, true};
    }

    inline ret4<node&, node&, node&, node&> deref_list4(node* n) {
        using namespace compute_asts;
        static let fail = ret4<node&, node&, node&, node&>{*((node*) nullptr), *((node*) nullptr), *((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        if_ref(node2, node1.next); else return fail;
        if_ref(node3, node2.next); else return fail;
        return {node0, node1, node2, node3, true};
    }
#pragma clang diagnostic pop

    inline void release(ast_storage& storage) {
        release(storage.node_arena);
        release(storage.text_arena);
    }

    inline void release(ast& ast) {
        release(ast.storage);
        ast.root = nullptr;
    }
}
#endif //FRANCA2_COMPUTE_AST_H