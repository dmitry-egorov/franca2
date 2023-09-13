#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_COMPILATION_H
#define FRANCA2_COMPUTE_COMPILATION_H
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/arr_dyns.h"
#include "utility/hex_ex.h"
#include "utility/wasm_emit.h"

#include "asts.h"

namespace compute_asts {
    arr_view<u8> compile(const ast&);

    namespace compilation {
        using namespace wasm_emit;
        using enum type_t;
        using enum binary_op;

        struct scope_old {
            uint prev_locals_in_scope_count;
            uint prev_funcs_in_scope_count;
        };

        struct context {
            size_t non_inline_func_count;
            arr_dyn<func >      funcs;
            arr_dyn<func*>      func_stack;
            arr_dyn<scope_old> scope_stack;
            arr_dyn<func*>     funcs_in_scope;
            arr_dyn<variable> locals_in_scope;

            stream data;

            wasm_emitter emitter;
            arena& arena;
        };

        static wasm_opcode bop_to_wasm_op[(u32)t_enum_size * (u32)bop_enum_size];

        context make_context(arena& arena);

        void compile_fn_main(node *, context &);

        void gather_defs(node*, context&);
        void type_check (node*, context&);

        void emit_wasm_op(wasm_opcode, node * args, context &);
        void emit_wasm_node       (node&, context&);
        void emit_wasm_body       (node*, context&);

        void emit_scoped_chain    (node*, context&);
        void emit_node            (node&, context&);
        void emit_body            (node&, context&);
        void emit_func_def        (node&, context&);
        void emit_istr            (node&, context&);
        void emit_chr             (node&, context&);
        void emit_bop_a(binary_op, node&, context&);

        void emit_func_call(const string& id, arr_view<type_t> params, context&);

        void push_scope(context&);
        void  pop_scope(context&);
        #define tmp_compilation_scope(ctx) push_scope(ctx); defer { pop_scope(ctx); }

        auto find_local     (string id, context&) -> variable*;
        auto find_func      (string id, arr_view<type_t> params, context&) -> func*;
        auto find_func_index(string id, arr_view<type_t> params, context&) -> ret1<uint>;

        func& declare_import(cstr module_id, cstr id, const init_list<type_t>& params, const init_list<type_t>& results, context& ctx);
        func& declare_import(string module_id, string id, arr_view<type_t> params, arr_view<type_t> results, context& ctx);

        func& declare_func(cstr id, node* body_node, bool is_inline_wasm, context&);
        func& declare_func(string id, node* body_node, bool is_inline_wasm, context&);

        void begin_func_definition(func& func, context&);
        void   end_func_definition(context&);
        #define tmp_func_definition_scope(func, ctx) begin_func_definition(func, ctx); defer { end_func_definition(ctx); }

        auto curr_func_of(context &) -> func&;

        auto add_data(const string& text, context&) -> uint;

        auto get_or_add_local(string id, type_t, context&) -> variable&;
        auto add_local       (string id, type_t, context&) -> variable&;

        void error_local_not_found(string id);
        void error_func_not_found (string id);

        auto add_param(string id, node*, type_t, variable::kind_t, func&) -> uint;
        void add_result(type_t, func&);

        void fill_bop_to_wasm();
        auto get_bop_to_wasm(binary_op, type_t) -> wasm_opcode;
        void set_bop_to_wasm(binary_op, type_t, wasm_opcode);

        void print_node_error(node& node);
    }

