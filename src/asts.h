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
#include "asts.h"
#include "utility/wasm_emit.h"

namespace compute_asts {
    using namespace arenas;
    using namespace arrays;
    using namespace strings;
    using namespace code_views;

    enum class prim_type {
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
    using enum prim_type;

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

    struct node ;
    struct fn   ;
    struct macro;
    struct local;
    struct scope;

    struct func ;
    struct variable;

    struct ast {
        node* root;

        array<node > nodes;
        array<macro> macros;
        array<scope> scopes;

        arr_dyn<fn> fns;

        arena& data_arena;
        arena& temp_arena;
    };

    struct node {
        enum struct type_t {
            literal,
            func,
        } type;

        string text;
        bool text_is_quoted;
        bool can_be_int  ;
        bool can_be_uint ;
        bool can_be_float;
        union {
            int   int_value;
            uint uint_value;
        };
        float float_value;

        prim_type value_type;
        func* declared_func; //TODO: remove with compilation

        string prefix;
        string infix ;
        string suffix;

        node* parent;
        node* first_child;
        node*  last_child;
        node* next;

        string file_path;

        scope* parent_scope;

        enum struct sem_kind_t {
            sk_unknown,
            sk_str_lit,
            sk_num_lit,
            sk_block,
            sk_ret,
            sk_wasm_type,
            sk_wasm_op,
            sk_local_decl,
            sk_macro_decl,
            sk_macro_invoke,
            sk_local_get,
            sk_local_ref,
            sk_code_embed,
        } sem_kind;

        union {
            struct { // sk_wasm_op
                wasm_emit::wasm_opcode wasm_op;
            };
            struct { // sk_wasm_op
                wasm_emit::wasm_type wasm_type;
            };
            struct { // sk_local_decl
                local* decled_local;
                node* init_node; // TODO: remove this when declaration and assignment are separated
            };
            struct { // sk_macro_decl
                macro* decled_macro;
            };
            struct { // sk_macro_invoke
                macro* refed_macro;
            };
            struct { // sk_local_get & sk_local_ref
                local* local;
            };
            struct { // sk_ret
                node* value_node;
            };
        };
    };

//---- old ----
    struct variable {
        string    id;
        uint      index;
        node*     code_node;
        prim_type value_type;

        enum struct kind_t {
            vk_unknown    ,
            vk_local  ,
            vk_param_value,
            vk_param_ref  ,
            vk_param_code ,
            vk_enum_size  ,
        } kind;
    };

    struct func {
        string id;
        uint index;

        node*  body_node;
        stream body_wasm;

        bool is_inline_wasm;

        arr_dyn<variable > params ;
        arr_dyn<prim_type> results;
        arr_dyn<variable > locals ;

        bool   imported;
        string import_module;

        bool exported;
    };
//---- old end ----

    struct scope {
        macro* parent_macro;
        node*  body_chain;
        arr_dyn<local*> locals;
        arr_dyn<macro*> macros;

        scope* parent;
        scope* next;
        scope* first_child;
    };

    struct local {
        string id;
        uint index_in_macro;

        enum struct kind_t {
            lk_unknown  ,
            lk_value    ,
            lk_ref      ,
            lk_code     ,
            lk_enum_size,
        } kind;

        prim_type value_type;
    };

    struct fn {
        string id;
        uint index;

        arr_dyn<prim_type> params ;
        arr_dyn<prim_type> results;

        enum struct kind_t {
            fk_unknown,
            fk_imported,
            fk_regular,
            fk_enum_size,
        } kind;

        union {
            struct { // local and exported
                arr_dyn<prim_type> locals;
                macro* macro;
                stream body_wasm;
                bool   exported;
            };
            struct { // imported
                string import_module;
            };
        };
    };

    struct macro {
        string id;

        uint params_count;
        arr_dyn<prim_type> results;
        arr_dyn<local    > locals ; // includes parameters

        bool has_returns;

        scope* parent_scope;
        scope*   body_scope;
    };

#define loop_over_chain(name, first_node) for (var name = body_chain; name; name = name->next)

