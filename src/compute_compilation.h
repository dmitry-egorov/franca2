#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_COMPILATION_H
#define FRANCA2_COMPUTE_COMPILATION_H
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/arr_dyns.h"
#include "utility/hex_ex.h"
#include "utility/wasm_emit.h"

#include "compute_asts.h"

namespace compute_asts {
    arr_view<u8> emit_wasm(const ast&, arena& = gta);

    namespace compilation {
        using namespace wasm_emit;

        using enum primitive_type;

        struct scope {
            uint prev_locals_in_scope_count;
            uint prev_funcs_in_scope_count;
        };

        struct local {
            string id;
            uint   index;
            primitive_type type;
        };

        struct func {
            string id;
            uint index;

            bool is_inline;

            arr_dyn<primitive_type> params ;
            arr_dyn<primitive_type> locals ;
            arr_dyn<primitive_type> results;
            stream body;

            bool   imported;
            string import_module;

            bool exported;
        };

        struct context {
            arr_dyn<func >   funcs;
            arr_dyn<func*>   func_stack;
            arr_dyn<scope>  scope_stack;
            arr_dyn<func*>  funcs_in_scope;
            arr_dyn<local> locals_in_scope;

            stream data;

            wasm_memory  mem;
            string exported_memory_id;

            wasm_emitter emitter;
            arena& arena;
        };

        context make_context(arena& arena);

        void emit_fn_main(node*, context &);

        void gather_defs(node*, context&);

        void emit_scoped_chain               (node*, context&);
        void emit_node                       (node&, context&);
        void emit_body                       (node&, context&);
        void emit_func_def                   (node*, context&);
        void emit_func_call(const func& func, node*, context&);
        void emit_ref                        (node*, context&);
        void emit_istr                       (node*, context&);
        void emit_chr                        (node*, context&);
        void emit_minus                      (node*, context&);
        void emit_bop           (wasm_opcode, node*, context&);
        void emit_bop_a         (wasm_opcode, node*, context&);

        void emit_func_call(const string& id, context&);

        void push_scope(context& ctx);
        void  pop_scope(context& ctx);
        #define tmp_compilation_scope(ctx) push_scope(ctx); defer { pop_scope(ctx); }

        auto find_local(string id, context& ctx) -> local*;
        auto find_func (string id, context& ctx) -> func*;
        auto find_func_index(string id, context& ctx) -> ret1<uint>;

        func& declare_import(const char* module_id, const char* id, const std::initializer_list<primitive_type>& params, const std::initializer_list<primitive_type>& results, context& ctx);
        func& declare_import(string module_id, string id, const arr_view<primitive_type>& params, const arr_view<primitive_type>& results, context& ctx);

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

        local add_local(string id, primitive_type type, context& ctx);

        void error_local_not_found (string id);
        void error_func_not_found(string id);

        uint add_param (primitive_type type, func& func);
        void add_result(primitive_type type, func& func);

         primitive_type primitive_type_by(string name);
        wasm_value_type wasm_type_of(primitive_type type);
    }

    arr_view<u8> emit_wasm(const ast& ast, arena& arena) {
        using namespace compilation;
        using namespace wasm_emit;

        var arena_start_bytes = arena.used_bytes;

        var ctx = make_context(arena);

        declare_import("env", "print_str", {pt_u32, pt_u32}, {   }, ctx);

        set_mem("memory", 1, 1, ctx);

        emit_fn_main(ast.root, ctx);

        // emit wasm
        ref emitter = ctx.emitter;

        var func_types = alloc<wasm_func_type>(arena, ctx.funcs.data.count);
        var imports    = make_arr_dyn<wasm_func_import>( 32, arena);
        var exports    = make_arr_dyn<wasm_export     >( 32, arena);
        var func_specs = make_arr_dyn<wasm_func       >(256, arena);

        push(exports, wasm_export {.name = ctx.exported_memory_id, .kind = wasm_export_kind::mem, .obj_index = 0});

        for (var i = 0u; i < ctx.funcs.data.count; ++i) {
            let func = ctx.funcs.data[i];
            func_types[i] = { // TODO: distinct
                .params  = alloc_g<wasm_value_type>(arena, func.params .data.count),
                .results = alloc_g<wasm_value_type>(arena, func.results.data.count),
            };

            for (var pi = 0u; pi < func.params.data.count; pi += 1)
                func_types[i].params[pi] = wasm_type_of(func.params.data[pi]);

            for (var ri = 0u; ri < func.results.data.count; ri += 1)
                func_types[i].results[ri] = wasm_type_of(func.results.data[ri]);

            if (func.imported) {
                push(imports, {.module = func.import_module, .name = func.id, .type_index = i});
                assert(func.body.data.count == 0);
                continue;
            }

            var wasm_func = wasm_emit::wasm_func {
                .type_index = i,
                .locals     = alloc_g<wasm_value_type>(arena, func.locals .data.count),
                .body       = func.body  .data,
            };

            for (var li = 0u; li < func.locals.data.count; li += 1)
                wasm_func.locals[li] = wasm_type_of(func.locals.data[li]);

            push(func_specs, wasm_func);

            if (func.exported) {
                push(exports, wasm_export {.name = func.id, .kind = wasm_export_kind::func, .obj_index = func.index});
            }
        }

        var datas = view({wasm_data{ctx.data.data}});

        printf("func types: %zu\n", func_types.count);
        printf("imports   : %zu\n", imports.data.count);
        printf("exports   : %zu\n", exports.data.count);
        printf("funcs     : %zu\n", func_specs.data.count);
        printf("wasm:\n");

        emit_header(emitter);
        emit_type_section  (func_types     , emitter);
        emit_import_section(imports   .data, emitter);
        emit_func_section  (func_specs.data, emitter);
        emit_memory_section(ctx.mem        , emitter);
        emit_export_section(exports   .data, emitter);
        emit_code_section  (func_specs.data, emitter);
        emit_data_section  (datas          , emitter);

        let wasm = emitter.wasm.data;
        print_hex(wasm);

        printf("wasm size  : %zu\n", wasm.count);
        printf("memory used: %zu\n", arena.used_bytes - arena_start_bytes);

        return wasm;
    }

