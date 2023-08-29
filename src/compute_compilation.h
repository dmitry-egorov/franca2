#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_COMPILATION_H
#define FRANCA2_COMPUTE_COMPILATION_H
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/arena_arrays.h"
#include "utility/hex_ex.h"
#include "utility/wasm_emit.h"

#include "compute_asts.h"

namespace compute_asts {
    array_view<u8> compile_wasm(const ast&, arena& = gta);

    namespace compilation {
        using namespace wasm_emit;

        struct scope {
            uint prev_vars_count;
            uint prev_funcs_count;
        };

        struct variable {
            string id;
            uint   local_index;
        };

        struct func {
            string id;
            uint  local_index;
            uint params_count;
        };

        struct context {
            arena& arena;

            wasm_memory mem;

            arena_array<scope    > scopes;
            arena_array<variable > scope_vars ;
            arena_array<func     > scope_funcs;

            arena_array<wasm_data> datas;
            arena_array<uint     > data_offsets;

            arena_array<wasm_func_type  > func_types;
            arena_array<wasm_func_import> func_imports;
            arena_array<wasm_export     > exports;
            arena_array<wasm_func       > func_specs;

            wasm_emitter emitter;
        };

#define using_compilation_context ref [arena, mem, scopes, variables, funcs, datas, data_offsets, func_types, imports, exports, func_specs, emitter] =
#define tmp_compilation_scope(ctx) push_scope(ctx); defer { pop_scope(ctx); }
        context make_context(arena& arena) {
            using namespace wasm_emit;
            return {
                .arena        = arena,
                .scopes       = make_arena_array<scope    >(  32, arena),
                .scope_vars    = make_arena_array<variable >(1024, arena),
                .scope_funcs        = make_arena_array<func     >(256, arena),
                .datas        = make_arena_array<wasm_data>(  32, arena),
                .data_offsets = make_arena_array<uint     >(  32, arena),
                .func_types   = make_arena_array<wasm_func_type       >(32, arena),
                .func_imports = make_arena_array<wasm_func_import>(32, arena),
                .exports      = make_arena_array<wasm_export    >(32, arena),
                .func_specs   = make_arena_array<wasm_func  >(32, arena),
                .emitter      = make_emitter(arena)
            };
        }

        void push_scope(context& ctx) {
            push(ctx.scopes, {.prev_vars_count = ctx.scope_vars.data.count, .prev_funcs_count = ctx.scope_funcs.data.count});
        }

        void pop_scope(context& ctx) {
            let scope = pop(ctx.scopes);
            ctx.scope_vars.data.count = scope.prev_vars_count;
            ctx.scope_funcs    .data.count = scope.prev_funcs_count;
        }

        variable* find_var(string id, context& ctx) {
            for (var i = (int)ctx.scope_vars.data.count - 1; i >= 0; i--) {
                ref v = ctx.scope_vars.data[i];
                if (v.id == id) return &v;
            }
            return nullptr;
        }

        func* find_func(string id, context& ctx) {
            for (var i = (int)ctx.scope_funcs.data.count - 1; i >= 0; i--) {
                ref v = ctx.scope_funcs.data[i];
                if (v.id == id) return &v;
            }
            return nullptr;
        }

        struct func_context {
            string id;

            arena_array<wasm_value_type> params ;
            arena_array<wasm_value_type> results;
            arena_array<wasm_value_type> locals ;
            arena_array<u8> body;

            bool should_export;

            context& ctx;
        };

        func_context make_func_context(string id, context& ctx) { return {
            .id = id,
            .params  = make_arena_array<wasm_value_type>(8, ctx.arena),
            .results = make_arena_array<wasm_value_type>(4, ctx.arena),
            .locals  = make_arena_array<wasm_value_type>(32, ctx.arena),
            .body    = make_arena_array<u8>        (32, ctx.arena),
            .should_export = false,
            .ctx = ctx,
        };}

#define using_func_context ref [id, params, results, locals, body, should_export, ctx] =

        void emit_main(node*, context&);

        void emit_u2str(context &);

        void emit_block(node*, func_context&);
        void emit_node (node&, func_context&);
        void emit_body (node&, func_context&);

        void emit_func_def(node*, context&);

        void emit_ref (node*, func_context&);
        void emit_istr(node*, func_context&);

