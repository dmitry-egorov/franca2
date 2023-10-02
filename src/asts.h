#ifndef FRANCA2_ASTS_H
#define FRANCA2_ASTS_H

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/arr_bucks.h"
#include "utility/strings.h"
#include "utility/iterators.h"
#include "utility/swap_queuess.h"
#include "utility/files.h"
#include "code_views.h"
#include "utility/wasm_emit.h"

namespace asts {
    using namespace arenas;
    using namespace arrays;
    using namespace strings;
    using namespace swap_queuess;
    using namespace code_views;
    using namespace string_tables;

#define TYPES \
    T(any ) \
    T(void) \
    T(i8  ) \
    T(i16 ) \
    T(i32 ) \
    T(i64 ) \
    T(u8  ) \
    T(u16 ) \
    T(u32 ) \
    T(u64 ) \
    T(f32 ) \
    T(f64 ) \
    T(str )

#define T(id) stx_concat(t_, id),
    enum class type_t {
        t_unknown,
        t_invalid,
        TYPES
        t_enum_size
    };
#undef T
    using enum type_t;

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
    struct expansion;

    struct func;
    struct variable;

    struct ast {
        node* root_chain;

        string_table symbols;

        arr_buck<node     > nodes;
        arr_buck<macro    > macros;
        arr_buck<scope    > scopes;
        arr_buck<fn       > fns;
        arr_buck<expansion> expansions;

        swap_queues<node*> pipeline;

        arena& data_arena;
        arena& temp_arena;
    };


    struct node {
        enum struct lex_kind_t {
            lk_leaf,
            lk_subtree,
        } lex_kind;

        string    text;
        string_id id;
        
        bool is_string   ;
        bool can_be_int  ;
        bool can_be_uint ;
        bool can_be_float;
        union {
            int   int_value;
            uint uint_value;
        };
        float float_value;

        type_t value_type;

        func* declared_func; //TODO: remove with compilation

        string prefix;
        string infix ;
        string suffix;

        node* parent;
        node* child_chain;
        node* next;

        string file_path;

        //// semantics
        expansion* expansion;
        node     * exp_source;

        enum struct stage_t {
            ns_scoping = 0,
            ns_analysis,
            ns_complete
        } stage;

        bool queued;

        scope* scope;

        enum struct sem_kind_t {
            sk_unknown,
            sk_lit,
            sk_block,
            sk_ret,
            sk_wasm_type,
            sk_wasm_op,
            sk_local_decl,
            sk_macro_decl,
            sk_macro_expand,
            sk_fn_call,
            sk_local_get,
            sk_local_ref,
            sk_code_embed,
            sk_chr,
            sk_sub_of,
            sk_each,
            sk_id
        } sem_kind;

        union {
            struct { // sk_wasm_op
                wasm_emit::wasm_opcode wasm_op;
            };
            struct { // sk_wasm_type
                wasm_emit::wasm_type wasm_type;
            };
            struct { // sk_local_decl
                local* decled_local;
                uint   decl_local_index_in_fn;
                node*   init_node ; // TODO: remove this when declaration and assignment are separated
            };
            struct { // sk_macro_decl
                arr_dyn<macro*> decled_macros;
            };
            struct { // sk_macro_expand
                macro* refed_macro; // TODO: can depend on the expansion
                asts::expansion* macro_expansion; // TODO: move to sk_macro_expand struct
            };
            struct { // sk_fn_call
                fn* refed_fn; // TODO: can depend on the expansion
            };
            struct { // sk_local_get & sk_local_ref & sk_code_embed
                local* refed_local;
                uint   local_index_in_fn;
                node* embedded_code;
            };
            struct { // sk_chr
                u8 chr_value;
            };
            struct { // sk_sub_of
                u32   sub_index;
                node* sub_of_node;
            };
            struct { // each
                local* each_list_local;
                node*  each_body_node;
            };
        };
    };

    struct local {
        string_id id;
        uint index_in_macro;
        uint offset;

        enum struct kind_t {
            lk_unknown  ,
            lk_value    ,
            lk_ref      ,
            lk_code     ,
            lk_enum_size,
        } kind;

        type_t value_type;
    };

    struct scope {
        macro* macro;
        node*  chain;
        arr_dyn<local*> locals;
        arr_dyn<asts::macro*> macros;

        scope* parent;
    };

    struct macro {
        string_id id;

        uint   params_count;
        type_t result_type;
        arr_dyn<local> locals ; // includes parameters

        bool has_returns;

        scope* parent_scope;
        scope*   body_scope;
    };

    struct fn {
        string_id id;
        uint index;

