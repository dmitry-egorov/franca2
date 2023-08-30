#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_COMPILATION_H
#define FRANCA2_COMPUTE_COMPILATION_H
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/arrayds.h"
#include "utility/hex_ex.h"
#include "utility/wasm_emit.h"

#include "compute_asts.h"

namespace compute_asts {
    arrayv<u8> emit_wasm(const ast&, arena& = gta);

    namespace compilation {
        using namespace wasm_emit;

        struct scope {
            uint prev_scope_vars_count;
            uint prev_scope_funcs_count;
        };

        struct var_ {
            string id;
            uint   local_index;
        };

        struct func {
            string id;
            uint index;

            arrayd<wasm_value_type> params ;
            arrayd<wasm_value_type> results;
            arrayd<wasm_value_type> locals ;
            stream body;

            bool   imported;
            string import_module;

            bool exported;
        };

        struct context {
            arena& arena;

            wasm_memory mem;
            string exported_memory_id;

            arrayd<func > funcs;
            arrayd<var_ >  vars_in_scope;
            arrayd<func*> funcs_in_scope;
            arrayd<func*> func_stack;
            arrayd<scope> scopes;

            arrayd<wasm_data> datas;
            arrayd<uint     > data_offsets;

            wasm_emitter emitter;
        };

        void emit_fn_main (node *, context &);
        void emit_fn_u2str(context &);

        void gather_defs(node*, context&);

        void emit_scoped_chain    (node*, context&);
        void emit_node            (node&, context&);
        void emit_body            (node&, context&);
        void emit_func_def        (node*, context&);
        void emit_ref             (node*, context&);
        void emit_istr            (node*, context&);
        void emit_bop(wasm_opcode, node*, context&);

        context make_context(arena& arena);

        void push_scope(context& ctx);
        void  pop_scope(context& ctx);
        #define tmp_compilation_scope(ctx) push_scope(ctx); defer { pop_scope(ctx); }

        var_* find_var (string id, context& ctx);
        func* find_func(string id, context& ctx);

        func& declare_import(const char* module_id, const char* id, const std::initializer_list<wasm_value_type>& params, const std::initializer_list<wasm_value_type>& results, context& ctx);
        func& declare_import(string module_id, string id, const arrayv<wasm_value_type>& params, const arrayv<wasm_value_type>& results, context& ctx);

        func& declare_func(const char* id, context& ctx);
        func& declare_func(string id, context& ctx);

        void begin_func_definition(func& func, context& ctx);
        void   end_func_definition(context& ctx);
        #define tmp_func_definition_scope(func, ctx) begin_func_definition(func, ctx); defer { end_func_definition(ctx); }

        func* get_curr_func(context& ctx);

        void set_mem(const char* export_name, uint min, uint max, context& ctx);
        void set_mem(string export_name, uint min, uint max, context& ctx);

        void add_data(const char* text, context& ctx);
        uint add_data(const string& text, context& ctx);

        var_ add_local(string id, wasm_value_type type, context& ctx);

        void error_var_not_found (string id);
        void error_func_not_found(string id);

        uint add_param (wasm_value_type type, func& func);
        void add_result(wasm_value_type type, func& func);
    }

