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
    struct poly_value {
        enum class type_t {
            integer,
            string
        } type;

        union {
            uint integer;
            arrays::array_view<char> string;
        };
    };

    struct node {
        enum struct type_t {
            literal,
            list,
        } type;

        poly_value value;

        arrays::array_view<char> prefix;
        arrays::array_view<char> suffix;

        node* parent;
        node* first_child;
        node*  last_child;
        node* next;
    };

#define loop_over_sequence(name, first_node) for (var name = first_node; name; name = name->next)

    struct ast_storage {
        arenas::arena node_arena;
        arenas::arena text_arena;
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
        var_ref       =   4,
        istring       =   5,
        op_assign     = 100,
        op_plus       = 110,
        fn_print      = 200,
        macro_show_as = 300,
    };

    poly_value to_poly(uint value) { return poly_value { .type = poly_value::type_t::integer, .integer = value }; }
    poly_value to_poly(const arrays::array_view<char>& value) { return poly_value { .type = poly_value::type_t::string, .string = value }; }

    inline bool is_int_literal(const node* node) { return node->type == node::type_t::literal && node->value.type == poly_value::type_t::integer; }
    inline bool is_str_literal(const node* node) { return node->type == node::type_t::literal && node->value.type == poly_value::type_t::string ; }
    inline bool is_int_literal(const node& node) { return is_int_literal(&node); }
    inline bool is_str_literal(const node& node) { return is_str_literal(&node); }

    inline ret1<uint> get_int(const poly_value& v) { if (v.type == poly_value::type_t::integer) return ret1_ok(v.integer); else return ret1_fail; }
    inline ret1<arrays::array_view<char>> get_str(const poly_value& v) { if (v.type == poly_value::type_t::string ) return ret1_ok(v.string ); else return ret1_fail; }
    inline ret1<uint> get_int(const node& node) { if (is_int_literal(node)) return ret1_ok(node.value.integer); else return ret1_fail; }
    inline ret1<arrays::array_view<char>> get_str(const node& node) { if (is_str_literal(node)) return ret1_ok(node.value.string); else return ret1_fail; }

    inline bool value_is(const node& node, uint value) { return is_int_literal(&node) && node.value.integer == value; }

    inline node* make_node(ast_storage& storage, const node& node) { return alloc_one(storage.node_arena, node); }

    inline node* make_list_node   (ast_storage& storage, node* first_child, node* last_child) { return make_node(storage, { .type = node::type_t::list, .first_child = first_child, .last_child = last_child }); }
    inline node* make_literal_node(ast_storage& storage, uint i, node* next_sibling = nullptr) { return make_node(storage, { .type = node::type_t::literal, .value = to_poly(i), .next = next_sibling }); }
    inline node* make_literal_node(ast_storage& storage, const char* s, node* next_sibling = nullptr) { return make_node(storage, { .type = node::type_t::literal, .value = to_poly(strings::view_of(s)), .next = next_sibling }); }
    inline node* make_literal_node(ast_storage& storage, const arrays::array_view<char>& s, node* next_sibling = nullptr) { return make_node(storage, { .type = node::type_t::literal, .value = to_poly(s), .next = next_sibling }); }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-dereference"
    inline ret2<const node&, const node&> deref_list2(const node* n) {
        using namespace compute_asts;
        static let fail = ret2<const node&, const node&>{*((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        return {node0, node1, true};
    }

    inline ret3<const node&, const node&, const node&> deref_list3(const node* n) {
        using namespace compute_asts;
        static let fail = ret3<const node&, const node&, const node&>{*((node*) nullptr), *((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        if_ref(node2, node1.next); else return fail;
        return {node0, node1, node2, true};
    }

    inline ret4<const node&, const node&, const node&, const node&> deref_list4(const node* n) {
        using namespace compute_asts;
        static let fail = ret4<const node&, const node&, const node&, const node&>{*((node*) nullptr), *((node*) nullptr), *((node*) nullptr), *((node*) nullptr), false};
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