        void emit_bop(wasm_opcode, node*, func_context&);

        uint add_type(const array_view<wasm_value_type>& params, const array_view<wasm_value_type>& results, context& ctx) {
            push(ctx.func_types, {
                .params  = params,
                .results = results,
            });
            return ctx.func_types.data.count - 1;
        }

        uint add_type(const std::initializer_list<wasm_value_type>& params, const std::initializer_list<wasm_value_type>& results, context& ctx) {
            return add_type(view_of(params), view_of(results), ctx);
        }

        void add_import(const char* module_name, const char* name, wasm_func_type_index type_index, context& ctx) {
            assert(ctx.func_specs.data.count == 0); // add import before funcs_specs
            push(ctx.func_imports, {.module = view_of(module_name), .name = view_of(name), .type_index = type_index });
        }

        void add_export(string name, wasm_export_kind kind, uint obj_index, context& ctx) {
            push(ctx.exports, { .name = name, .kind = kind, .obj_index = obj_index });
        }

        void add_export(const char* name, wasm_export_kind kind, uint obj_index, context& ctx) {
            add_export(view_of(name), kind, obj_index, ctx);
        }

        wasm_func& add_func(string export_name, const array_view<wasm_value_type>& params, const array_view<wasm_value_type>& results, const array_view<wasm_value_type>& locals, const array_view<u8>& body_wasm, context& ctx) {
            var type_index = add_type(params, results, ctx);

            ref func = push(ctx.func_specs, {
                .type_index = type_index,
                .locals     = locals,
                .body_wasm  = body_wasm
            });

            var index = ctx.func_imports.data.count + ctx.func_specs.data.count - 1;

            if (export_name.count > 0) {
                add_export(export_name, wasm_export_kind::func, index, ctx);
            }

            return func;
        }

        wasm_func& add_func(func_context& func_ctx) {
            using enum wasm_value_type;
            using_func_context func_ctx;

            var index = ctx.func_imports.data.count + ctx.func_specs.data.count;
            ref func = add_func(should_export ? id : view_of(""), params.data, results.data, locals.data, body.data, ctx);
            push(ctx.scope_funcs, { .id = func_ctx.id, .local_index = index, .params_count = params.data.count });
            return func;
        }

        void set_mem(uint min, uint max, context& ctx) {
            ctx.mem.min = min;
            ctx.mem.max = max;
        }

        uint add_data(const array_view<char>& text, context& ctx) {
            ref datas   = ctx.datas;
            ref offsets = ctx.data_offsets;

            var offset = 0u;
            if (datas.data.count > 0) {
                let last_data_i = datas.data.count - 1;

                offset = offsets.data[last_data_i] + datas.data[last_data_i].data.count;
            }

            push(datas, { .data = {(u8*)text.data, text.count} });
            push(offsets, offset);

            return offset;
        }

        void add_data(const char* text, context& ctx) {
            add_data(view_of(text), ctx);
        }

        uint add_param(wasm_value_type type, func_context& func_ctx) {
            assert(func_ctx.locals.data.count == 0); // add params before locals

            push(func_ctx.params, type);

            return func_ctx.params.data.count - 1;
        }

        variable add_local(string id, wasm_value_type type, func_context& func_ctx) {
            var index = func_ctx.params.data.count + func_ctx.locals.data.count;
            var var_ = variable {.id = id, .local_index = index};
            push(func_ctx.ctx.scope_vars, var_);
            push(func_ctx.locals, type);
            return var_;
        }

        array_view<u8> emit_wasm(context& ctx) {
            using_compilation_context ctx;

            emit_header(emitter);
            emit_type_section  (func_types.data, emitter);
            emit_import_section(imports   .data, emitter);
            emit_func_section  (func_specs.data, emitter);
            emit_memory_section(mem            , emitter);
            emit_export_section(exports   .data, emitter);
            emit_code_section  (func_specs.data, emitter);
            emit_data_section  (datas     .data, emitter);

            return emitter.wasm.data;
        }

        void error_var_not_found(string id) {
            printf("Variable %.*s not found.\n", (int)id.count, id.data);
            dbg_fail_return;
        }

        void error_func_not_found(string id) {
            printf("Function %.*s not found.\n", (int)id.count, id.data);
            dbg_fail_return;
        }
    }