    arrayv<u8> emit_wasm(const ast& ast, arena& arena) {
        using namespace compilation;
        using namespace wasm_emit;
        using enum wasm_value_type;

        var ctx = make_context(arena);

        declare_import("env", "log_i32", {vt_i32        }, {   }, ctx);
        declare_import("env", "log_str", {vt_i32, vt_i32}, {   }, ctx);

        set_mem("memory", 1, 1, ctx);

        emit_fn_u2str(ctx);
        emit_fn_main(ast.root, ctx);

        // emit wasm
        ref emitter = ctx.emitter;

        var func_types = alloc<wasm_func_type>(arena, ctx.funcs.data.count);
        var imports    = make_darray<wasm_func_import>(32, arena);
        var exports    = make_darray<wasm_export>(32, arena);
        var func_specs = make_darray<wasm_func>(256, arena);

        push(exports, wasm_export {.name = ctx.exported_memory_id, .kind = wasm_export_kind::mem, .obj_index = 0});

        for (var i = 0u; i < ctx.funcs.data.count; ++i) {
            let func = ctx.funcs.data[i];
            func_types[i] = { // TODO: distinct
                .params  = func.params .data,
                .results = func.results.data,
            };

            if (func.imported) {
                push(imports, {.module = func.import_module, .name = func.id, .type_index = i});
                assert(func.body.data.count == 0);
                continue;
            }

            push(func_specs, wasm_func {
                .type_index = i,
                .locals     = func.locals.data,
                .body       = func.body  .data,
            });

            if (func.exported) {
                push(exports, wasm_export {.name = func.id, .kind = wasm_export_kind::func, .obj_index = func.index});
            }
        }

        emit_header(emitter);
        emit_type_section  (func_types, emitter);
        emit_import_section(imports   .data, emitter);
        emit_func_section  (func_specs.data, emitter);
        emit_memory_section(ctx.mem        , emitter);
        emit_export_section(exports   .data, emitter);
        emit_code_section  (func_specs.data, emitter);
        emit_data_section  (ctx.datas .data, emitter);

        return emitter.wasm.data;
    }

    namespace compilation {
        void emit_fn_main(node* root, context & ctx) {
            using enum wasm_value_type;

            ref func = declare_func("main", ctx);
            add_result(vt_i32, func);
            func.exported = true;

            tmp_func_definition_scope(func, ctx);
            emit_scoped_chain(root, ctx);
            emit(op_end, func.body);
        }

        void emit_scoped_chain(node* chain, context& ctx) {
            gather_defs(chain, ctx); // TODO: comptime here?

            for(var node = chain; node; node = node->next)
                emit_node(*node, ctx);
        }

        void emit_node(node& node, context& ctx) {
            using namespace wasm_emit;
            using enum wasm_value_type;
            using enum wasm_opcode;

            if_ref(curr_func, get_curr_func(ctx)); else { dbg_fail_return; }
            ref body = curr_func.body;

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
                emit_node(arg, ctx);
                emit(op_call, 1, body);
                return;
            }

            if (fn_id == ret_id) {
                if_ref(arg, args_node_p); else { dbg_fail_return; }
                emit_node(arg, ctx);
                emit(op_return, body);
                return;
            }

            if (fn_id == decl_var_id) {
                if_ref(id_node, args_node_p); else { dbg_fail_return; }
                let var_ = add_local(id_node.text, vt_i32, ctx);

                if_ref(init_node, id_node.next) {
                    emit_node(init_node, ctx);
                    emit(op_local_set, var_.local_index, body);
                }
                return;
            }

            if (fn_id == ref_id) {
                emit_ref(args_node_p, ctx);
                return;
            }

            if (fn_id == assign_id) {
                if_var2(var_id_node, value_node, deref_list2(args_node_p)); else { dbg_fail_return; }
                let var_id = var_id_node.text;
                if_ref (v, find_var(var_id, ctx)); else { error_var_not_found(var_id); return; }
                emit_node(value_node, ctx);
                emit(op_local_set, v.local_index, body);
                return;
            }

            if (fn_id == add_id) { emit_bop(op_i32_add , args_node_p, ctx); return; }
            if (fn_id == sub_id) { emit_bop(op_i32_sub , args_node_p, ctx); return; }
            if (fn_id ==  eq_id) { emit_bop(op_i32_eq  , args_node_p, ctx); return; }
            if (fn_id ==  ne_id) { emit_bop(op_i32_ne  , args_node_p, ctx); return; }
            if (fn_id ==  le_id) { emit_bop(op_i32_le_u, args_node_p, ctx); return; }
            if (fn_id ==  ge_id) { emit_bop(op_i32_ge_u, args_node_p, ctx); return; }
            if (fn_id ==  lt_id) { emit_bop(op_i32_lt_u, args_node_p, ctx); return; }
            if (fn_id ==  gt_id) { emit_bop(op_i32_gt_u, args_node_p, ctx); return; }