        arr_dyn<type_t> param_types;
        type_t result_type;

        enum struct kind_t {
            fk_unknown,
            fk_imported,
            fk_regular,
            fk_enum_size,
        } kind;

        union {
            struct { // local and exported
                macro* macro;
                arr_dyn<type_t> local_types;
                bool exported;

                stream body_wasm;
                arr_dyn<uint> local_offsets;
                uint next_local_offset;

                expansion* expansion;
            };
            struct { // imported
                string_id import_module;
            };
        };
    };

    struct var_binding {
        uint  index_in_fn;
        node* code; // NOTE: lexical  node
        node* init; // NOTE: expanded node
    };

    struct expansion {
        fn   * fn;
        macro* macro;

        uint block_depth;

        arr_dyn<var_binding> bindings;

        node*    source_node;
        node* generated_chain;

        expansion* parent;
        expansion* child_chain;
        expansion* next;
    };

//begin ---- old ----
    struct variable {
        string id;
        uint   index;
        node*  code_node;
        type_t value_type;

        enum struct kind_t {
            vk_unknown    ,
            vk_local      ,
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

        arr_dyn<variable> params ;
        arr_dyn<type_t  > results;
        arr_dyn<variable> locals ;

        bool   imported;
        string import_module;

        bool exported;
    };
//end ---- old ----

#define for_chain(first_node) for (var it = first_node; it; it = it->next)
#define for_chain2(it_name, first_node) for (var it_name = first_node; it_name; it_name = it_name->next)

#define BUILTINS \
    BI(          bi_main, "main"          ) \
    BI(bi_emit_local_get, "emit_local_get") \
    BI(    bi_decl_local, "var"           ) \
    BI(            bi_in, "$"             ) \
    BI(           bi_ref, "&"             ) \
    BI(          bi_code, "#"             ) \
    BI(           bi_def, "def"           ) \
    BI(      bi_def_wasm, "def_wasm"      ) \
    BI(     bi_block_old, "{}"            ) \
    BI(          bi_list, "."             ) \
    BI(           bi_ret, "ret"           ) \
    BI(            bi_as, "as"            ) \
    BI(            bi_if, "if"            ) \
    BI(         bi_while, "while"         ) \
    BI(          bi_istr, "istr"          ) \
    BI(           bi_chr, "chr"           ) \
    BI(         bi_add_a, "+="            ) \
    BI(         bi_sub_a, "-="            ) \
    BI(         bi_mul_a, "*="            ) \
    BI(         bi_div_a, "/="            ) \
    BI(         bi_rem_a, "%="            ) \
    BI(         bi_print, "print"         ) \
    BI(          bi_show, "show"          ) \
    BI(        bi_sub_of, "sub_of"        ) \
    BI(       bi_varargs, "##"            ) \

#define T(id) static uint stx_concat(t_, stx_concat(id, _id));
    TYPES
#undef T

#define BI(id, text) static let stx_concat(id, _text) = view(text);
    BUILTINS
#undef BI
#define BI(id, text) static uint stx_concat(id, _id);
    BUILTINS
#undef BI

    inline uint id_of(string symbol, ast& ast);
    uint size_32_of(type_t type, bool void_as_empty = true);

    ast make_ast(arena& data_arena, arena& temp_arena = gta) {
        var ast = asts::ast {
            .symbols    = make_string_table(32, data_arena),

            .nodes      = make_arr_buck<node     >(2048, data_arena),
            .macros     = make_arr_buck<macro    >( 256, data_arena),
            .scopes     = make_arr_buck<scope    >( 512, data_arena),
            .fns        = make_arr_buck<fn       >(  64, data_arena),
            .expansions = make_arr_buck<expansion>(1024, data_arena),

            .pipeline = make_swap_queue<node*>(temp_arena),
            .data_arena = data_arena,
            .temp_arena = temp_arena
        };

#define T(id) stx_concat(t_, stx_concat(id, _id)) = id_of(view(#id), ast);
        TYPES
#undef T

#define BI(id, text) stx_concat(id, _id) = id_of(stx_concat(id, _text), ast);
        BUILTINS
#undef BI

        return ast;
    }