    namespace compilation {
        void emit_fn_main(node* root, context & ctx) {
            using enum primitive_type;

            ref func = declare_func("main", ctx);
            add_result(pt_u32, func);
            func.exported = true;

            tmp_func_definition_scope(func, ctx);
            emit_scoped_chain(root, ctx);
            emit(op_i32_const, 0u, func.body);
            emit(op_return, func.body);
            emit(op_end, func.body);
        }

        void emit_scoped_chain(node* chain, context& ctx) {
            gather_defs(chain, ctx); // TODO: comptime here?

            for(var node = chain; node; node = node->next)
                emit_node(*node, ctx);
        }

        void emit_node(node& node, context& ctx) {
            using namespace wasm_emit;
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

            if (fn_id == as_id) {
                if_var2(arg_node, type_node, deref2(args_node_p)); else { dbg_fail_return; }
                emit_node(arg_node, ctx);
                node.value_type = primitive_type_by(type_node.text);
                return;
            }

            if (fn_id == ret_id) {
                if_ref(arg, args_node_p); else { dbg_fail_return; }
                emit_node(arg, ctx);
                emit(op_return, body);
                return;
            }

            if (fn_id == decl_local_id) {
                if_var2(id_node, type_node, deref2(args_node_p)); else { dbg_fail_return; }
                let local = add_local(id_node.text, primitive_type_by(type_node.text), ctx);

                if_ref(init_node, type_node.next) {
                    emit_node(init_node, ctx);
                    emit(op_local_set, local.index, body);
                }
                return;
            }

            if (fn_id == ref_id) {
                emit_ref(args_node_p, ctx);
                return;
            }

            if (fn_id == store_u8_id) {
                if_var2(mem_offset_node, value_node, deref2(args_node_p)); else { dbg_fail_return; }
                emit_node(mem_offset_node, ctx);
                emit_node(     value_node, ctx);
                emit(op_i32_store_8, 0u, 0u, body);
                return;
            }

            if (fn_id == assign_id) {
                if_var2(var_id_node, value_node, deref2(args_node_p)); else { dbg_fail_return; }
                let var_id = var_id_node.text;
                if_ref (local, find_local(var_id, ctx)); else { error_local_not_found(var_id); return; }
                emit_node(value_node, ctx);
                emit(op_local_set, local.index, body);
                return;
            }

            if (fn_id ==   sub_id) { emit_minus(args_node_p, ctx); return; }
            if (fn_id ==   add_id) { emit_bop  (op_i32_add  , args_node_p, ctx); return; }
            if (fn_id ==   mul_id) { emit_bop  (op_i32_mul  , args_node_p, ctx); return; }
            if (fn_id ==   div_id) { emit_bop  (op_i32_div_u, args_node_p, ctx); return; }
            if (fn_id ==   rem_id) { emit_bop  (op_i32_rem_u, args_node_p, ctx); return; }
            if (fn_id == add_a_id) { emit_bop_a(op_i32_add  , args_node_p, ctx); return; }
            if (fn_id == sub_a_id) { emit_bop_a(op_i32_sub  , args_node_p, ctx); return; }
            if (fn_id == mul_a_id) { emit_bop_a(op_i32_mul  , args_node_p, ctx); return; }
            if (fn_id == div_a_id) { emit_bop_a(op_i32_div_u, args_node_p, ctx); return; }
            if (fn_id == rem_a_id) { emit_bop_a(op_i32_rem_u, args_node_p, ctx); return; }
            if (fn_id ==    eq_id) { emit_bop  (op_i32_eq   , args_node_p, ctx); return; }
            if (fn_id ==    ne_id) { emit_bop  (op_i32_ne   , args_node_p, ctx); return; }
            if (fn_id ==    le_id) { emit_bop  (op_i32_le_u , args_node_p, ctx); return; }
            if (fn_id ==    ge_id) { emit_bop  (op_i32_ge_u , args_node_p, ctx); return; }
            if (fn_id ==    lt_id) { emit_bop  (op_i32_lt_u , args_node_p, ctx); return; }
            if (fn_id ==    gt_id) { emit_bop  (op_i32_gt_u , args_node_p, ctx); return; }

            if (fn_id == istr_id) { emit_istr(args_node_p, ctx); return; }
            if (fn_id ==  chr_id) { emit_chr (args_node_p, ctx); return; }
            if (fn_id ==  def_id) { emit_func_def(args_node_p, ctx); return; }

            if (fn_id == block_id) {
                tmp_compilation_scope(ctx);

                emit(op_block, vt_void, body);
                emit_scoped_chain(args_node_p, ctx);
                emit(op_end, body);
                return;
            }

            if (fn_id == if_id) {
                if_var2(cond_node, then_node, deref2(args_node_p)); else { dbg_fail_return; }
                emit_node(cond_node, ctx);
                emit(op_if, (u8)0x40, body);

                tmp_compilation_scope(ctx);
                emit_body(then_node, ctx);
                emit(op_end, body);
                return;
            }

            if (fn_id == while_id) {
                emit_while_scope(body);
                if_var2(cond_node, body_node, deref2(args_node_p)); else { dbg_fail_return; }

                emit_node(cond_node, ctx);
                emit(op_i32_eqz    , body);
                emit(op_br_if  , 1u, body);

                tmp_compilation_scope(ctx);
                emit_body(body_node, ctx);
                return;
            }

            if (fn_id == print_id) {
                emit_istr(args_node_p, ctx);
                emit_func_call(view("print_str"), ctx);
                return;
            }

            if_ref(func, find_func(fn_id, ctx)); else { error_func_not_found(fn_id); return; }
            emit_func_call(func, args_node_p, ctx);
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

                if_var3(id_node, params_node, type_node, deref3(node.first_child)); else { dbg_fail_return; }

                let id_text = id_node.text;
                ref func = declare_func(id_text, ctx);

                for (var param_node_p = params_node.first_child; param_node_p; param_node_p = param_node_p->next) {
                    ref param_node = *param_node_p;
                    if (is_func(param_node, decl_param_id)); else continue;
                    if_var2(__, prm_type_node, deref2(param_node.first_child)); else { dbg_fail_return; }

                    add_param(primitive_type_by(prm_type_node.text), func);
                }

                add_result(primitive_type_by(type_node.text), func); // TODO: get result types from code

                //TODO: recurse to allow for function definitions inside expressions
            }
        }

