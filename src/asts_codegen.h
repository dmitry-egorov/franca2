#ifndef FRANCA2_ASTS_CODEGEN_H
#define FRANCA2_ASTS_CODEGEN_H

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/arr_dyns.h"
#include "utility/hex_ex.h"
#include "utility/wasm_emit.h"

#include "asts.h"

namespace compute_asts {
    arr_view<u8> emit_wasm(ast &);

    namespace codegen {
        using namespace wasm_emit;
        using enum prim_type;
        using enum binary_op;

        struct macro_context;

        struct binding {
            uint index_in_fn;
            node* code;
        };

        struct macro_context {
            macro* macro;
            uint block_depth;

            arr_ref<macro_context> parent_ctx;
            arr_dyn<binding> bindings;
        };

        struct context {
            ast& ast;
            fn*  curr_fn;

            arr_ref<macro_context> curr_macro_ctx;
            arr_dyn<macro_context> macro_ctx_stack;

            stream data;
        };

        void emit      (fn&  , context&);
        void emit_chain(node*, context&);
        void emit      (node&, context&);

        auto  begin_macro_ctx(macro&, context&) -> void;
        auto    end_macro_ctx(context&) -> void;
        #define tmp_macro_ctx(macro, ctx) \
            begin_macro_ctx(macro, ctx); \
            defer { end_macro_ctx(ctx); }
        #define tmp_switch_macro_ctx(ptr, ctx) \
            let stx_concat(return_ctx_, __LINE__) = ctx.curr_macro_ctx; \
            ctx.curr_macro_ctx = ptr; \
            defer { ctx.curr_macro_ctx = stx_concat(return_ctx_, __LINE__); }

        auto curr_macro_ctx(context&) -> macro_context*;

        void bind_fn_params(context&);
        void bind(uint fn_index, context&);
        void bind(arr_view<binding>, context&);

        uint add_fn_local(prim_type, context&);
        uint add_fn_local(local&, context&);

        uint add_data(string text, context&);

        auto find_binding (uint macro_index, context&) -> binding;
        auto find_local_offset(uint local_index_in_macro, context&) -> uint;
        auto find_local_index (uint local_index_in_macro, context&) -> uint;

        void emit_local_set(uint offset, prim_type, stream&);
        void emit_local_get(uint offset, prim_type, stream&);
        void emit_local_get(const local&, context&);

        void print_macro_contexts(context&);
    }