    inline auto is_func         (const node& node) -> bool { return node.lex_kind == node::lex_kind_t::lk_subtree; }
    inline auto is_int_literal  (const node& node) -> bool { return node.lex_kind == node::lex_kind_t::lk_leaf && node.can_be_int && !node.is_string; }
    inline auto is_uint_literal (const node& node) -> bool { return node.lex_kind == node::lex_kind_t::lk_leaf && node.can_be_uint && !node.is_string; }
    inline auto is_float_literal(const node& node) -> bool { return node.lex_kind == node::lex_kind_t::lk_leaf && node.can_be_float && !node.is_string; }
    inline auto is_str_literal  (const node& node) -> bool { return node.lex_kind == node::lex_kind_t::lk_leaf && node.is_string; }
    inline auto get_int  (const node& node   ) -> ret1<int  >  { if (is_int_literal  (node)) return ret1_ok(node.  int_value); else return ret1_fail; }
    inline auto get_uint (const node& node   ) -> ret1<uint >  { if (is_uint_literal (node)) return ret1_ok(node. uint_value); else return ret1_fail; }
    inline auto get_float(const node& node   ) -> ret1<float>  { if (is_float_literal(node)) return ret1_ok(node.float_value); else return ret1_fail; }

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

    inline string_id id_of(string symbol, ast& ast) {
        return get_or_add(symbol, ast.symbols);
    }

    inline string text_of(string_id id, const ast& ast) {
        return ast.symbols.strings_in_order[id];
    }

    inline node& add_node(ast& ast, const node& node) { return push(node, ast.nodes); }

    inline scope& add_scope(macro& parent_macro, scope* parent_scope, node* body_nodes, ast& ast) {
        return push(scope {
            .macro = &parent_macro,
            .chain   = body_nodes,
            .locals = make_arr_dyn<local*>(16, ast.data_arena),
            .macros = make_arr_dyn<macro*>( 4, ast.data_arena),
            .parent = parent_scope,
        }, ast.scopes);
    }

    macro& add_macro(string_id id, scope* parent_scope, node* body_chain, ast& ast) {
        ref macro = push(asts::macro {
            .id           = id,
            .locals       = make_arr_dyn<local    >(16, ast.data_arena),
            .parent_scope = parent_scope,
        }, ast.macros);

        macro.body_scope = &add_scope(macro, nullptr, body_chain, ast);

        if_ref(scope, parent_scope) push(scope.macros, &macro);
        if_ref(node , body_chain  ) node.scope = parent_scope;

        return macro;
    }
    macro& add_macro(cstr id, scope* parent_scope, node* body_chain, ast& ast) {
        return add_macro(id_of(view(id), ast), parent_scope, body_chain, ast);
    }

    local& add_local(string_id id, local::kind_t kind, type_t type, scope& scope) {
        if_ref(macro, scope.macro); else { assert(false); }

        var index = macro.locals.count;
        ref local = push(macro.locals, asts::local {
            .id = id,
            .index_in_macro = index,
            .kind = kind,
            .value_type = type
        });
        push(scope.locals, &local);
        return local;
    }

    inline local& add_param(string_id id, local::kind_t kind, type_t value_type, macro& macro) {
        assert(macro.locals.count == macro.params_count); // params must be added before locals
        macro.params_count += 1;
        return add_local(id, kind, value_type, *macro.body_scope);
    }

    arr_view<local> params_of(macro& macro) {
        return {macro.locals.data.data, macro.params_count };
    }

    expansion& add_expansion(fn& fn, macro& macro, expansion* parent, node* source_node, ast& ast) {
        return push({
            .fn    = &fn,
            .macro = &macro,
            .bindings = make_arr_dyn<var_binding>(16, ast.data_arena),
            .source_node = source_node,
            .parent = parent,
        }, ast.expansions);
    }

    fn& add_fn(macro& macro, bool exported, ast& ast) {
        let index   = count_of(ast.fns);
        let params  = params_of(macro);

        ref fn = push({
            .id            = macro.id,
            .index         = index,
            .param_types   = make_arr_dyn<type_t>(params .count, ast.data_arena),
            .result_type   = macro.result_type,
            .kind          = fn::kind_t::fk_regular,
            .macro         = &macro,
            .local_types   = make_arr_dyn<type_t>(8, ast.data_arena),
            .local_offsets = make_arr_dyn<uint     >(8, ast.data_arena),
            .body_wasm     = make_stream(32, ast.data_arena),
            .exported      = exported,
        }, ast.fns);

        for_arr(params)
            push(fn.param_types, params[i].value_type);


        return fn;
    }

    uint add_fn_local(type_t type, fn& fn) {
        let index = (uint)fn.local_types.count;
        push(fn.local_types  , type);
        push(fn.local_offsets, fn.next_local_offset);
        fn.next_local_offset += size_32_of(type);
        return index;
    }