            if (fn_id == istr_id) { emit_istr(args_node_p, ctx); return; }
            if (fn_id ==  def_id) { emit_func_def(args_node_p, ctx); return; }

            if (fn_id == block_id) {
                tmp_compilation_scope(ctx);

                emit(op_block, vt_void, body);
                emit_scoped_chain(args_node_p, ctx);
                emit(op_end, body);
                return;
            }

            if (fn_id == if_id) {
                if_var2(cond_node, then_node, deref_list2(args_node_p)); else { dbg_fail_return; }
                emit_node(cond_node, ctx);
                emit(op_if, (u8)0x40, body);

                tmp_compilation_scope(ctx);
                emit_body(then_node, ctx);
                emit(op_end, body);
                return;
            }

            if (fn_id == while_id) {
                emit_while_scope(body);
                if_var2(cond_node, body_node, deref_list2(args_node_p)); else { dbg_fail_return; }

                emit_node(cond_node, ctx);
                emit(op_i32_const, 0, body);
                emit(op_i32_eq      , body);
                emit(op_br_if ,   1u, body);

                tmp_compilation_scope(ctx);
                emit_body(body_node, ctx);
                return;
            }

            if_ref(func, find_func(fn_id, ctx)); else { error_func_not_found(fn_id); return; }

            var arg_node_p = args_node_p;
            for (var param_i = 0u; param_i < func.params.data.count; ++param_i) {
                if_ref (arg, arg_node_p); else { dbg_fail_return; }
                emit_node(arg, ctx);
                arg_node_p = arg_node_p->next;
            }

            //TODO: #cleanup: deal with extra parameters?
            emit(op_call, func.index, body);
        }

        void emit_body(node& node, context& ctx) {
            if (is_func(node, block_id)) {
                emit_scoped_chain(node.first_child, ctx);
            } else {
                emit_node(node, ctx);
            }
        }

        void gather_defs(node* chain, context& ctx) {
            //TODO: maybe need to rethink this and gather definitions when creating a new scope

            for (var node_p = chain; node_p; node_p = node_p->next) {
                ref node = *node_p;
                if (is_func(node, def_id)); else continue;

                if_var2 (id_node, params_node, deref_list2(node.first_child)); else { dbg_fail_return; }

                let id_text = id_node.text;
                ref func = declare_func(id_text, ctx);

                for (var param_node_p = params_node.first_child; param_node_p; param_node_p = param_node_p->next) {
                    ref param_node = *param_node_p;
                    if (is_func(param_node, decl_param_id)); else continue;
                    add_param(vt_i32, func);
                }

                add_result(vt_i32, func); // TODO: get result types from code

                //TODO: recourse to allow for function definitions inside expressions
            }
        }

        void emit_func_def(node* args, context& ctx) {
            using namespace wasm_emit;
            using enum wasm_value_type;

            if_var3(id_node, params_node, body_node, deref_list3(args)); else { dbg_fail_return; }
            if_ref (func, find_func(id_node.text, ctx)); else { dbg_fail_return; }

            tmp_func_definition_scope(func, ctx);

            var param_index = 0u;
            for (var node_p = params_node.first_child; node_p; node_p = node_p->next) {
                ref node = *node_p;
                if (is_func(node, decl_param_id)); else continue;

                if_ref(param_id_node, node.first_child); else { dbg_fail_return; }
                push(ctx.vars_in_scope, {.id = param_id_node.text, .local_index = param_index});
                param_index++;
            }

            emit_body(body_node, ctx);
            emit(op_end, func.body);
        }

        void emit_ref(node* args, context& ctx) {
            if_ref(id_node, args); else { dbg_fail_return; }
            let id = id_node.text;
            if_ref(v, find_var(id, ctx)); else { error_var_not_found(id); return; }
            if_ref(curr_func, get_curr_func(ctx)); else { dbg_fail_return; }

            emit(op_local_get, v.local_index, curr_func.body);
        }