    array_view<u8> compile_wasm(const ast& ast, arena& arena) {
        using namespace compilation;
        using namespace wasm_emit;
        using enum wasm_value_type;

        var ctx = make_context(arena);

        add_type({vt_i32     }, {   }, ctx);
        add_type({vt_i32, vt_i32}, {   }, ctx);

        add_import("env", "log_i32", 0, ctx);
        add_import("env", "log_str", 1, ctx);

        set_mem(1, 1, ctx);
        add_export("memory", wasm_export_kind::mem, 0, ctx);

        emit_u2str(ctx);
        emit_main(ast.root, ctx);

        return emit_wasm(ctx);
    }

    namespace compilation {
        void emit_main(node* root, context& ctx_) {
            using enum wasm_value_type;

            var func_ctx = make_func_context(view_of("main"), ctx_);
            using_func_context func_ctx;

            emit_block(root, func_ctx);
            emit(op_end, body);

            should_export = true;

            push(results, vt_i32);
            add_func(func_ctx);
        }

        void emit_block(node* chain, func_context& func_ctx) {
            tmp_compilation_scope(func_ctx.ctx);
            for(var node = chain; node; node = node->next)
                emit_node(*node, func_ctx);
        }

        void emit_node(node& node, func_context& func_ctx) {
            using namespace wasm_emit;
            using enum wasm_value_type;
            using enum wasm_opcode;
            using_func_context   func_ctx;
            using_compilation_context ctx;

            if (is_uint_literal(node)) {
                emit_const(node.uint_value, body);
                return;
            }

            if (is_str_literal(node)) {
                let str    = node.text;
                let offset = add_data(str, ctx);
                emit_const(offset   , body);
                emit_const(str.count, body);
                return;
            }

            if_var1(fn_id, get_fn_id(node)); else { dbg_fail_return; }
            var args_node_p = node.first_child;

            if (fn_id == print_id) {
                if_ref(arg, args_node_p); else { dbg_fail_return; }
                emit_node(arg, func_ctx);
                emit(op_call, 1, body);
                return;
            }

            if (fn_id == ret_id) {
                if_ref(arg, args_node_p); else { dbg_fail_return; }
                emit_node(arg, func_ctx);
                emit(op_return, body);
                return;
            }

            if (fn_id == decl_var_id) {
                if_ref(id_node, args_node_p); else { dbg_fail_return; }
                let var_ = add_local(id_node.text, vt_i32, func_ctx);

                if_ref(init_node, id_node.next) {
                    emit_node(init_node, func_ctx);
                    emit(op_local_set, var_.local_index, body);
                }
                return;
            }

            if (fn_id == ref_id) {
                emit_ref(args_node_p, func_ctx);
                return;
            }

            if (fn_id == assign_id) {
                if_var2(var_id_node, value_node, deref_list2(args_node_p)); else { dbg_fail_return; }
                let var_id = var_id_node.text;
                if_ref (v, find_var(var_id, ctx)); else { error_var_not_found(var_id); return; }
                emit_node(value_node, func_ctx);
                emit(op_local_set, v.local_index, body);
                return;
            }

            if (fn_id == add_id) {
                emit_bop(op_i32_add, args_node_p, func_ctx);
                return;
            }

            if (fn_id == sub_id) {
                emit_bop(op_i32_sub, args_node_p, func_ctx);
                return;
            }

            if (fn_id == eq_id) {
                emit_bop(op_i32_eq, args_node_p, func_ctx);
                return;
            }

            if (fn_id == neq_id) {
                emit_bop(op_i32_ne, args_node_p, func_ctx);
                return;
            }

            if (fn_id == lte_id) {
                emit_bop(op_i32_le_u, args_node_p, func_ctx);
                return;
            }

            if (fn_id == gte_id) {
                emit_bop(op_i32_ge_u, args_node_p, func_ctx);
                return;
            }

            if (fn_id == lt_id) {
                emit_bop(op_i32_lt_u, args_node_p, func_ctx);
                return;
            }

            if (fn_id == gt_id) {
                emit_bop(op_i32_gt_u, args_node_p, func_ctx);
                return;
            }

            if (fn_id == istr_id) {
                emit_istr(args_node_p, func_ctx);
                return;
            }

            if (fn_id == def_id) {
                emit_func_def(args_node_p, ctx);
                return;
            }

            if (fn_id == block_id) {
                emit(op_block, vt_void, body);
                emit_block(args_node_p, func_ctx);
                emit(op_end, body);
                return;
            }

            if (fn_id == if_id) {
                if_var2(cond_node, then_node, deref_list2(args_node_p)); else { dbg_fail_return; }
                emit_node(cond_node, func_ctx);
                emit(op_if, (u8)0x40, body);
                emit_node(then_node, func_ctx);
                emit(op_end, body);
                return;
            }

            if (fn_id == while_id) {
                emit_while_scope(body);
                if_var2(cond_node, body_node, deref_list2(args_node_p)); else { dbg_fail_return; }

                emit_node(cond_node, func_ctx);
                emit(op_i32_const, 0, body);
                emit(op_i32_eq      , body);
                emit(op_br_if ,   1u, body);

                emit_body(body_node, func_ctx);

                return;
            }

            if_ref(func, find_func(fn_id, ctx)); else { error_func_not_found(fn_id); return; }

            var arg_node_p = args_node_p;
            for (var param_i = 0u; param_i < func.params_count; ++param_i) {
                if_ref (arg, arg_node_p); else { dbg_fail_return; }
                emit_node(arg, func_ctx);
                arg_node_p = arg_node_p->next;
            }

            //TODO: deal with extra parameters?
            emit(op_call, func.local_index, body);
        }