    arr_view<u8> compile(const ast& ast) {
        using namespace compilation;
        using namespace wasm_emit;
        using enum wasm_export_kind;

        fill_bop_to_wasm(); //TODO: do only once

        var ctx = make_context(ast.temp_arena);

        declare_import("env", "print_str", {t_u32, t_u32}, {   }, ctx);
        compile_fn_main(ast.root, ctx);

        // emit wasm
        ref emitter = ctx.emitter;

        var func_types = alloc<wasm_func_type>(ast.temp_arena, ctx.funcs.count);
        var imports    = make_arr_dyn<wasm_func_import>( 32, ast.temp_arena);
        var exports    = make_arr_dyn<wasm_export     >( 32, ast.temp_arena);
        var func_specs = make_arr_dyn<wasm_func       >(256, ast.temp_arena);

        push(exports, wasm_export {.name = view("memory"), .kind = ek_mem, .obj_index = 0});

        var func_i = 0u;
        for (var i = 0u; i < ctx.funcs.count; ++i) {
            let func = ctx.funcs.data[i];
            if (!func.is_inline_wasm); else continue;
            defer { func_i += 1; };

            func_types[func_i] = { // TODO: distinct
                .params  = alloc_g<wasm_type>(ast.temp_arena, func.params .count),
                .results = alloc_g<wasm_type>(ast.temp_arena, func.results.count),
            };

            for (var pi = 0u; pi < func.params.count; pi += 1)
                func_types[func_i].params[pi] = wasm_types_of(func.params.data[pi].value_type)[0]; // NOTE: multi-value not supported

            for (var ri = 0u; ri < func.results.count; ri += 1)
                func_types[func_i].results[ri] = wasm_types_of(func.results.data[ri])[0];

            if (func.imported) {
                push(imports, {.module = func.import_module, .name = func.id, .type_index = func_i});
                assert(func.body_wasm.count == 0);
                continue;
            }

            var wasm_func = wasm_emit::wasm_func {
                .type_index = func_i,
                .locals     = alloc_g<wasm_type>(ast.temp_arena, func.locals.count),
                .body       = func.body_wasm.data,
            };

            for (var li = 0u; li < func.locals.count; li += 1)
                wasm_func.locals[li] = wasm_types_of(func.locals.data[li].value_type)[0];

            push(func_specs, wasm_func);

            if (func.exported)
                push(exports, wasm_export {.name = func.id, .kind = ek_func, .obj_index = func.index});
        }

        var mem = wasm_memory {1,1};

        var data  = wasm_data{ctx.data.data};
        var datas = view(data);

        printf("func types: %zu\n", func_types.count);
        printf("imports   : %zu\n", imports   .count);
        printf("exports   : %zu\n", exports   .count);
        printf("funcs     : %zu\n", func_specs.count);
        printf("wasm:\n");

        emit_header(emitter);
        emit_type_section  (func_types     , emitter);
        emit_import_section(imports   .data, emitter);
        emit_func_section  (func_specs.data, emitter);
        emit_memory_section(mem            , emitter);
        emit_export_section(exports   .data, emitter);
        emit_code_section  (func_specs.data, emitter);
        emit_data_section  (datas          , emitter);

        let wasm = emitter.wasm.data;
        print_hex(wasm);
        return wasm;
    }

    namespace compilation {
#define node_failed_return(node) print_node_error(node); assert(false); return

        void compile_fn_main(node* root, context & ctx) {
            ref func = declare_func("main", root, false, ctx);
            add_result(t_u32, func);
            func.exported = true;

            tmp_func_definition_scope(func, ctx);
            emit_scoped_chain(root, ctx);
            emit(op_i32_const, 0u, func.body_wasm);
            emit(op_return       , func.body_wasm);
        }

        void gather_defs(node* chain, context& ctx) {
            for (var node_p = chain; node_p; node_p = node_p->next) {
                ref node = *node_p;
                let is_regular_func = is_func(node, def_id);
                let is_wasm_func = is_func(node, def_wasm_id);
                if (is_regular_func || is_wasm_func); else continue;

                if_var3(id_node, disp_node, type_node, deref3(node.first_child)); else { dbg_fail_return; }
                let body_node = type_node.next;

                let id_text = id_node.text;
                ref func = declare_func(id_text, body_node, is_wasm_func, ctx);

                node.declared_func = &func;
                func.is_inline_wasm = is_wasm_func;

                for (var prm_node_p = disp_node.first_child; prm_node_p; prm_node_p = prm_node_p->next) {
                    ref prm_node = *prm_node_p;
                    let is_in   = is_func(prm_node,   in_id);
                    let is_ref  = is_func(prm_node,  ref_id);
                    let is_code = is_func(prm_node, code_id);

                    if (is_ref  && !is_wasm_func) { printf("Ref parameters only supported in wasm functions!\n"); dbg_fail_return; }
                    if (is_code && !is_wasm_func) { printf("Code parameters only supported in wasm functions!\n"); dbg_fail_return; }
                    if (is_in || is_ref || is_code); else continue;
                    if_var2(prm_id_node, prm_type_node, deref2(prm_node.first_child)); else { dbg_fail_return; }

                    let type = primitive_type_by(prm_type_node.text);
                    using enum variable::kind_t;
                    var kind = is_in ? vk_param_value : is_ref ? vk_param_ref : vk_param_code;
                    add_param(prm_id_node.text, prm_node_p, type, kind, func);
                    prm_node.value_type = type;
                }

                let res_type = primitive_type_by(type_node.text);
                if (res_type != t_void)
                    add_result(res_type, func);

                //TODO: recurse to allow for function definitions inside expressions
            }
        }

        void type_check(node*, context&) {
            //TODO: implement
            //
            // using enum node::lex_kind_t;
            // for (var node_p = chain; node_p; node_p = node_p->next) {
            //     ref node = *node_p;
            //     if (node.type == literal) {
            //         if (node.text_is_quoted) { node.type = t_str; continue; }
            //         node.type = pt_num_literal;
            //         continue;
            //     }
            //
            //     if (node.type == func) {
            //
            //     }
            //     else {
            //         //TODO: need to define locals
            //         // For that, need to store scopes, because we will reuse them on the emit stage
            //         if_ref(local, find_local(node.text, ctx)); else { error_local_not_found(node.text); return; }
            //         node.type = local.type;
            //     }
            // }
        }