        void emit_func_def(node* args, context& ctx) {
            using namespace wasm_emit;
            using enum wasm_value_type;

            if_var4(id_node, params_node, type_node, body_node, deref4(args)); else { dbg_fail_return; }
            if_ref (func, find_func(id_node.text, ctx)); else { dbg_fail_return; }

            tmp_func_definition_scope(func, ctx);

            var param_index = 0u;
            for (var node_p = params_node.first_child; node_p; node_p = node_p->next) {
                ref node = *node_p;
                if (is_func(node, decl_param_id)); else continue;

                if_ref(param_id_node, node.first_child); else { dbg_fail_return; }
                push(ctx.locals_in_scope, {
                    .id    = param_id_node.text,
                    .index = param_index,
                    .type  = primitive_type_by(type_node.text)
                });
                param_index++;
            }

            emit_body(body_node, ctx);
            emit(op_end, func.body);
        }

        void emit_func_call(const func& func, node* args, context& ctx) {
            //TODO: #cleanup: deal with extra parameters?
            var arg_node_p = args;
            for (var param_i = 0u; param_i < func.params.data.count; ++param_i) {
                if_ref (arg, arg_node_p); else { dbg_fail_return; }
                emit_node(arg, ctx);
                arg_node_p = arg_node_p->next;
            }

            if_ref(curr_func, get_curr_func(ctx)); else { dbg_fail_return; }

            if (!func.is_inline) {
                emit(op_call, func.index, curr_func.body);
                return;
            }

            //TODO: inline function
        }