    fn& add_import(string_id module_id, string_id id, arr_view<type_t> params, type_t result_type, ast& ast) {
        let index = count_of(ast.fns);
        ref fn = push(asts::fn {
            .id      = id,
            .index   = index,
            .param_types  = make_arr_dyn<type_t>(params .count, ast.data_arena),
            .result_type = result_type,
            .kind = fn::kind_t::fk_imported,
            .import_module = module_id,
        }, ast.fns);

        push(fn.param_types, params);

        return fn;
    }

    fn& add_import(cstr module_id, cstr id, init_list<type_t> params, type_t result_type, ast& ast) {
        return add_import(id_of(view(module_id), ast), id_of(view(id), ast), view(params), result_type, ast);
    }

    fn* find_fn(string_id id, arr_view<type_t> param_types, ast& ast) {
        for_arr_buck_begin(ast.fns, fn, fn_i) {
            if (fn.id == id); else continue;

            let params = fn.param_types.data;
            if (param_types.count == params.count); else continue;

            var found = true;
            for_arr2(prm_i, param_types)
                if (params[prm_i] != t_any && param_types[prm_i] != params[prm_i]) {
                    found = false;
                    break;
                }

            if (found); else continue;

            return &fn;
        } for_arr_buck_end

        return nullptr;
    }

    macro* find_macro(string_id id, arr_view<type_t> param_types, scope& scope) {
        for(var scope_p = &scope; scope_p; ) {
            ref sc = *scope_p;
            var macros = sc.macros.data;
            defer {
                ref parent = sc.parent;
                scope_p = parent ? parent : sc.macro->parent_scope;
            };

            for(var i = 0u; i < macros.count; i++) {
                if_ref(macro, macros[i]); else { dbg_fail_return nullptr; }
                if (macro.id == id); else continue;

                let params = params_of(macro);
                if (param_types.count == params.count); else continue;

                var found = true;
                for_arr2(prm_i, param_types)
                    if (params[prm_i].value_type != t_any && param_types[prm_i] != params[prm_i].value_type) {
                        found = false;
                        break;
                    }

                if (found); else continue;

                return &macro;
            }
        }

        return nullptr;
    }