        void emit_scoped_chain(node* chain, context& ctx) {
            //TODO: maybe need to rethink this and gather definitions when creating a new scope?
            gather_defs(chain, ctx); // TODO: comptime here?
            //type_check_chain (chain, ctx);

            for(var node = chain; node; node = node->next)
                emit_node(*node, ctx);
        }

        void emit_wasm_op(wasm_opcode op, node* args, context& ctx) {
            ref body = curr_func_of(ctx).body_wasm;
            emit(op, body);

            for (var arg_p = args; arg_p; arg_p = arg_p->next) {
                ref arg = *arg_p;
                if_var1(vu, get_uint (arg)) { emit(vu, body); continue; }
                if_var1(vi, get_int  (arg)) { emit(vi, body); continue; }
                if_var1(vf, get_float(arg)) { emit(vf, body); continue; }
                if_ref (local, find_local(arg.text, ctx)) { emit(local.index, body); continue; }
            }
        }

        void emit_node(node& node, context& ctx) {
            using namespace wasm_emit;
            using enum wasm_opcode;
            using enum variable::kind_t;

            ref curr_func = curr_func_of(ctx);
            ref body = curr_func.body_wasm;

            if (is_str_literal(node)) {
                let str    = node.text;
                let offset = add_data(str, ctx);
                emit_const_get(offset, body);
                emit_const_get(str.count, body);
                node.value_type = t_str;
                return;
            }

            if (is_uint_literal(node)) {
                emit_const_get(node.uint_value, body);
                node.value_type = t_u32;
                return;
            }

            if (is_int_literal(node)) {
                emit_const_get(node.int_value, body);
                node.value_type = t_i32;
                return;
            }

            if (is_float_literal(node)) {
                emit_const_get(node.float_value, body);
                node.value_type = t_f32;
                return;
            }

            var op = find_op(node.text);
            if (op != op_invalid) {
                emit_wasm_op(op, node.first_child, ctx);
                return;
            }

            if (is_func(node)) {
                if_var1(node_id, get_id(node)); else { dbg_fail_return; }
                var args_node_p = node.first_child;

                if (node_id == emit_local_get_id) {
                    if_ref(id_node, args_node_p); else { dbg_fail_return; }
                    let id = id_node.text;
                    if_ref(v, find_local(id, ctx)); else { error_local_not_found(id); return; }
                    emit(op_local_get, v.index, body);
                    node.value_type = v.value_type;
                    return;
                }

                if (node_id == ref_id) {
                    if_ref(id_node, args_node_p); else { dbg_fail_return; }
                    let id = id_node.text;
                    if_ref(v, find_local(id, ctx)); else { error_local_not_found(id); return; }
                    node.value_type = v.value_type;
                    return;
                }

                if (node_id == code_id) {
                    if_ref(id_node, args_node_p); else { dbg_fail_return; }
                    let id = id_node.text;
                    if_ref(v, find_local(id, ctx)); else { error_local_not_found(id); return; }
                    node.value_type = v.value_type;
                    return;
                }

                if (node_id == as_id) {
                    if_var2(arg_node, type_node, deref2(args_node_p)); else { dbg_fail_return; }
                    emit_node(arg_node, ctx);
                    var src_type = arg_node.value_type;
                    var dst_type = primitive_type_by(type_node.text);
                    if (src_type == t_f32 && dst_type == t_u32)
                        emit(op_i32_trunc_f32_u, body);
                    if (src_type == t_f32 && dst_type == t_i32)
                        emit(op_i32_trunc_f32_s, body);

                    node.value_type = dst_type;
                    return;
                }

                if (node_id == decl_local_id) {
                    if_var2(id_node, type_node, deref2(args_node_p)); else { dbg_fail_return; }
                    ref local = add_local(id_node.text, primitive_type_by(type_node.text), ctx);
                    if_ref(init_node, type_node.next) {
                        emit_node(init_node, ctx);
                        emit(op_local_set, local.index, body);
                        node.value_type = t_void; // TODO: analyze compatible and cast
                    }
                    return;
                }

                //if (node_id ==    add_a_id) { emit_bop_a(bop_add, node, ctx); return; }
                if (node_id ==    sub_a_id) { emit_bop_a(bop_sub, node, ctx); return; }
                if (node_id ==    mul_a_id) { emit_bop_a(bop_mul, node, ctx); return; }
                if (node_id ==    div_a_id) { emit_bop_a(bop_div, node, ctx); return; }
                if (node_id ==    rem_a_id) { emit_bop_a(bop_rem, node, ctx); return; }
                if (node_id ==     istr_id) { emit_istr          (node, ctx); return; }
                if (node_id ==      chr_id) { emit_chr           (node, ctx); return; }
                if (node_id ==      def_id) { emit_func_def      (node, ctx); return; }
                if (node_id == def_wasm_id) { return; }

                if (node_id == block_id) {
                    tmp_compilation_scope(ctx);

                    emit(op_block, vt_void, body);
                    emit_scoped_chain(args_node_p, ctx);
                    emit(op_end, body);
                    node.value_type = t_void; // TODO: allow returning values from blocks
                    return;
                }

                if (node_id == if_id) {
                    if_var2(cond_node, then_node, deref2(args_node_p)); else { dbg_fail_return; }
                    emit_node(cond_node, ctx);

                    emit(op_if, (u8)0x40, body);
                    tmp_compilation_scope(ctx);
                    emit_body(then_node, ctx);
                    emit(op_end, body);

                    node.value_type = t_void; // TODO: allow returning from ifs
                    return;
                }

                if (node_id == while_id) {
                    emit_while_scope(body);
                    if_var2(cond_node, body_node, deref2(args_node_p)); else { dbg_fail_return; }

                    emit_node(cond_node, ctx);
                    emit(op_i32_eqz  , body);
                    emit(op_br_if, 1u, body);

                    tmp_compilation_scope(ctx);
                    emit_body(body_node, ctx);
                    node.value_type = t_void; // TODO: allow returning from loops
                    return;
                }

                if (node_id == print_id) {
                    emit_istr(node, ctx);
                    emit_func_call(view("print_str"), view({t_u32, t_u32}), ctx);
                    node.value_type = t_void; // TODO: allow returning from prints
                    return;
                }

                tmp_compilation_scope(ctx);

                var args = make_arr_dyn<type_t>(4, ctx.arena);
                var arg_node_p = args_node_p;
                for (; arg_node_p; arg_node_p = arg_node_p->next) {
                    ref arg = *arg_node_p;
                    emit_node(arg, ctx);
                    push(args, arg.value_type);
                }

                if_ref(func, find_func(node_id, args.data, ctx)); else { error_func_not_found(node_id); return; }
                node.value_type = func.results.count == 0 ? t_void : func.results.data[0]; //TODO: support multiple return values

                arg_node_p = args_node_p;
                let params = func.params.data;
                for (var i = 0u; i < params.count; ++i, arg_node_p = arg_node_p->next) {
                    ref arg_node = *arg_node_p;
                    let param = params[i];
                    if (param.kind == vk_param_ref) {
                        if (is_func(arg_node, ref_id)); else { dbg_fail_return; }
                        if_ref(name_node, arg_node.first_child); else { dbg_fail_return; }
                        if_ref(local, find_local(name_node.text, ctx)); else { error_local_not_found(name_node.text); return; }
                        push(ctx.locals_in_scope, {.id = param.id, .index = local.index, .value_type = param.value_type, .kind = vk_param_ref});
                    }
                    else if (param.kind == vk_param_code) {
                        if (is_func(arg_node, code_id)); else { node_failed_return(arg_node); }
                        push(ctx.locals_in_scope, {.id = param.id, .index = i, .code_node = arg_node.first_child, .value_type = param.value_type, .kind = vk_param_code});
                    }
                }

                if (func.is_inline_wasm) {
                    emit_wasm_body(func.body_node, ctx);
                    return;
                }

                emit(op_call, func.index, curr_func.body_wasm);
                return;
            }

            if_ref(local, find_local(node.text, ctx)) {
                emit(op_local_get, local.index, body);
                node.value_type = local.value_type;
                return;
            }
        }