    arr_view<u8> emit_wasm(ast& ast) {
        using namespace codegen;
        using namespace wasm_emit;
        using enum fn::kind_t;

        ref temp_arena = ast.temp_arena;

        var ctx = codegen::context {
            .ast  = ast,
            .curr_macro_ctx = {(uint)-1},
            .macro_ctx_stack = make_arr_dyn<macro_context>(32, temp_arena),
            .data = make_stream(1024, temp_arena),
        };

        ref fns = ast.fns;
        for(var i = 0u; i < count_of(fns); ++i) {
            ref fn = fns[i];
            emit(fn, ctx);
            if (fn.id == view("main")) {
                ref body = fn.body_wasm;
                emit(op_i32_const, 0u, body);
                emit(op_return       , body);
            }
        }

        // emit wasm
        var emitter = make_emitter(temp_arena);

        var func_types = alloc<wasm_func_type>(temp_arena, count_of(fns));
        var imports    = make_arr_dyn<wasm_func_import>( 32, temp_arena);
        var exports    = make_arr_dyn<wasm_export     >( 32, temp_arena);
        var func_specs = make_arr_dyn<wasm_func       >(256, temp_arena);

        push(exports, wasm_export {.name = view("memory"), .kind = ek_mem, .obj_index = 0});



        for (var i = 0u; i < count_of(fns); ++i) {
            ref fn = fns[i];
            let params  = fn.params .data;

            var w_params  = make_arr_dyn<wasm_type>(4, temp_arena);
            var w_results = make_arr_dyn<wasm_type>(4, temp_arena);

            for (var pi = 0u; pi < params.count; pi += 1)
                push(w_params, wasm_types_of(params[pi]));

            push(w_results, wasm_types_of(fn.result_type));

            func_types[i] = { // TODO: distinct
                .params  = w_params .data,
                .results = w_results.data,
            };

            if (fn.kind == fk_imported) {
                push(imports, {.module = fn.import_module, .name = fn.id, .type_index = i});
                continue;
            }

            let locals = fn.local_types.data;

            var w_locals = make_arr_dyn<wasm_type>(16, temp_arena);
            for (var li = 0u; li < locals.count; li += 1)
                push(w_locals, wasm_types_of(locals[li]));

            var wasm_func = wasm_emit::wasm_func {
                .type_index = i,
                .locals     = w_locals.data,
                .body       = fn.body_wasm.data,
            };

            push(func_specs, wasm_func);

            if (fn.exported)
                push(exports, wasm_export {.name = fn.id, .kind = ek_func, .obj_index = fn.index});
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

    namespace codegen {
        using enum fn::kind_t;

        void emit(fn& fn, context& ctx) {
            if (fn.kind == fk_regular); else return;

            ctx.curr_fn = &fn;

            if_ref(macro, fn.macro); else { dbg_fail_return; }
            tmp_macro_ctx(macro, ctx);
            bind_fn_params(ctx);

            let body_ptr = macro.body_scope;
            if_ref(body , body_ptr); else { dbg_fail_return; }
            emit_chain(body.body_chain, ctx);
        }

        void emit_chain(node* chain, context& ctx) {
            for(var node_p = chain; node_p; node_p = node_p->next)
                emit(*node_p, ctx);
        }

        void emit(node& node, context& ctx) {
            using enum local::kind_t;
            using enum node::sem_kind_t;

            if_ref(fn       , ctx.curr_fn        ); else { node_error(node); return; }
            if_ref(macro_ctx, curr_macro_ctx(ctx)); else { node_error(node); return; }
            ref wasm = fn.body_wasm;

            if (node.sem_kind == sk_macro_decl) { return; }

            if (node.sem_kind == sk_block) {
                emit(op_block, wasm_types_of(node.value_type, false), wasm); defer { emit(op_end, wasm); };
                emit_chain(node.first_child, ctx);
                return;
            }

            if (node.sem_kind == sk_ret) {
                if_ref(value_node, node.first_child)
                    emit(value_node, ctx);

                emit(op_br, macro_ctx.block_depth, wasm);
                return;
            }

            if (node.sem_kind == sk_lit) {
                if (node.value_type == pt_str) {
                    let str    = node.text;
                    let offset = add_data(str, ctx);
                    emit_const_get(offset   , wasm);
                    emit_const_get(str.count, wasm);
                    return;
                }
                if (node.value_type == pt_u32) {
                    emit_const_get(node.uint_value, wasm);
                    return;
                }
                if (node.value_type == pt_i32) {
                    emit_const_get(node.int_value, wasm);
                    return;
                }
                if (node.value_type == pt_f32) {
                    emit_const_get(node.float_value, wasm);
                    return;
                }
            }

            if (node.sem_kind == sk_wasm_type) {
                emit(node.wasm_type, wasm);
                return;
            }

            if (node.sem_kind == sk_wasm_op) {
                emit(node.wasm_op, wasm);

                if (node.wasm_op == op_block || node.wasm_op == op_loop || node.wasm_op == op_if) macro_ctx.block_depth += 1;
                if (node.wasm_op == op_end) macro_ctx.block_depth -= 1;

                for (var arg_p = node.first_child; arg_p; arg_p = arg_p->next) {
                    ref arg = *arg_p;
                    assert(arg.sem_kind == sk_lit);
                    wasm_emit::emit(arg.uint_value, wasm);
                }
                return;
            }

            if (node.sem_kind == sk_local_decl) {
                if_ref (loc, node.decled_local); else { node_error(node); return; }
                let index = add_fn_local(loc, ctx);

                if_ref(init_node, node.init_node); else { node_error(node); return; }
                emit(init_node, ctx);
                emit_local_set(fn.local_offsets[index], loc.value_type, wasm);
                node.value_type = pt_void; // TODO: analyze compatible and cast
                return;
            }

            if (node.sem_kind == sk_local_get) {
                if_ref (l, node.local); else { node_error(node); return; }
                emit_local_get(l, ctx);
                return;
            }

            if (node.sem_kind == sk_local_ref) {
                if_ref (l, node.local); else { node_error(node); return; }

                //TODO: support multi-word locals
                var offset = find_local_offset(l.index_in_macro, ctx);
                wasm_emit::emit(offset, wasm);
                return;
            }

            if (node.sem_kind == sk_code_embed) {
                if_ref (l, node.local); else { node_error(node); return; }
                let binding = find_binding(l.index_in_macro, ctx);
                if_ref (code, binding.code); else { node_error(node); return; }

                let block_depth = macro_ctx.block_depth;
                tmp_switch_macro_ctx(macro_ctx.parent_ctx, ctx);
                if_ref(new_macro_ctx, curr_macro_ctx(ctx)); else { node_error(node); return; }
                new_macro_ctx.block_depth += block_depth; defer { new_macro_ctx.block_depth -= block_depth; };

                emit(code, ctx);
                return;
            }

            if (node.sem_kind == sk_chr) {
                emit_const_get(node.chr_value, wasm);
                return;
            }

            if (node.sem_kind == sk_sub_of) {
                if_ref(n, node.sub_of_node); else { node_error(node); return; }
                if (n.sem_kind == sk_local_get) {
                    if_ref(l, n.local); else { node_error(node); return; }
                    let offset = find_local_offset(l.index_in_macro, ctx);
                    emit(op_local_get, offset + node.sub_index, wasm);
                    return;
                }

                dbg_fail_return;
            }

            if (node.sem_kind == sk_macro_invoke) {
                // bind arguments and embed the function

                if_ref(refed_macro, node.refed_macro); else { node_error(node); return; }
                let params = params_of(refed_macro);
                var buffer = alloc<binding>(ctx.ast.temp_arena, params.count);
                var arg_node_p = node.first_child;
                for (var i = 0u; i < params.count; ++i, arg_node_p = arg_node_p->next) {
                    let param = params[i];
                    if_ref(arg_node, arg_node_p); else { dbg_fail_return; }

                    if (param.kind == lk_code) {
                        buffer[i] = { .index_in_fn = (uint)-1, .code = &arg_node };
                        continue;
                    }

                    if (arg_node.sem_kind == sk_local_get && param.kind != lk_code) {
                        if_ref(refed_local, arg_node.local); else { dbg_fail_return; }
                        let fn_index = find_local_index(refed_local.index_in_macro, ctx);
                        buffer[i] = {.index_in_fn = fn_index};
                        continue;
                    }

                    if (arg_node.sem_kind != sk_local_get && param.kind == lk_value) {
                        emit(arg_node, ctx);
                        let type = arg_node.value_type;
                        let fn_index = add_fn_local(type, ctx);
                        buffer[i] = {.index_in_fn = fn_index};
                        emit_local_set(fn.local_offsets[fn_index], type, wasm);
                        continue;
                    }

                    //TODO: support code ref
                    dbg_fail_return;
                }

                tmp_macro_ctx(refed_macro, ctx);
                bind(buffer, ctx);
                if (refed_macro.has_returns) emit(op_block, wasm_types_of(refed_macro.result_type, false), wasm);
                defer { if (refed_macro.has_returns) emit(op_end, wasm); };

                emit_chain(refed_macro.body_scope->body_chain, ctx);
                return;
            }

            if (node.sem_kind == sk_fn_call) {
                if_ref(refed_fn, ptr(node.refed_fn, ctx.ast.fns)); else { node_error(node); return; }
                for (var arg_p = node.first_child; arg_p; arg_p = arg_p->next) {
                    ref arg = *arg_p;
                    emit(arg, ctx);
                }

                emit(op_call, refed_fn.index, wasm);
                return;
            }

            node_error(node);
        }

        uint add_data(string text, context& ctx) {
            var offset = ctx.data.count;
            push(ctx.data, {(u8*)text.data, text.count});
            return offset;
        }

        void begin_macro_ctx(macro& macro, context& ctx) {
            push(ctx.macro_ctx_stack, {.macro = &macro, .parent_ctx = ctx.curr_macro_ctx, .bindings = make_arr_dyn<binding>(4, ctx.ast.temp_arena)});
            ctx.curr_macro_ctx = last_ref_of(ctx.macro_ctx_stack);
        }

        void end_macro_ctx(context & ctx) {
            let scope = pop(ctx.macro_ctx_stack);
            ctx.curr_macro_ctx = scope.parent_ctx;
        }

        auto curr_macro_ctx(context& ctx) -> macro_context* {
            if (ctx.curr_macro_ctx.offset != (size_t)-1); else { dbg_fail_return nullptr; }
            return ptr(ctx.curr_macro_ctx, ctx.macro_ctx_stack);
        }

        void bind_fn_params(context& ctx) {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return; }
            ref params = fn.params.data;

            for(var i = 0u; i < params.count; ++i)
                bind(i, ctx);
        }

        uint add_fn_local(prim_type type, context& ctx) {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return -1; }
            let index = (uint)fn.local_types.count;
            push(fn.local_types  , type);
            push(fn.local_offsets, fn.next_local_offset);
            fn.next_local_offset += size_32_of(type);
            return index;
        }

        uint add_fn_local(local& loc, context& ctx) {
            let index = add_fn_local(loc.value_type, ctx);
            bind(index, ctx);
            return index;
        }

        void bind(uint fn_index, context& ctx) {
            assert(   fn_index != (uint)-1);
            if_ref(macro_ctx, curr_macro_ctx(ctx)); else { dbg_fail_return; }
            push(macro_ctx.bindings, {.index_in_fn = fn_index});
        }

        void bind(arr_view<binding> bindings, context& ctx) {
            if_ref(scope, curr_macro_ctx(ctx)); else { dbg_fail_return; }
            push(scope.bindings, bindings);
        }

        auto find_local_index (uint local_index_in_macro, context& ctx) -> uint {
            return find_binding(local_index_in_macro, ctx).index_in_fn;
        }

        uint find_local_offset(uint local_index_in_macro, context& ctx) {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return -1; }
            return fn.local_offsets[find_local_index(local_index_in_macro, ctx)];
        }