    local* find_local(string_id id, scope& scope) {
        for(var scope_p = &scope; scope_p; scope_p = scope_p->parent) {
            var locals = scope_p->locals.data;
            for_arr(locals) {
                if_ref(v, locals[i]); else { dbg_fail_return nullptr; }
                if (v.id == id) return &v;
            }
        }

        return nullptr;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-dereference"
    inline ret2<node&, node&> deref2(node* n) {
        using namespace asts;
        static let fail = ret2<node&, node&>{*((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        return ret2_ok(node0, node1);
    }

    inline ret3<node&, node&, node&> deref3(node* n) {
        using namespace asts;
        static let fail = ret3<node&, node&, node&>{*((node*) nullptr), *((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        if_ref(node2, node1.next); else return fail;
        return ret3_ok(node0, node1, node2);
    }

    inline ret4<node&, node&, node&, node&> deref4(node* n) {
        using namespace asts;
        static let fail = ret4<node&, node&, node&, node&>{*((node*) nullptr), *((node*) nullptr), *((node*) nullptr), *((node*) nullptr), false};
        if_ref(node0, n         ); else return fail;
        if_ref(node1, node0.next); else return fail;
        if_ref(node2, node1.next); else return fail;
        if_ref(node3, node2.next); else return fail;
        return ret4_ok(node0, node1, node2, node3);
    }
#pragma clang diagnostic pop

    #define if_chain2(n0, n1, first_node) if_var2(n0, n1, deref2(first_node))
    #define if_chain3(n0, n1, n2, first_node) if_var3(n0, n1, n2, deref3(first_node))
    #define if_chain4(n0, n1, n2, n3, first_node) if_var4(n0, n1, n2, n3, deref4(first_node))

    ret1<type_t> find_type(string_id id) {
        if (id == t_any_id ) return ret1_ok(t_any );
        if (id == t_void_id) return ret1_ok(t_void);
        if (id == t_i8_id  ) return ret1_ok(t_i8  );
        if (id == t_i16_id ) return ret1_ok(t_i16 );
        if (id == t_i32_id ) return ret1_ok(t_i32 );
        if (id == t_u8_id  ) return ret1_ok(t_u8  );
        if (id == t_u16_id ) return ret1_ok(t_u16 );
        if (id == t_u32_id ) return ret1_ok(t_u32 );
        if (id == t_i64_id ) return ret1_ok(t_i64 );
        if (id == t_u64_id ) return ret1_ok(t_u64 );
        if (id == t_f32_id ) return ret1_ok(t_f32 );
        if (id == t_f64_id ) return ret1_ok(t_f64 );
        if (id == t_str_id ) return ret1_ok(t_str );

        return ret1_fail;
    }

    arr_view<wasm_emit::wasm_type> wasm_types_of(type_t type, bool void_as_empty = true) {
        using enum wasm_emit::wasm_type;

        static let wasm_types_arr = alloc(gta, {vt_void, vt_i32, vt_i64, vt_f32, vt_f64, /* str */ vt_i32, vt_i32});

        switch (type) {
            case t_void: return sub(wasm_types_arr, 0, void_as_empty ? 0 : 1);
            case t_i8 :
            case t_i16:
            case t_i32:
            case t_u8 :
            case t_u16:
            case t_u32: return sub(wasm_types_arr, 1, 1);
            case t_i64:
            case t_u64: return sub(wasm_types_arr, 2, 1);
            case t_f32: return sub(wasm_types_arr, 3, 1);
            case t_f64: return sub(wasm_types_arr, 4, 1);

            case t_str: return sub(wasm_types_arr, 5, 2);

            default: dbg_fail_return {};
        }
    }

    uint size_32_of(type_t type, bool void_as_empty) {
        return wasm_types_of(type, void_as_empty).count;
    }

    void print_symbols(ast& ast) {
        printf("Symbols:\n");
        for_arr_buck_begin(ast.symbols.strings_in_order, symbol, symbol_i) {
            printf("%zu: %.*s\n", symbol_i, (int)symbol.count, symbol.data);
        } for_arr_buck_end
    }

    void bind(node* code, expansion& exp) {
        push(exp.bindings, {.index_in_fn = (uint)-1, .code = code});
    }

    void bind(uint index_in_fn, expansion& exp) {
        assert(index_in_fn != (uint)-1);
        push(exp.bindings, {.index_in_fn = index_in_fn});
    }

    void bind_fn_params(expansion& exp) {
        if_ref(fn, exp.fn); else { dbg_fail_return; }
        ref params = fn.param_types.data;

        for(var i = 0u; i < params.count; ++i)
            bind(i, exp);
    }

    auto add_and_bind_fn_local(type_t type, expansion& exp) -> uint /* index */ {
        if_ref(fn, exp.fn); else { dbg_fail_return -1; }
        let index = add_fn_local(type, fn);
        bind(index, exp);
        return index;
    }

    auto find_binding(uint index_in_macro, expansion& exp) -> var_binding {
        let bindings = exp.bindings.data;
        if (index_in_macro < bindings.count); else {
            printf("Failed to find binding for macro local %u\n", index_in_macro);
            dbg_fail_return {};
        }

        return bindings[index_in_macro];
    }

    auto find_local_index(uint index_in_macro, expansion& exp) -> uint {
        return find_binding(index_in_macro, exp).index_in_fn;
    }

    auto find_local_offset(uint index_in_macro, expansion& exp) -> uint {
        if_ref(fn, exp.fn); else { dbg_fail_return -1; }
        return fn.local_offsets[find_local_index(index_in_macro, exp)];
    }

    auto find_code_node(uint index_in_macro, expansion& exp) -> node* {
        return find_binding(index_in_macro, exp).code;
    }

    void node_error(node& node) {
        printf("%.*s: Failed to compile node %.*s, kind: %u\n", (int)node.file_path.count, node.file_path.data, (int)node.text.count, node.text.data, (uint)node.sem_kind);
        dbg_fail_return;
    }

    void print_exp_bindings(const expansion& exp, const ast& ast) {
        if (exp.parent) print_exp_bindings(*exp.parent, ast);

        if_ref(macro, exp.macro); else { dbg_fail_return; }
        let macro_id = text_of(macro.id, ast);
        printf("%.*s: ", (int)macro_id.count, macro_id.data);

        ref bindings = exp.bindings.data;
        for_arr2(j, bindings) {
            if (j < count_of(macro.locals)) {
                let local_name = text_of(macro.locals[j].id, ast);
                printf("%.*s", (int)local_name.count, local_name.data);
            }
            else {
                printf("arg_tmp_%zu", (size_t)(j - count_of(macro.locals)));
            }

            printf(" -> ");

            ref binding = bindings[j];
            if (binding.index_in_fn == (uint)-1) {
                if_ref(code, binding.code); else { dbg_fail_return; }
                printf("%.*s", (int)code.text.count, code.text.data);
            }
            else
                printf("$%u", binding.index_in_fn);

            if (j != bindings.count - 1) printf(", ");
        }
        printf("\n");
    }
}

#pragma clang diagnostic pop

#endif //FRANCA2_ASTS_H