        void emit_wasm_node(node& node, context& ctx) {
            using namespace wasm_emit;
            using enum wasm_opcode;
            using enum variable::kind_t;

            if_ref(v, find_local(node.text, ctx)) {
                if (v.kind == vk_param_value) {
                    emit(op_local_get, v.index, curr_func_of(ctx).body_wasm);
                    return;
                }
                if (v.kind == vk_param_code) {
                    if_ref(code, v.code_node); else { dbg_fail_return; }
                    emit_node(code, ctx);
                    return;
                }

                dbg_fail_return;
            }

            let op = find_op(node.text);
            if (op != op_invalid); else { printf("Wasm opcode not found: %.*s\n", (int)node.text.count, node.text.data); dbg_fail_return; }
            emit_wasm_op(op, node.first_child, ctx);
        }

        void emit_wasm_body(node* node, context& ctx) {
            if_ref(body, node); else return;

            if (is_func(body, block_id))
                node = node->first_child;

            for (; node; node = node->next)
                emit_wasm_node(*node, ctx);
        }

        void emit_body(node& node, context& ctx) {
            if (is_func(node, block_id)) {
                emit_scoped_chain(node.first_child, ctx);
            } else {
                emit_node(node, ctx);
            }
        }

        void emit_func_def(node& node, context& ctx) {
            using namespace wasm_emit;
            using enum wasm_type;
            using enum variable::kind_t;

            if_var4(id_node, disp_node, type_node, body_node, deref4(node.first_child)); else { dbg_fail_return; }
            if_ref (func, node.declared_func); else { dbg_fail_return; }

            tmp_func_definition_scope(func, ctx);

            var param_index = 0u;
            for (var param_node_p = disp_node.first_child; param_node_p; param_node_p = param_node_p->next) {
                ref param_node = *param_node_p;
                if (is_func(param_node, in_id)); else continue;

                if_var2(param_id_node, param_type_node, deref2(param_node.first_child)); else { dbg_fail_return; }
                push(ctx.locals_in_scope, {
                    .id    = param_id_node.text,
                    .index = param_index,
                    .code_node  = param_node_p,
                    .value_type = primitive_type_by(param_type_node.text),
                    .kind  = vk_param_value,
                });
                param_index++;
            }

            emit_body(body_node, ctx);
            node.value_type = t_void;
        }