        void emit_body(node& node, func_context& func_ctx) {
            if (is_func(node, block_id)) {
                emit_block(node.first_child, func_ctx);
            } else {
                emit_node(node, func_ctx);
            }
        }

        void emit_func_def(node* args, context& ctx_) {
            using namespace wasm_emit;
            using enum wasm_value_type;

            if_var3 (id_node, params_node, body_node, deref_list3(args)); else { dbg_fail_return; }

            let id_text = id_node.text;
            var func_ctx = make_func_context(id_text, ctx_);
            using_func_context func_ctx;
            using_compilation_context ctx;

            for (var node_p = params_node.first_child; node_p; node_p = node_p->next) {
                ref node = *node_p;
                if (is_func(node, decl_param_id)); else continue;
                add_param(vt_i32, func_ctx);
            }

            push(results, vt_i32);// TODO: get result types from code

            ref wasm_func = add_func(func_ctx);
            {
                tmp_scope(ctx);

                var param_index = 0u;
                for (var node_p = params_node.first_child; node_p; node_p = node_p->next) {
                    ref node = *node_p;
                    if (is_func(node, decl_param_id)); else continue;

                    if_ref(param_id_node, node.first_child); else { dbg_fail_return; }
                    let param_id = param_id_node.text;
                    var var_  = variable {.id = param_id, .local_index = param_index};
                    push(func_ctx.ctx.scope_vars, var_);
                    param_index++;
                }

                emit_body(body_node, func_ctx);
                emit(op_end, body);
            }

            wasm_func.locals    = locals.data;
            wasm_func.body_wasm = body  .data;
        }

        void emit_ref(node* args, func_context& func_ctx) {
            ref ctx  = func_ctx.ctx;
            ref body = func_ctx.body;

            if_ref (id_node, args); else { dbg_fail_return; }
            let id = id_node.text;
            if_ref (v, find_var(id, ctx)); else { error_var_not_found(id); return; }
            emit(op_local_get, v.local_index, body);
        }

