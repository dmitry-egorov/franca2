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

        struct macro_scope;

        struct binding {
            uint fn_index;
            node* code;
            macro_scope* scope;
        };

        struct macro_scope {
            arr_dyn<binding> bindings;

            //TODO: previous scope?
        };

        struct context {
            ast& ast;
            fn*  curr_fn;

            arr_dyn<macro_scope> macro_scope_stack;

            stream data;
        };

        void emit      (fn&  , context&);
        void emit_chain(node*, context&);
        void emit      (node&, context&);

        auto begin_macro_scope(context &) -> macro_scope&;
        void   end_macro_scope(context &);

        auto curr_macro_scope(context &) -> macro_scope*;
        void bind_fn_params(context&);
        void bind(uint fn_index, context&);
        void bind(arr_view<binding>, context&);

        uint add_fn_local  (local&, context&);
        uint add_temp_local(prim_type, context&);

        uint add_data(string text, context&);

        auto find_binding (uint macro_index, context&) -> binding;
        auto find_fn_index(uint macro_index, context&) -> uint;

        void print_bindings(context&);
    }

    arr_view<u8> emit_wasm(ast& ast) {
        using namespace codegen;
        using namespace wasm_emit;
        using enum fn::kind_t;

        ref temp_arena = ast.temp_arena;

        var ctx = codegen::context {
            .ast  = ast,
            .macro_scope_stack = make_arr_dyn<macro_scope>(32, temp_arena),
            .data = make_stream(1024, temp_arena),
        };

        ref fns = ast.fns.data;
        for(var i = 0u; i < fns.count; ++i) {
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

        var func_types = alloc<wasm_func_type>(temp_arena, fns.count);
        var imports    = make_arr_dyn<wasm_func_import>( 32, temp_arena);
        var exports    = make_arr_dyn<wasm_export     >( 32, temp_arena);
        var func_specs = make_arr_dyn<wasm_func       >(256, temp_arena);

        push(exports, wasm_export {.name = view("memory"), .kind = ek_mem, .obj_index = 0});

        for (var i = 0u; i < fns.count; ++i) {
            ref fn = fns[i];
            let params  = fn.params .data;
            let results = fn.results.data;

            func_types[i] = { // TODO: distinct
                .params  = alloc_g<wasm_type>(temp_arena, params .count),
                .results = alloc_g<wasm_type>(temp_arena, results.count),
            };

            for (var pi = 0u; pi < params.count; pi += 1)
                func_types[i].params[pi] = wasm_type_of(params[pi]);

            for (var ri = 0u; ri < results.count; ri += 1)
                func_types[i].results[ri] = wasm_type_of(results[ri]);

            if (fn.kind == fk_imported) {
                push(imports, {.module = fn.import_module, .name = fn.id, .type_index = i});
                continue;
            }

            let locals = fn.locals.data;
            var wasm_func = wasm_emit::wasm_func {
                .type_index = i,
                .locals     = alloc_g<wasm_type>(temp_arena, locals.count),
                .body       = fn.body_wasm.data,
            };

            for (var li = 0u; li < locals.count; li += 1)
                wasm_func.locals[li] = wasm_type_of(locals[li]);

            push(func_specs, wasm_func);

            if (fn.exported)
                push(exports, wasm_export {.name = fn.id, .kind = ek_func, .obj_index = fn.index});
        }

        var mem = wasm_memory {1,1};

        var data  = wasm_data{ctx.data.data};
        var datas = view(data);

        printf("func types: %zu\n", func_types.count);
        printf("imports   : %zu\n", imports   .data.count);
        printf("exports   : %zu\n", exports   .data.count);
        printf("funcs     : %zu\n", func_specs.data.count);
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
            begin_macro_scope(ctx); defer { end_macro_scope(ctx); };
            bind_fn_params(ctx);

            if_ref(macro, fn.macro); else { dbg_fail_return; }
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

            if_ref(scope, node.parent_scope); else { node_error(node); return; }
            if_ref(fn   , ctx .curr_fn     ); else { node_error(node); return; }
            ref body  = fn.body_wasm;

            if (node.sem_kind == sk_macro_decl) { return; }

            if (node.sem_kind == sk_block) {
                emit_chain(node.first_child, ctx);
                return;
            }

            if (node.sem_kind == sk_str_lit) {
                let str    = node.text;
                let offset = add_data(str, ctx);
                emit_const_get(offset   , body);
                emit_const_get(str.count, body);
                return;
            }

            if (node.sem_kind == sk_num_lit) {
                if (node.value_type == pt_u32) {
                    emit_const_get(node.uint_value, body);
                    return;
                }
                if (node.value_type == pt_i32) {
                    emit_const_get(node.int_value, body);
                    return;
                }
                if (node.value_type == pt_f32) {
                    emit_const_get(node.float_value, body);
                    return;
                }
            }

            if (node.sem_kind == sk_wasm_type) {
                emit(node.wasm_type, body);
                return;
            }
            if (node.sem_kind == sk_wasm_op) {
                emit(node.wasm_op, body);
                for (var arg_p = node.first_child; arg_p; arg_p = arg_p->next) {
                    ref arg = *arg_p;
                    assert(arg.sem_kind == sk_num_lit);
                    wasm_emit::emit(arg.uint_value, body);
                }
                return;
            }

            if (node.sem_kind == sk_local_decl) {
                if_ref (loc, node.decled_local); else { node_error(node); return; }
                let index = add_fn_local(loc, ctx);

                if_ref(init_node, node.init_node); else { node_error(node); return; }
                emit(init_node, ctx);
                emit(op_local_set, index, body);
                node.value_type = pt_void; // TODO: analyze compatible and cast
                return;
            }

            if (node.sem_kind == sk_local_get) {
                if_ref (l, node.local); else { node_error(node); return; }

                var bound_index = find_fn_index(l.macro_index, ctx);
                emit(op_local_get, bound_index, body);
                return;
            }

            if (node.sem_kind == sk_local_ref) {
                if_ref (l, node.local); else { node_error(node); return; }

                var bound_index = find_fn_index(l.macro_index, ctx);
                wasm_emit::emit(bound_index, body);
                return;
            }

            if (node.sem_kind == sk_code_embed) {
                if_ref (l, node.local); else { node_error(node); return; }
                let binding = find_binding(l.macro_index, ctx);
                if_ref (code, binding.code); else { node_error(node); return; }
                begin_macro_scope(ctx); defer { end_macro_scope(ctx); };
                bind(binding.scope->bindings.data, ctx);
                emit(code, ctx);
                return;
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
                        buffer[i] = { .fn_index = (uint)-1, .code = &arg_node, .scope = curr_macro_scope(ctx),  };
                        continue;
                    }

                    if (arg_node.sem_kind == sk_local_get) {
                        if_ref(refed_local, arg_node.local); else { dbg_fail_return; }
                        let fn_index = find_fn_index(refed_local.macro_index, ctx);
                        buffer[i] = {.fn_index = fn_index};
                        continue;
                    }

                    if (arg_node.sem_kind != sk_local_get && param.kind == lk_value) {
                        emit(arg_node, ctx);
                        let fn_index = add_temp_local(arg_node.value_type, ctx);
                        buffer[i] = {.fn_index = fn_index};
                        emit(op_local_set, fn_index, body);
                        continue;
                    }

                    //TODO: support code ref
                    dbg_fail_return;
                }

                begin_macro_scope(ctx); defer { end_macro_scope(ctx); };
                bind(buffer, ctx);
                emit_chain(refed_macro.body_scope->body_chain, ctx);

                return;
            }

            node_error(node);
        }

        uint add_data(string text, context& ctx) {
            var offset = ctx.data.data.count;
            push(ctx.data, {(u8*)text.data, text.count});
            return offset;
        }

        macro_scope& begin_macro_scope(context & ctx) {
            return push(ctx.macro_scope_stack, {.bindings = make_arr_dyn<binding>(4, ctx.ast.temp_arena)});
        }

        void end_macro_scope(context & ctx) {
            pop(ctx.macro_scope_stack);
        }

        void bind_fn_params(context& ctx) {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return; }
            ref params = fn.params.data;

            for(var i = 0u; i < params.count; ++i)
                bind(i, ctx);
        }

        uint add_fn_local(local& loc, context& ctx) {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return -1; }
            let index = (uint)fn.locals.data.count;
            push(fn.locals, loc.value_type);
            bind(index, ctx);
            return index;
        }

        uint add_temp_local(prim_type type, context& ctx) {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return -1; }
            let index = (uint)fn.locals.data.count;
            push(fn.locals, type);
            bind(index, ctx);
            return index;
        }

        void bind(uint fn_index, context& ctx) {
            assert(   fn_index != (uint)-1);
            if_ref(scope, curr_macro_scope(ctx)); else { dbg_fail_return; }
            push(scope.bindings, {.fn_index = fn_index});
        }

        void bind(arr_view<binding> bindings, context& ctx) {
            if_ref(scope, curr_macro_scope(ctx)); else { dbg_fail_return; }
            push(scope.bindings, bindings);
        }

        auto curr_macro_scope(context& ctx) -> macro_scope* {
            return last_of(ctx.macro_scope_stack);
        }

        uint find_fn_index(uint macro_index, context& ctx) {
            return find_binding(macro_index, ctx).fn_index;
        }

        binding find_binding(uint macro_index, context& ctx) {
            if_ref(scope, curr_macro_scope(ctx)); else { dbg_fail_return {}; }
            let bindings = scope.bindings.data;
            if (macro_index < bindings.count); else {
                printf("Failed to find binding for macro local %u\n", macro_index);
                print_bindings(ctx);
                dbg_fail_return {};
            }

            return bindings[macro_index];
        }

        void print_bindings(context& ctx) {
            let scopes = ctx.macro_scope_stack.data;
            for (var i = 0u; i < scopes.count; i += 1) {
                let bindings = scopes[i].bindings.data;
                printf("Binding context %u: %u. ", i, (uint)bindings.count);
                for (var j = 0u; j < bindings.count; j += 1)
                    printf("%u -> %u, ", j, bindings[j].fn_index);
                printf("\n");
            }
        }
    }
}

#pragma clang diagnostic pop

#endif //FRANCA2_ASTS_CODEGEN_H