        void emit_fn_u2str(context& ctx) {
            using namespace wasm_emit;
            using enum wasm_value_type;

            ref func = declare_func(view("u2str"), ctx);

            var n_id          = add_param(vt_i32, func);
            var mem_offset_id = add_param(vt_i32, func);
            add_result(vt_i32, func);

            tmp_func_definition_scope(func, ctx);

            ref body = func.body;

            // if (n == 0)
            emit(op_local_get,     n_id, body);
            emit(op_i32_eqz            , body);
            emit(op_if       ,  vt_void, body); {
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
            var d_id = add_local(view("d"), vt_i32, ctx).local_index;
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
        }

        void emit_istr(node* args, context& ctx) {
            // TODO: implement with string builder

            if_ref(uint_to_str_func, find_func(view("u2str"), ctx)); else { dbg_fail_return; }
            let uint_to_str_id = uint_to_str_func.index;

            if_ref(func, get_curr_func(ctx)); else { dbg_fail_return; }
            ref body = func.body;

            // write to memory, put the string on the stack
            // convert uints to strings

            // var mem_offset = 1024;
            var mem_offset_id = add_local(view("mem_offset"), wasm_value_type::vt_i32, ctx).local_index;
            emit_const        (         1024, body);
            emit(op_local_set, mem_offset_id, body);

            for(var node_p = args; node_p; node_p = node_p->next) {
                ref node = *node_p;
                if (is_str_literal(node)) {
                    let str = node.text;
                    var str_offset = add_data(str, ctx);

                    // memcpy(mem_offset, str_offset, str.count);
                    emit(op_local_get, mem_offset_id, body);
                    emit_const        (str_offset   , body);
                    emit_const        (str.count    , body);
                    emit_memcpy                      (body);

                    // mem_offset += str.count;
                    emit(op_local_get, mem_offset_id, body);
                    emit_const        (str.count    , body);
                    emit(op_i32_add                 , body);
                    emit(op_local_set, mem_offset_id, body);
                    continue;
                }

                // mem_offset = uint_to_str(fn(), mem_offset);
                emit_node(node, ctx);
                emit(op_local_get,  mem_offset_id, body);
                emit(op_call     , uint_to_str_id, body);
                emit(op_local_set,  mem_offset_id, body);
            }

            // return string { 1024, mem_offset - 1024 };
            emit_const        (         1024, body);
            emit(op_local_get, mem_offset_id, body);
            emit_const        (         1024, body);
            emit(op_i32_sub                 , body);
        }

        void emit_bop(wasm_opcode opcode, node* args_node_p, context& ctx) {
            if_var2(a_node, b_node, deref_list2(args_node_p)); else { dbg_fail_return; }

            if_ref(func, get_curr_func(ctx)); else { dbg_fail_return; }

            emit_node(a_node, ctx);
            emit_node(b_node, ctx);
            emit(opcode, func.body);
        }

        context make_context(arena& arena) {
            using namespace wasm_emit;
            return {
                .arena          = arena,
                .funcs          = make_darray<func>(256, arena),
                . vars_in_scope = make_darray<var_>(64, arena),
                .funcs_in_scope = make_darray<func*>(128, arena),
                .func_stack     = make_darray<func*>(16, arena),
                .scopes         = make_darray<scope>(32, arena),
                .datas          = make_darray<wasm_data>(32, arena),
                .data_offsets   = make_darray<uint>(32, arena),
                .emitter        = make_emitter(arena)
            };
        }

        void push_scope(context& ctx) {
            push(ctx.scopes, {
                .prev_scope_vars_count  = ctx.vars_in_scope.data.count,
                .prev_scope_funcs_count = ctx.funcs_in_scope.data.count
            });
        }

        void pop_scope(context& ctx) {
            let scope = pop(ctx.scopes);
            ctx. vars_in_scope.data.count = scope.prev_scope_vars_count;
            ctx.funcs_in_scope.data.count = scope.prev_scope_funcs_count;
        }

        var_* find_var(string id, context& ctx) {
            for (var i = (int)ctx.vars_in_scope.data.count - 1; i >= 0; i--) {
                ref v = ctx.vars_in_scope.data[i];
                if (v.id == id) return &v;
            }
            return nullptr;
        }

        func* find_func(string id, context& ctx) {
            for (var i = (int)ctx.funcs_in_scope.data.count - 1; i >= 0; i--) {
                if_ref(v, ctx.funcs_in_scope.data[i]); else { dbg_fail_return nullptr; }
                if (v.id == id) return &v;
            }
            return nullptr;
        }

        func& declare_import(const char* module_id, const char* id, const std::initializer_list<wasm_value_type>& params, const std::initializer_list<wasm_value_type>& results, context& ctx) {
            return declare_import(view(module_id), view(id), view_of(params), view_of(results), ctx);
        }

        func& declare_import(string module_id, string id, const arrayv<wasm_value_type>& params,
                             const arrayv<wasm_value_type>& results, context& ctx) {
            let index = ctx.funcs.data.count;
            ref func  = push(ctx.funcs, {
                .id      = id,
                .index   = index,
                .params  = { .data = params , .capacity = params.count, .arena = &ctx.arena },
                .results = { .data = results, .capacity = params.count, .arena = &ctx.arena },
                .imported = true,
                .import_module = module_id,
            });

            push(ctx.funcs_in_scope, &func);

            return func;
        }

        func& declare_func(const char* id, context& ctx) {
            return declare_func(view(id), ctx);
        }

        func& declare_func(string id, context& ctx) {
            let index = ctx.funcs.data.count;
            ref func  = push(ctx.funcs, {
                .id      = id,
                .index   = index,
                .params  = make_darray<wasm_value_type>(4, ctx.arena),
                .results = make_darray<wasm_value_type>(2, ctx.arena),
                .locals  = make_darray<wasm_value_type>(32, ctx.arena),
                .body    = make_stream(32, ctx.arena),
            });

            push(ctx.funcs_in_scope, &func);

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

        func* get_curr_func(context& ctx) {
            var f_pp = last_of(ctx.func_stack);
            if_ref(f_p, f_pp); else { dbg_fail_return nullptr; }
            return f_p;
        }

        void set_mem(const char* export_name, uint min, uint max, context& ctx) {
            set_mem(view(export_name), min, max, ctx);
        }

        void set_mem(string export_name, uint min, uint max, context& ctx) {
            ctx.mem.min = min;
            ctx.mem.max = max;
            ctx.exported_memory_id = export_name;
        }

        void add_data(const char* text, context& ctx) {
            add_data(view(text), ctx);
        }

        uint add_data(const string& text, context& ctx) {
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

        var_ add_local(string id, wasm_value_type type, context& ctx) {
            if_ref(func, get_curr_func(ctx)); else { dbg_fail_return {}; }
            push(func.locals, type);

            var index = func.params.data.count + func.locals.data.count - 1;
            var v = var_ {.id = id, .local_index = index};
            push(ctx.vars_in_scope, v);
            return v;
        }

        void error_var_not_found(string id) {
            printf("Variable %.*s not found.\n", (int)id.count, id.data);
            dbg_fail_return;
        }

        void error_func_not_found(string id) {
            printf("Function %.*s not found.\n", (int)id.count, id.data);
            dbg_fail_return;
        }

        uint add_param(wasm_value_type type, func& func) {
            assert(func.locals.data.count == 0); // add params before locals
            push(func.params, type);
            return func.params.data.count - 1;
        }

        void add_result(wasm_value_type type, func& func) {
            push(func.results, type);
        }
    }
}

#endif //FRANCA2_COMPUTE_COMPILATION_H

#pragma clang diagnostic pop