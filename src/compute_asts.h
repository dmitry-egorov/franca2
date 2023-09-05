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

    enum class primitive_type {
        pt_unknown,
        pt_invalid,
        pt_void,
        pt_i8 ,
        pt_i16,
        pt_i32,
        pt_i64,
        pt_u8 ,
        pt_u16,
        pt_u32,
        pt_u64,
        pt_f32,
        pt_f64,
        pt_str,
        pt_num_literal,
        pt_enum_size
    };
    using enum primitive_type;

    enum class binary_op {
        bop_invalid,
        bop_eq,
        bop_ne,
        bop_le,
        bop_ge,
        bop_lt,
        bop_gt,
        bop_add,
        bop_sub,
        bop_mul,
        bop_div,
        bop_rem,
        bop_and,
        bop_or,
        bop_xor,
        bop_shl,
        bop_shr,
        bop_rotl,
        bop_rotr,
        bop_enum_size
    };

    struct poly_value {
        primitive_type type;

        union {
              u8   u8;
             u16   u16;
            uint   u32;
             u64   u64;
              i8   i8;
             i16   i16;
             int   i32;
             i64   i64;
             float f32;
            double f64;
            string str;
        };
    };

    struct func;
    struct node;

    struct node {
        string text;
        bool text_is_quoted;
        bool can_be_int  ;
        bool can_be_uint ;
        bool can_be_float;
        int     int_value;
        uint   uint_value;
        float float_value;

        func* func;

        enum struct type_t {
            literal,
            func,
        } type;

        primitive_type value_type;

        string prefix;
        string infix ;
        string suffix;

        node* parent;
        node* first_child;
        node*  last_child;
        node* next;
    };

    struct variable {
        string id;
        uint index;
        primitive_type type;
    };

    struct func {
        string id;
        uint index;

        node* body_node;

        bool is_inline_wasm;

        arr_dyn<variable> params ;
        arr_dyn<variable> locals ;
        arr_dyn<primitive_type> results;
        stream body_wasm;

        bool   imported;
        string import_module;

        bool exported;
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

    static let emit_local_get_id = view("emit_local_get");
    static let  decl_local_id = view("var");
    static let  decl_param_id = view("$");
    static let         def_id = view("def");
    static let    def_wasm_id = view("def_wasm");
    static let       block_id = view("{}");
    static let          as_id = view("as");
    static let          if_id = view("if");
    static let       while_id = view("while");
    static let        istr_id = view("istr");
    static let         chr_id = view("chr");
    static let      assign_id = view("=");
    static let         sub_id = view("-");
    static let         mul_id = view("*");
    static let         div_id = view("/");
    static let         rem_id = view("%");
    static let       add_a_id = view("+=");
    static let       sub_a_id = view("-=");
    static let       mul_a_id = view("*=");
    static let       div_a_id = view("/=");
    static let       rem_a_id = view("%=");
    static let       print_id = view("print");
    static let        show_id = view("show");

    ast_storage make_ast_storage() {
        return {
            arenas::make(2 * 1048 * sizeof(node)),
            arenas::make(2 * 1024 * 1024 * sizeof(char))
        };
    }

    poly_value to_poly(uint value) { return poly_value { .type = pt_u32, .u32 = value }; }
    poly_value to_poly(const string& value) { return poly_value { .type = pt_str, .str = value }; }

    inline auto is_func         (const node& node) -> bool { return node.type == node::type_t::func; }
    inline auto is_int_literal  (const node& node) -> bool { return node.type == node::type_t::literal && node.can_be_int   && !node.text_is_quoted; }
    inline auto is_uint_literal (const node& node) -> bool { return node.type == node::type_t::literal && node.can_be_uint  && !node.text_is_quoted; }
    inline auto is_float_literal(const node& node) -> bool { return node.type == node::type_t::literal && node.can_be_float && !node.text_is_quoted; }
    inline auto is_str_literal  (const node& node) -> bool { return node.type == node::type_t::literal && node.text_is_quoted; }

    inline auto get_uint (const poly_value& v) -> ret1<uint>   { if (v.type == pt_u32) return ret1_ok(v.u32  ); else return ret1_fail; }
    inline auto get_str  (const poly_value& v) -> ret1<string> { if (v.type == pt_str) return ret1_ok(v.str); else return ret1_fail; }
    inline auto get_int  (const node& node   ) -> ret1<int  >  { if (is_int_literal  (node)) return ret1_ok(node.  int_value); else return ret1_fail; }
    inline auto get_uint (const node& node   ) -> ret1<uint >  { if (is_uint_literal (node)) return ret1_ok(node. uint_value); else return ret1_fail; }
    inline auto get_float(const node& node   ) -> ret1<float>  { if (is_float_literal(node)) return ret1_ok(node.float_value); else return ret1_fail; }
    inline auto get_str  (const node& node   ) -> ret1<string> { return ret1_ok(node.text); }

    inline auto get_fn_id(const node& n) -> ret1<string>   { if (is_func(n)) return ret1_ok(n.text); else return ret1_fail; }

    inline bool is_func(const node& node, string id) {
        if     (is_func(node)); else return false;
        return node.text == id;
    }

    inline bool is_func(const node* n, string id) {
        if_ref (node , n); else return false;
        return is_func(node, id);
    }


    inline bool parent_is_func(const node& node, string id) {
        if_ref(parent, node.parent); else return false;
        return is_func(parent, id);
    }

    inline bool value_is(const node& node,  int value) { return is_int_literal(node)  && node. int_value == value; }
    inline bool value_is(const node& node, uint value) { return is_uint_literal(node) && node.uint_value == value; }

    inline node& make_node(ast_storage& storage, const node& node) { return *alloc_one(storage.node_arena, node); }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-dereference"
    inline ret2<node&, node&> deref2(node* n) {
        using namespace compute_asts;
        static let fail = ret2<node&, node&>{*((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        return ret2_ok(node0, node1);
    }

    inline ret3<node&, node&, node&> deref3(node* n) {
        using namespace compute_asts;
        static let fail = ret3<node&, node&, node&>{*((node*) nullptr), *((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        if_ref(node2, node1.next); else return fail;
        return ret3_ok(node0, node1, node2);
    }

    inline ret4<node&, node&, node&, node&> deref4(node* n) {
        using namespace compute_asts;
        static let fail = ret4<node&, node&, node&, node&>{*((node*) nullptr), *((node*) nullptr), *((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        if_ref(node2, node1.next); else return fail;
        if_ref(node3, node2.next); else return fail;
        return ret4_ok(node0, node1, node2, node3);
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

    primitive_type primitive_type_by(string name) {
        if (name == view("void")) return pt_void ;
        if (name == view("i8"  )) return pt_i8 ;
        if (name == view("i16" )) return pt_i16;
        if (name == view("i32" )) return pt_i32;
        if (name == view("u8 " )) return pt_u8 ;
        if (name == view("u16" )) return pt_u16;
        if (name == view("u32" )) return pt_u32;
        if (name == view("i64" )) return pt_i64;
        if (name == view("u64" )) return pt_u64;
        if (name == view("f32" )) return pt_f32;
        if (name == view("f64" )) return pt_f64;

        printf("Type %.*s not found.\n", (int)name.count, name.data);
        dbg_fail_return pt_invalid;
    }
}

#endif //FRANCA2_COMPUTE_AST_H