        void emit_istr(node& node, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            // TODO: implement with string builder
            if_var1(push_str, find_func_index(view("push"), view({t_u32, t_u32, t_u32}), ctx)); else { dbg_fail_return; }
            if_var1(push_u32, find_func_index(view("push"), view({t_u32        , t_u32}), ctx)); else { dbg_fail_return; }
            if_var1(push_i32, find_func_index(view("push"), view({t_i32        , t_u32}), ctx)); else { dbg_fail_return; }
            if_var1(push_f32, find_func_index(view("push"), view({t_f32        , t_u32}), ctx)); else { dbg_fail_return; }

            ref body = curr_func.body_wasm;

            // write to memory, put the string on the stack
            // convert uints to strings

            // var mem_offset = 1024;
            var dst = get_or_add_local(view("istr_dst__"), t_u32, ctx).index;
            emit_const_get(1024, body);
            emit(op_local_set, dst, body);

            for(var node_p = node.first_child; node_p; node_p = node_p->next) {
                ref arg_node = *node_p;

                // dst = push_*(fn(), dst);
                emit_node(arg_node, ctx);

                let t  = arg_node.value_type;
                let fn = t == t_str ? push_str : t == t_i32 ? push_i32 : t == t_f32 ? push_f32 : push_u32;
                emit(op_local_get, dst, body);
                emit(op_call,       fn, body);
                emit(op_local_set, dst, body);
            }

            // return string { 1024, mem_offset - 1024 };
            emit_const_get(1024, body);
            emit(op_local_get, dst, body);
            emit_const_get(1024, body);
            emit(op_i32_sub       , body);

            node.value_type = t_str;
        }

        void emit_chr(node& node, context& ctx) {
            ref curr_func = curr_func_of(ctx);

            if_ref(value_node, node.first_child); else { dbg_fail_return; }
            if(!is_func(value_node)); else { dbg_fail_return; }
            let c = value_node.text[0];
            emit_const_get((uint) c, curr_func.body_wasm);
            node.value_type = t_u32; // TODO: t_u8
        }

        void emit_bop_a(binary_op bop, node& node, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            ref body      = curr_func.body_wasm;

            if_var2(var_id_node, value_node, deref2(node.first_child)); else { dbg_fail_return; }

            let var_id = var_id_node.text;
            if_ref   (v, find_local(var_id, ctx)); else { error_local_not_found(var_id); return; }
            emit     (op_local_get, v.index, body);
            emit_node(value_node           , ctx );
            if(v.value_type == value_node.value_type); else {
                printf("Type mismatch %u, %u\n", (uint)v.value_type, (uint)value_node.value_type);
                print_node(node);
                printf("\n");
                dbg_fail_return;
            }
            emit(get_bop_to_wasm(bop, v.value_type), body);
            emit(op_local_set,        v.index, body);
            node.value_type = v.value_type;
        }

        context make_context(arena& arena) {
            return {
                .funcs           = make_arr_dyn<func>     ( 256, arena),
                .func_stack      = make_arr_dyn<func*>    (  16, arena),
                .scope_stack     = make_arr_dyn<scope_old>(  32, arena),
                . funcs_in_scope = make_arr_dyn<func*>    ( 128, arena),
                .locals_in_scope = make_arr_dyn<variable> (64, arena),
                .data            = make_stream            (1024, arena),
                .emitter         = make_emitter           (      arena),
                .arena           =                               arena ,
            };
        }