        void emit_ref(node* args, context& ctx) {
            if_ref(id_node, args); else { dbg_fail_return; }
            let id = id_node.text;
            if_ref(local, find_local(id, ctx)); else { error_local_not_found(id); return; }
            if_ref(curr_func, get_curr_func(ctx)); else { dbg_fail_return; }

            emit(op_local_get, local.index, curr_func.body);
        }

        void emit_istr(node* args, context& ctx) {
            // TODO: implement with string builder
            if_var1(u32_to_str_id, find_func_index(view("u32_to_str"), ctx)); else { dbg_fail_return; }
            if_var1(i32_to_str_id, find_func_index(view("i32_to_str"), ctx)); else { dbg_fail_return; }

            if_ref(func, get_curr_func(ctx)); else { dbg_fail_return; }
            ref body = func.body;

            // write to memory, put the string on the stack
            // convert uints to strings

            // var mem_offset = 1024;
            var dst_ptr_id = add_local(view("mem_offset"), pt_u32, ctx).index;
            emit_const        (  1024, body);
            emit(op_local_set, dst_ptr_id, body);

            for(var node_p = args; node_p; node_p = node_p->next) {
                ref node = *node_p;
                if (is_str_literal(node)) {
                    let str = node.text;
                    var str_offset = add_data(str, ctx);

                    // memcpy(dst_ptr, str_offset, str.count);
                    emit(op_local_get, dst_ptr_id, body);
                    emit_const        (str_offset, body);
                    emit_const        (str.count , body);
                    emit_memcpy                   (body);

                    // mem_offset += str.count;
                    emit(op_local_get, dst_ptr_id, body);
                    emit_const         (str.count, body);
                    emit(op_i32_add              , body);
                    emit(op_local_set, dst_ptr_id, body);
                    continue;
                }

                // mem_offset = *_to_str(fn(), mem_offset);
                emit_node(node, ctx);

                let fn_id = node.value_type == pt_i32 ? i32_to_str_id : u32_to_str_id;

                emit(op_local_get, dst_ptr_id, body);
                emit(op_call,           fn_id, body);
                emit(op_local_set, dst_ptr_id, body);
            }

            // return string { 1024, mem_offset - 1024 };
            emit_const          (1024, body);
            emit(op_local_get, dst_ptr_id, body);
            emit_const          (1024, body);
            emit(op_i32_sub          , body);
        }

        void emit_chr(node* args, context& ctx) {
            if_ref(func, get_curr_func(ctx)); else { dbg_fail_return; }

            if_ref(value_node, args); else { dbg_fail_return; }
            let c = value_node.text[0];
            emit_const((uint)c, func.body);
        }

        void emit_minus(node* args_node_p, context& ctx) {
            if_ref(func, get_curr_func(ctx)); else { dbg_fail_return; }

            if_ref(a_node, args_node_p); else { dbg_fail_return; }
            if_ref(b_node, a_node.next); else {
                emit_const(         0, func.body);
                emit_node (    a_node, ctx      );
                emit      (op_i32_sub, func.body);
                return;
            }

            emit_node(a_node, ctx);
            emit_node(b_node, ctx);
            emit(op_i32_sub, func.body);
        }

        void emit_bop(wasm_opcode opcode, node* args_node_p, context& ctx) {
            if_ref (func, get_curr_func(ctx)); else { dbg_fail_return; }

            if_var2(a_node, b_node, deref2(args_node_p)); else { dbg_fail_return; }

            emit_node(a_node, ctx);
            emit_node(b_node, ctx);
            emit(opcode, func.body);
        }

        void emit_bop_a(wasm_opcode opcode, node* args_node_p, context& ctx) {
            if_var2(var_id_node, value_node, deref2(args_node_p)); else { dbg_fail_return; }

            if_ref(func, get_curr_func(ctx)); else { dbg_fail_return; }
            ref body = func.body;

            let var_id = var_id_node.text;
            if_ref (v, find_local(var_id, ctx)); else { error_local_not_found(var_id); return; }
            emit     (op_local_get, v.index, body);
            emit_node(value_node           , ctx );
            emit     (opcode               , body);
            emit     (op_local_set, v.index, body);
        }