        binding find_binding(uint macro_index, context& ctx) {
            if_ref(scope, curr_macro_ctx(ctx)); else { dbg_fail_return {}; }
            let bindings = scope.bindings.data;
            if (macro_index < bindings.count); else {
                printf("Failed to find binding for macro local %u\n", macro_index);
                print_macro_contexts(ctx);
                dbg_fail_return {};
            }

            return bindings[macro_index];
        }

        void emit_local_set(uint offset, prim_type type, stream& dst) {
            let size = size_32_of(type);
            for(var i = 0u; i < size; ++i)
                emit(op_local_set, offset + (size - 1 - i), dst);
        }

        void emit_local_get(uint offset, prim_type type, stream& dst) {
            let size = size_32_of(type);
            for(var i = 0u; i < size; ++i)
                emit(op_local_get, offset + i, dst);
        }

        void emit_local_get(const local& l, context& ctx) {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return; }
            emit_local_get(find_local_offset(l.index_in_macro, ctx), l.value_type, fn.body_wasm);
        }

        void print_macro_contexts(context& ctx) {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return; }

            let scopes = ctx.macro_ctx_stack.data;
            for (var i = 0u; i < scopes.count; i += 1) {
                ref scope    = scopes[i];
                if_ref(macro, scope.macro); else { dbg_fail_return; }
                let bindings = scopes[i].bindings.data;
                printf("Macro expansion scope ");
                if (i == ctx.curr_macro_ctx.offset) printf("*"); else printf(" ");

                printf("%u", i);
                if (scope.parent_ctx.offset != (size_t)-1)
                    printf("[%zu]", scope.parent_ctx.offset);
                else
                    printf("[-]");
                printf(". %.*s: [%u] ", (int)macro.id.count, macro.id.data, (uint)bindings.count);
                for (var j = 0u; j < bindings.count; j += 1) {

                    if (j < count_of(macro.locals)) {
                        let local_id = macro.locals[j].id;
                        printf("%.*s", (int)local_id.count, local_id.data);
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
    }
}

#undef tmp_macro_ctx

#pragma clang diagnostic pop

#endif //FRANCA2_ASTS_CODEGEN_H