        void emit_func_call(const string& id, arr_view<type_t> params, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            if_ref(func, find_func(id, params, ctx)); else { dbg_fail_return; }
            emit(op_call, func.index, curr_func.body_wasm);
        }

        void push_scope(context& ctx) { push(ctx.scope_stack, {
            .prev_locals_in_scope_count = ctx.locals_in_scope.count,
            . prev_funcs_in_scope_count = ctx. funcs_in_scope.count
        });}

        void pop_scope(context& ctx) { let scope = pop(ctx.scope_stack);
            ctx.locals_in_scope.count = scope.prev_locals_in_scope_count;
            ctx. funcs_in_scope.count = scope. prev_funcs_in_scope_count;
        }

        variable* find_local(string id, context& ctx) {
            for (var i = (int)ctx.locals_in_scope.count - 1; i >= 0; i--) {
                ref v = ctx.locals_in_scope.data[i];
                if (v.id == id) return &v;
            }
            return nullptr;
        }

        func* find_func(string id, arr_view<type_t> params, context& ctx) {
            for (var i = (int)ctx.funcs_in_scope.count - 1; i >= 0; i--) {
                if_ref(func, ctx.funcs_in_scope.data[i]); else { dbg_fail_return nullptr; }
                if (func.id == id); else continue;
                let f_params = func.params.data;
                if (f_params.count == params.count); else continue;

                var found = true;
                for (var prm_i = 0u; prm_i < params.count; ++prm_i)
                    if (f_params[prm_i].value_type != params[prm_i]) {
                        found = false;
                        break;
                    }

                if (found); else continue;

                return &func;
            }

            return nullptr;
        }

        ret1<uint> find_func_index(string id, arr_view<type_t> params, context& ctx) {
            if_ref(func, find_func(id, params, ctx)); else { dbg_fail_return {}; }
            return ret1_ok(func.index);
        }

        func& declare_import(cstr module_id, cstr id, const init_list<type_t>& params, const init_list<type_t>& results, context& ctx) {
            return declare_import(view(module_id), view(id), view(params), view(results), ctx);
        }

        func& declare_import(string module_id, string id, arr_view<type_t> params, arr_view<type_t> results, context& ctx) {
            let index = ctx.funcs.count;
            var prms = make_arr_dyn<variable>(params.count, ctx.arena);
            for (var prm_i = 0u; prm_i < params.count; ++prm_i)
                push(prms, {.id = view(""), .index = prm_i, .value_type = params[prm_i], .kind = variable::kind_t::vk_param_value});

            ref func  = push(ctx.funcs, compute_asts::func {
                .id      = id,
                .index   = index,
                .params  = { .data = prms.data, .capacity = params.count, .arena = &ctx.arena },
                .results = { .data = results  , .capacity = params.count, .arena = &ctx.arena },
                .imported = true,
                .import_module = module_id,
            });

            push(ctx.funcs_in_scope, &func);
            ctx.non_inline_func_count++;

            return func;
        }

        func& declare_func(cstr id, node* body_node, bool is_inline_wasm, context& ctx) {
            return declare_func(view(id), body_node, is_inline_wasm, ctx);
        }

        func& declare_func(string id, node* body_node, bool is_inline_wasm, context& ctx) {
            let index = ctx.non_inline_func_count;
            ref func  = push(ctx.funcs, {
                .id        = id,
                .index     = index,
                .body_node = body_node,
                .body_wasm = make_stream(32, ctx.arena),

                .is_inline_wasm = is_inline_wasm,

                .params    = make_arr_dyn<variable      >(4, ctx.arena),
                .results   = make_arr_dyn<type_t>(2, ctx.arena),
                .locals    = make_arr_dyn<variable>(32, ctx.arena),
            });

            push(ctx.funcs_in_scope, &func);

            if (!is_inline_wasm) ctx.non_inline_func_count++;

            return func;
        }

        void begin_func_definition(func& func, context& ctx) {
            push(ctx.func_stack, &func);
            push_scope(ctx);
        }

        void end_func_definition(context& ctx) {
            pop_scope(ctx);
            pop(ctx.func_stack);
        }

        func& curr_func_of(context& ctx) {
            var f_pp = last_of(ctx.func_stack);
            if_ref(f_p, f_pp); else { assert(false); }
            if_ref(f  , f_p ); else { assert(false); }
            return f;
        }

        uint add_data(const string& text, context& ctx) {
            var offset = ctx.data.count;
            push(ctx.data, {(u8*)text.data, text.count});
            return offset;
        }

        variable& get_or_add_local(string id, type_t type, context& ctx) {
            if_ref(v, find_local(id, ctx)) return v;
            return add_local(id, type, ctx);
        }