        context make_context(arena& arena) {
            using namespace wasm_emit;
            return {
                .funcs           = make_arr_dyn<func> ( 256, arena),
                .func_stack      = make_arr_dyn<func*>(  16, arena),
                .scope_stack     = make_arr_dyn<scope>(  32, arena),
                . funcs_in_scope = make_arr_dyn<func*>( 128, arena),
                .locals_in_scope = make_arr_dyn<local>(  64, arena),
                .data            = make_stream        (1024, arena),
                .emitter         = make_emitter             (arena),
                .arena           =                           arena ,
            };
        }

        void emit_func_call(const string& id, context& ctx) {
            if_ref(curr_func, get_curr_func(    ctx)); else { dbg_fail_return; }
            if_ref(     func,     find_func(id, ctx)); else { dbg_fail_return; }
            emit(op_call, func.index, curr_func.body);
        }

        void push_scope(context& ctx) { push(ctx.scope_stack, {
            .prev_locals_in_scope_count = ctx.locals_in_scope.data.count,
            . prev_funcs_in_scope_count = ctx. funcs_in_scope.data.count
        });}

        void pop_scope(context& ctx) { let scope = pop(ctx.scope_stack);
            ctx.locals_in_scope.data.count = scope.prev_locals_in_scope_count;
            ctx. funcs_in_scope.data.count = scope. prev_funcs_in_scope_count;
        }

        local* find_local(string id, context& ctx) {
            for (var i = (int)ctx.locals_in_scope.data.count - 1; i >= 0; i--) {
                ref v = ctx.locals_in_scope.data[i];
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

        ret1<uint> find_func_index(string id, context& ctx) {
            if_ref(func, find_func(id, ctx)); else { dbg_fail_return {}; }
            return ret1_ok(func.index);
        }

        func& declare_import(const char* module_id, const char* id, const std::initializer_list<primitive_type>& params, const std::initializer_list<primitive_type>& results, context& ctx) {
            return declare_import(view(module_id), view(id), view(params), view(results), ctx);
        }

        func& declare_import(string module_id, string id, const arr_view<primitive_type>& params, const arr_view<primitive_type>& results, context& ctx) {
            let index = ctx.funcs.data.count;
            ref func  = push(ctx.funcs, compilation::func {
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
                .params  = make_arr_dyn<primitive_type>( 4, ctx.arena),
                .locals  = make_arr_dyn<primitive_type>(32, ctx.arena),
                .results = make_arr_dyn<primitive_type>( 2, ctx.arena),
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
            var offset = ctx.data.data.count;
            push(ctx.data, {(u8*)text.data, text.count});
            return offset;
        }

        local add_local(string id, primitive_type type, context& ctx) {
            if_ref(func, get_curr_func(ctx)); else { dbg_fail_return {}; }
            push(func.locals, type);

            var index = func.params.data.count + func.locals.data.count - 1;
            var v = local {.id = id, .index = index};
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

        uint add_param(primitive_type type, func& func) {
            assert(func.locals.data.count == 0); // add params before locals
            push(func.params, type);
            return func.params.data.count - 1;
        }

        void add_result(primitive_type type, func& func) {
            push(func.results, type);
        }

        primitive_type primitive_type_by(string name){
            if (name == view("i8" )) return primitive_type::pt_i8 ;
            if (name == view("i16")) return primitive_type::pt_i16;
            if (name == view("i32")) return primitive_type::pt_i32;
            if (name == view("u8 ")) return primitive_type::pt_u8 ;
            if (name == view("u16")) return primitive_type::pt_u16;
            if (name == view("u32")) return primitive_type::pt_u32;
            if (name == view("i64")) return primitive_type::pt_i64;
            if (name == view("u64")) return primitive_type::pt_u64;
            if (name == view("f32")) return primitive_type::pt_f32;
            if (name == view("f64")) return primitive_type::pt_f64;

            printf("Type %.*s not found.\n", (int)name.count, name.data);
            dbg_fail_return primitive_type::invalid;
        }

        wasm_value_type wasm_type_of(primitive_type type) {
            switch (type) {
                case primitive_type::pt_i8 :
                case primitive_type::pt_i16:
                case primitive_type::pt_i32:
                case primitive_type::pt_u8 :
                case primitive_type::pt_u16:
                case primitive_type::pt_u32: return wasm_value_type::vt_i32;
                case primitive_type::pt_i64:
                case primitive_type::pt_u64: return wasm_value_type::vt_i64;
                case primitive_type::pt_f32: return wasm_value_type::vt_f32;
                case primitive_type::pt_f64: return wasm_value_type::vt_f64;

                default: dbg_fail_return wasm_value_type::vt_void;
            }
        }
    }
}

#endif //FRANCA2_COMPUTE_COMPILATION_H

#pragma clang diagnostic pop