    static let emit_local_get_id = view("emit_local_get");
    static let  decl_local_id = view("var");
    static let          in_id = view("$");
    static let         ref_id = view("&");
    static let        code_id = view("#");
    static let         def_id = view("def");
    static let    def_wasm_id = view("def_wasm");
    static let        disp_id = view("disp");
    static let        wasm_id = view("wasm");
    static let          fn_id = view("fn");
    static let       block_id = view("{}");
    static let         ret_id = view("ret");
    static let          as_id = view("as");
    static let          if_id = view("if");
    static let       while_id = view("while");
    static let        istr_id = view("istr");
    static let         chr_id = view("chr");
    static let       add_a_id = view("+=");
    static let       sub_a_id = view("-=");
    static let       mul_a_id = view("*=");
    static let       div_a_id = view("/=");
    static let       rem_a_id = view("%=");
    static let       print_id = view("print");
    static let        show_id = view("show");

    ast make_ast(arena& data_arena, arena& temp_arena = gta) {
        return {
        .nodes  = alloc_array<node >(data_arena, 2048),
        .macros = alloc_array<macro>(data_arena, 1024),
        .scopes = alloc_array<scope>(data_arena, 1024),

        .fns = make_arr_dyn<fn>(32, data_arena),

        .data_arena = data_arena,
        .temp_arena = temp_arena
    };}

    inline auto is_func         (const node& node) -> bool { return node.type == node::type_t::func; }
    inline auto is_int_literal  (const node& node) -> bool { return node.type == node::type_t::literal && node.can_be_int   && !node.text_is_quoted; }
    inline auto is_uint_literal (const node& node) -> bool { return node.type == node::type_t::literal && node.can_be_uint  && !node.text_is_quoted; }
    inline auto is_float_literal(const node& node) -> bool { return node.type == node::type_t::literal && node.can_be_float && !node.text_is_quoted; }
    inline auto is_str_literal  (const node& node) -> bool { return node.type == node::type_t::literal && node.text_is_quoted; }
    inline auto get_int  (const node& node   ) -> ret1<int  >  { if (is_int_literal  (node)) return ret1_ok(node.  int_value); else return ret1_fail; }
    inline auto get_uint (const node& node   ) -> ret1<uint >  { if (is_uint_literal (node)) return ret1_ok(node. uint_value); else return ret1_fail; }
    inline auto get_float(const node& node   ) -> ret1<float>  { if (is_float_literal(node)) return ret1_ok(node.float_value); else return ret1_fail; }
    inline auto get_str  (const node& node   ) -> ret1<string> { return ret1_ok(node.text); }

    inline auto get_id(const node& n) -> ret1<string> { if (is_func(n)) return ret1_ok(n.text); else return ret1_fail; }

    inline bool is_func(const node& node, string id) {
        if     (is_func(node)); else return false;
        return node.text == id;
    }

    inline bool parent_is_func(const node& node, string id) {
        if_ref(parent, node.parent); else return false;
        return is_func(parent, id);
    }

    inline bool value_is(const node& node,  int value) { return is_int_literal(node)  && node. int_value == value; }
    inline bool value_is(const node& node, uint value) { return is_uint_literal(node) && node.uint_value == value; }

    inline node& make_node(ast& ast, const node& node) { return push(ast.nodes, node); }

    inline scope& make_scope(macro& parent_macro, scope* parent_scope, node* body_nodes, ast& ast) {
        return push(ast.scopes, scope {
            .parent_macro = &parent_macro,
            .body_chain   = body_nodes,
            .locals = make_arr_dyn<local*>(16, ast.data_arena),
            .macros = make_arr_dyn<macro*>( 4, ast.data_arena),
            .parent = parent_scope,
        });
    }

    macro& add_macro(string id, scope* parent_scope, node* body_chain, ast& ast) {
        ref macro = push(ast.macros, compute_asts::macro {
            .id           = id,
            .results      = make_arr_dyn<prim_type>(2, ast.data_arena),
            .locals       = make_arr_dyn<local    >(16, ast.data_arena),
            .parent_scope = parent_scope,
        });

        macro.body_scope = &make_scope(macro, nullptr, body_chain, ast);

        if_ref(scope, parent_scope) push(scope.macros, &macro);
        if_ref(node , body_chain ) node.parent_scope = parent_scope;

        return macro;
    }

    macro& add_macro(cstr id, scope* parent_scope, node* body_nodes, ast& ast) {
        return add_macro(view(id), parent_scope, body_nodes, ast);
    }