        variable& add_local(string id, type_t type, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            var index = curr_func.params.count + curr_func.locals.count;
            ref v = push(curr_func.locals, {.id = id, .index = index, .value_type = type, .kind = variable::kind_t::vk_local} );
            push(ctx.locals_in_scope, v);
            return v;
        }

        void error_local_not_found(string id) {
            printf("Local %.*s not found.\n", (int)id.count, id.data);
            dbg_fail_return;
        }

        void error_func_not_found(string id) {
            printf("Function %.*s not found.\n", (int)id.count, id.data);
            dbg_fail_return;
        }

        uint add_param(string id, node* node, type_t type, variable::kind_t kind, func& func) {
            assert(func.locals.count == 0); // add params before locals
            let index = func.params.count;
            push(func.params, {id, index, node, type, kind});
            return index;
        }

        void add_result(type_t type, func& func) {
            push(func.results, type);
        }

        wasm_opcode get_bop_to_wasm(binary_op bop, type_t pt) {
            var result = bop_to_wasm_op[(u32)bop * (u32)t_enum_size + (u32)pt];
            if (result != op_invalid); else { printf("Unsupported combination of op %u and type %u\n", (uint)bop, (uint)pt); dbg_fail_return result; }
            return result;
        }

        void set_bop_to_wasm(binary_op bop, type_t pt, wasm_opcode op) {
            bop_to_wasm_op[(u32)bop * (u32)t_enum_size + (u32)pt] = op;
        }