        void emit_u2str(context & ctx_) {
            using namespace wasm_emit;
            using enum wasm_value_type;

            var func_ctx = make_func_context(view_of("u2str"), ctx_);
            using_func_context func_ctx;
            using_compilation_context ctx;

            var n_id          = add_param(vt_i32, func_ctx);
            var mem_offset_id = add_param(vt_i32, func_ctx);

            // if (n == 0)
            emit(op_local_get,     n_id, body);
            emit(op_i32_eqz            , body);
            emit(op_if       , (u8)0x40, body); {
                // push('0');
                emit(op_local_get, mem_offset_id, body);
                emit_const        ((u8)0x30, body); // 0x30 -- '0'
                emit(op_i32_store_8, 0u, 0u, body);

                // mem_offset++
                emit(op_local_get, mem_offset_id, body);
                emit_const        (            1, body);
                emit(op_i32_add                 , body);
                emit(op_local_set, mem_offset_id, body);

                emit(op_end, body);
            }

            // var d = 1000000000u;
            var d_id = add_local(view_of("d"), vt_i32, func_ctx).local_index;
            emit_const(1000000000u, body);
            emit(op_local_set, d_id, body);

            // while (true)
            {
                emit_while_scope(body);

                // if (d <= n) break;
                emit(op_local_get, d_id, body);
                emit(op_local_get, n_id, body);
                emit(op_i32_le_u       , body);
                emit(op_br_if    ,   1u, body);

                // d = d / 10;
                emit(op_local_get, d_id, body);
                emit_const        (  10, body);
                emit(op_i32_div_u      , body);
                emit(op_local_set, d_id, body);
            }

            // while(true)
            {
                emit_while_scope(body);

                // if (d == 0) break;
                emit(op_local_get, d_id, body);
                emit_const        (   0, body);
                emit(op_i32_eq         , body);
                emit(op_br_if    ,   1u, body);

                emit(op_local_get, mem_offset_id, body);

                // var digit = (n / d) % 10;
                emit(op_local_get, n_id, body);
                emit(op_local_get, d_id, body);
                emit(op_i32_div_u      , body);
                emit_const        (  10, body);
                emit(op_i32_rem_u      , body);

                // push(digit + '0'); // 0x30 -- zero
                emit_const          ( 0x30u, body);
                emit(op_i32_add            , body);
                emit(op_i32_store_8, 0u, 0u, body);

                // mem_offset++
                emit(op_local_get, mem_offset_id, body);
                emit_const        (            1, body);
                emit(op_i32_add                 , body);
                emit(op_local_set, mem_offset_id, body);

                // d = d / 10
                emit(op_local_get, d_id, body);
                emit_const        (  10, body);
                emit(op_i32_div_u      , body);
                emit(op_local_set, d_id, body);
            }

            // return mem_offset
            emit(op_local_get, mem_offset_id, body);
            emit(op_end                     , body);

            push(results, vt_i32);
            add_func(func_ctx);
        }

        void emit_istr(node* args, func_context& func_ctx) {
            using_func_context   func_ctx;
            using_compilation_context ctx;

            if_ref(uint_to_str_func, find_func(view_of("u2str"), ctx)); else { dbg_fail_return; }
            let uint_to_str_id = uint_to_str_func.local_index;

            // write to memory, put the string on the stack
            // convert uints to strings
            var mem_offset_id = add_local(view_of("mem_offset"), wasm_value_type::vt_i32, func_ctx).local_index;
            emit_const        (         1024, body);
            emit(op_local_set, mem_offset_id, body);

            for(var node_p = args; node_p; node_p = node_p->next) {
                ref node = *node_p;
                if (is_str_literal(node)) {
                    let str = node.text;
                    var str_offset = add_data(str, ctx);

                    // copy literal to the buffer
                    emit(op_local_get, mem_offset_id, body);
                    emit_const        (str_offset   , body);
                    emit_const        (str.count    , body);
                    emit_memcpy(                      body);

                    // increment buffer offset
                    emit(op_local_get, mem_offset_id, body);
                    emit_const        (str.count    , body);
                    emit(op_i32_add                 , body);
                    emit(op_local_set, mem_offset_id, body);
                    continue;
                }

                emit_node(node, func_ctx);
                emit(op_local_get,  mem_offset_id, body);
                emit(op_call     , uint_to_str_id, body);
                emit(op_local_set,  mem_offset_id, body);
            }

            // put string offset and count on the stack
            emit_const        (         1024, body);
            emit(op_local_get, mem_offset_id, body);
            emit_const        (         1024, body);
            emit(op_i32_sub                 , body);
        }

        void emit_bop(wasm_opcode opcode, node* args_node_p, func_context& func_ctx) {
            if_var2(a_node, b_node, deref_list2(args_node_p)); else { dbg_fail_return; }
            emit_node(a_node, func_ctx);
            emit_node(b_node, func_ctx);
            emit(opcode, func_ctx.body);
        }
    }
}

#endif //FRANCA2_COMPUTE_COMPILATION_H

#pragma clang diagnostic pop