    local& add_local(string id, local::kind_t kind, prim_type type, scope& scope) {
        ref macro = *scope.parent_macro;
        var index = macro.locals.data.count;
        ref local = push(scope.parent_macro->locals, compute_asts::local {
            .id = id,
            .index_in_macro = index,
            .kind = kind,
            .value_type = type
        });
        push(scope.locals, &local);
        return local;
    }

    inline local& add_param(string id, local::kind_t kind, prim_type value_type, macro& macro) {
        assert(macro.locals.data.count == macro.params_count); // params must be added before locals
        macro.params_count += 1;
        return add_local(id, kind, value_type, *macro.body_scope);
    }

    inline void add_result(prim_type type, macro& macro) {
        push(macro.results, type);
    }

    arr_view<local> params_of(macro& macro) {
        return {macro.locals.data.data, macro.params_count };
    }

    fn& add_fn(macro& macro, bool exported, ast& ast) {
        let index   = ast.fns.data.count;
        let params  = params_of(macro);
        let results = macro.results.data;

        ref fn = push(ast.fns, {
            .id        = macro.id,
            .index     = index,
            .params    = make_arr_dyn<prim_type>(params .count, ast.data_arena),
            .results   = make_arr_dyn<prim_type>(results.count, ast.data_arena),
            .kind      = fn::kind_t::fk_regular,
            .locals    = make_arr_dyn<prim_type>(8, ast.data_arena),
            .macro     = &macro,
            .body_wasm = make_stream(32, ast.data_arena),
            .exported  = exported,
        });

        for (var i = 0u; i < params.count; ++i)
            push(fn.params, params[i].value_type);

        push(fn.results, results);

        return fn;
    }

    fn& add_import(string module_id, string id, arr_view<prim_type> params, arr_view<prim_type> results, ast& ast) {
        ref fn = push(ast.fns, compute_asts::fn {
            .id      = id,
            .index   = ast.fns.data.count,
            .params  = make_arr_dyn<prim_type>(params .count, ast.data_arena),
            .results = make_arr_dyn<prim_type>(results.count, ast.data_arena),
            .kind = fn::kind_t::fk_imported,
            .import_module = module_id,
        });

        push(fn.params , params);
        push(fn.results, results);

        return fn;
    }

    fn& add_import(cstr module_id, cstr id, init_list<prim_type> params, init_list<prim_type> results, ast& ast) {
        return add_import(view(module_id), view(id), view(params), view(results), ast);
    }

    macro* find_macro(string id, arr_view<prim_type> param_types, scope& scope) {
        for(var scope_p = &scope; scope_p; ) {
            ref sc = *scope_p;
            var macros = sc.macros.data;
            defer {
                ref parent = sc.parent;
                scope_p = parent ? parent : sc.parent_macro->parent_scope;
            };

            for(var i = 0u; i < macros.count; i++) {
                if_ref(macro, macros[i]); else { dbg_fail_return nullptr; }
                if (macro.id == id); else continue;

                let params = params_of(macro);
                if (params.count == param_types.count); else continue;

                var found = true;
                for (var prm_i = 0u; prm_i < param_types.count; ++prm_i)
                    if (params[prm_i].value_type != param_types[prm_i]) {
                        found = false;
                        break;
                    }

                if (found); else continue;

                return &macro;
            }
        }

        return nullptr;
    }

    local* find_local(string id, scope& scope) {
        for(var scope_p = &scope; scope_p; scope_p = scope_p->parent) {
            var locals = scope_p->locals.data;
            for(var i = 0u; i < locals.count; i++) {
                ref v = *locals[i];
                if (v.id == id) return &v;
            }
        }

        return nullptr;
    }

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

    prim_type primitive_type_by(string name) {
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

    wasm_emit::wasm_type wasm_type_of(prim_type type) {
        using enum wasm_emit::wasm_type;

        switch (type) {
            case pt_void: return vt_void;
            case pt_i8 :
            case pt_i16:
            case pt_i32:
            case pt_u8 :
            case pt_u16:
            case pt_u32: return vt_i32;
            case pt_i64:
            case pt_u64: return vt_i64;
            case pt_f32: return vt_f32;
            case pt_f64: return vt_f64;

            default: dbg_fail_return vt_unknown;
        }
    }

    void node_error(node& node) {
        printf("%.*s: Failed to compile node %.*s\n", (int)node.file_path.count, node.file_path.data, (int)node.text.count, node.text.data);
        dbg_fail_return;
    }
}

#endif //FRANCA2_COMPUTE_AST_H