        void fill_bop_to_wasm() {
            set_bop_to_wasm(bop_eq  , t_i32, op_i32_eq   );
            set_bop_to_wasm(bop_ne  , t_i32, op_i32_ne   );
            set_bop_to_wasm(bop_lt  , t_i32, op_i32_lt_s );
            set_bop_to_wasm(bop_gt  , t_i32, op_i32_gt_s );
            set_bop_to_wasm(bop_le  , t_i32, op_i32_le_s );
            set_bop_to_wasm(bop_ge  , t_i32, op_i32_ge_s );
            set_bop_to_wasm(bop_add , t_i32, op_i32_add  );
            set_bop_to_wasm(bop_sub , t_i32, op_i32_sub  );
            set_bop_to_wasm(bop_mul , t_i32, op_i32_mul  );
            set_bop_to_wasm(bop_div , t_i32, op_i32_div_s);
            set_bop_to_wasm(bop_rem , t_i32, op_i32_rem_s);
            set_bop_to_wasm(bop_and , t_i32, op_i32_and  );
            set_bop_to_wasm(bop_or  , t_i32, op_i32_or   );
            set_bop_to_wasm(bop_xor , t_i32, op_i32_xor  );
            set_bop_to_wasm(bop_shl , t_i32, op_i32_shl  );
            set_bop_to_wasm(bop_shr , t_i32, op_i32_shr_s);
            set_bop_to_wasm(bop_rotl, t_i32, op_i32_rotl );
            set_bop_to_wasm(bop_rotr, t_i32, op_i32_rotr );

            set_bop_to_wasm(bop_eq  , t_u32, op_i32_eq   );
            set_bop_to_wasm(bop_ne  , t_u32, op_i32_ne   );
            set_bop_to_wasm(bop_lt  , t_u32, op_i32_lt_u );
            set_bop_to_wasm(bop_gt  , t_u32, op_i32_gt_u );
            set_bop_to_wasm(bop_le  , t_u32, op_i32_le_u );
            set_bop_to_wasm(bop_ge  , t_u32, op_i32_ge_u );
            set_bop_to_wasm(bop_add , t_u32, op_i32_add  );
            set_bop_to_wasm(bop_sub , t_u32, op_i32_sub  );
            set_bop_to_wasm(bop_mul , t_u32, op_i32_mul  );
            set_bop_to_wasm(bop_div , t_u32, op_i32_div_u);
            set_bop_to_wasm(bop_rem , t_u32, op_i32_rem_u);
            set_bop_to_wasm(bop_and , t_u32, op_i32_and  );
            set_bop_to_wasm(bop_or  , t_u32, op_i32_or   );
            set_bop_to_wasm(bop_xor , t_u32, op_i32_xor  );
            set_bop_to_wasm(bop_shl , t_u32, op_i32_shl  );
            set_bop_to_wasm(bop_shr , t_u32, op_i32_shr_u);
            set_bop_to_wasm(bop_rotl, t_u32, op_i32_rotl );
            set_bop_to_wasm(bop_rotr, t_u32, op_i32_rotr );

            set_bop_to_wasm(bop_eq  , t_i64, op_i64_eq   );
            set_bop_to_wasm(bop_ne  , t_i64, op_i64_ne   );
            set_bop_to_wasm(bop_lt  , t_i64, op_i64_lt_s );
            set_bop_to_wasm(bop_gt  , t_i64, op_i64_gt_s );
            set_bop_to_wasm(bop_le  , t_i64, op_i64_le_s );
            set_bop_to_wasm(bop_ge  , t_i64, op_i64_ge_s );
            set_bop_to_wasm(bop_add , t_i64, op_i64_add  );
            set_bop_to_wasm(bop_sub , t_i64, op_i64_sub  );
            set_bop_to_wasm(bop_mul , t_i64, op_i64_mul  );
            set_bop_to_wasm(bop_div , t_i64, op_i64_div_s);
            set_bop_to_wasm(bop_rem , t_i64, op_i64_rem_s);
            set_bop_to_wasm(bop_and , t_i64, op_i64_and  );
            set_bop_to_wasm(bop_or  , t_i64, op_i64_or   );
            set_bop_to_wasm(bop_xor , t_i64, op_i64_xor  );
            set_bop_to_wasm(bop_shl , t_i64, op_i64_shl  );
            set_bop_to_wasm(bop_shr , t_i64, op_i64_shr_s);
            set_bop_to_wasm(bop_rotl, t_i64, op_i64_rotl );
            set_bop_to_wasm(bop_rotr, t_i64, op_i64_rotr );

            set_bop_to_wasm(bop_eq  , t_u64, op_i64_eq   );
            set_bop_to_wasm(bop_ne  , t_u64, op_i64_ne   );
            set_bop_to_wasm(bop_lt  , t_u64, op_i64_lt_u );
            set_bop_to_wasm(bop_gt  , t_u64, op_i64_gt_u );
            set_bop_to_wasm(bop_le  , t_u64, op_i64_le_u );
            set_bop_to_wasm(bop_ge  , t_u64, op_i64_ge_u );
            set_bop_to_wasm(bop_add , t_u64, op_i64_add  );
            set_bop_to_wasm(bop_sub , t_u64, op_i64_sub  );
            set_bop_to_wasm(bop_mul , t_u64, op_i64_mul  );
            set_bop_to_wasm(bop_div , t_u64, op_i64_div_u);
            set_bop_to_wasm(bop_rem , t_u64, op_i64_rem_u);
            set_bop_to_wasm(bop_and , t_u64, op_i64_and  );
            set_bop_to_wasm(bop_or  , t_u64, op_i64_or   );
            set_bop_to_wasm(bop_xor , t_u64, op_i64_xor  );
            set_bop_to_wasm(bop_shl , t_u64, op_i64_shl  );
            set_bop_to_wasm(bop_shr , t_u64, op_i64_shr_u);
            set_bop_to_wasm(bop_rotl, t_u64, op_i64_rotl );
            set_bop_to_wasm(bop_rotr, t_u64, op_i64_rotr );

            set_bop_to_wasm(bop_eq  , t_f32, op_f32_eq );
            set_bop_to_wasm(bop_ne  , t_f32, op_f32_ne );
            set_bop_to_wasm(bop_lt  , t_f32, op_f32_lt );
            set_bop_to_wasm(bop_gt  , t_f32, op_f32_gt );
            set_bop_to_wasm(bop_le  , t_f32, op_f32_le );
            set_bop_to_wasm(bop_ge  , t_f32, op_f32_ge );
            set_bop_to_wasm(bop_add , t_f32, op_f32_add);
            set_bop_to_wasm(bop_sub , t_f32, op_f32_sub);
            set_bop_to_wasm(bop_mul , t_f32, op_f32_mul);
            set_bop_to_wasm(bop_div , t_f32, op_f32_div);

            set_bop_to_wasm(bop_eq  , t_f64, op_f64_eq );
            set_bop_to_wasm(bop_ne  , t_f64, op_f64_ne );
            set_bop_to_wasm(bop_lt  , t_f64, op_f64_lt );
            set_bop_to_wasm(bop_gt  , t_f64, op_f64_gt );
            set_bop_to_wasm(bop_le  , t_f64, op_f64_le );
            set_bop_to_wasm(bop_ge  , t_f64, op_f64_ge );
            set_bop_to_wasm(bop_add , t_f64, op_f64_add);
            set_bop_to_wasm(bop_sub , t_f64, op_f64_sub);
            set_bop_to_wasm(bop_mul , t_f64, op_f64_mul);
            set_bop_to_wasm(bop_div , t_f64, op_f64_div);
        }


        void print_node_error(node& node) {
            printf("%.*s: Failed to compile node %.*s\n", (int)node.file_path.count, node.file_path.data, (int)node.text.count, node.text.data);
        }

#undef node_failed_return
    }
}

#endif //FRANCA2_COMPUTE_COMPILATION_H

#pragma clang diagnostic pop