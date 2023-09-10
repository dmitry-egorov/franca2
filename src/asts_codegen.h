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

        struct bind {
            uint local_index;
            uint bound_index;
        };

        struct bind_scope {
            size_t bind_count;
        };

        struct context {
            ast& ast;
            fn* curr_fn;

            arr_dyn<bind_scope> bind_scope_stack;
            arr_dyn<bind      > bind_stack;

            stream data;
        };

        void emit      (fn&  , context& ctx);
        void emit_chain(node*, context& ctx);
        void emit      (node&, context& ctx);

        void begin_fn_bind_scope(fn&, context& ctx);
        void begin_bind_scope   (context&);
        void   end_bind_scope   (context&);
        void  add_bind (uint local_index, uint bound_index, context&);
        auto curr_binds(context&) -> arr_view<bind>;

        uint add_fn_local(vari& loc, context& ctx);

        auto add_data(string text, context&) -> uint;

        auto find_bound_index(uint local_index, context&) -> uint;
        auto find_bound_index(uint local_index, arr_view<bind> binds) -> uint;

        void print_bindings(context&);
    }

    arr_view<u8> emit_wasm(ast& ast) {
        using namespace codegen;
        using namespace wasm_emit;
        using enum fn::kind_t;

        ref temp_arena = ast.temp_arena;

        var ctx = codegen::context {
            .ast  = ast,
            .bind_scope_stack = make_arr_dyn<bind_scope>(32, temp_arena),
            .bind_stack       = make_arr_dyn<bind      >(256, temp_arena),
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
                .params  = alloc_g<wasm_value_type>(temp_arena, params .count),
                .results = alloc_g<wasm_value_type>(temp_arena, results.count),
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
                .locals     = alloc_g<wasm_value_type>(temp_arena, locals.count),
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
            begin_fn_bind_scope(fn, ctx);

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
            using enum vari::kind_t;
            using enum node::sem_kind_t;

            printf("BAM!!! Node: %.*s\n", (int)node.text.count, node.text.data);

            if_ref(scope, node.parent_scope); else { node_error(node); return; }
            if_ref(fn   , ctx .curr_fn     ); else { node_error(node); return; }
            ref body  = fn.body_wasm;

            if (node.sem_kind == sk_macro_decl) { return; }

            if (node.sem_kind == sk_str_lit) {
                let str    = node.text;
                let offset = add_data(str, ctx);
                emit_const(offset   , body);
                emit_const(str.count, body);
                return;
            }

            if (node.sem_kind == sk_num_lit) {
                if (node.value_type == pt_u32) {
                    emit_const(node.uint_value, body);
                    return;
                }
                if (node.value_type == pt_i32) {
                    emit_const(node.int_value, body);
                    return;
                }
                if (node.value_type == pt_f32) {
                    emit_const(node.float_value, body);
                    return;
                }
            }

            if (node.sem_kind == sk_wasm_op) {
                emit(node.wasm_op, body);
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

            if (node.sem_kind == sk_var) {
                if_ref (v, node.refed_var); else { node_error(node); return; }

                var bound_index = find_bound_index(v.local_index, ctx);
                emit(op_local_get, bound_index, body);
                return;
            }

            if (node.sem_kind == sk_var_ref) {
                if_ref (v, node.refed_var); else { node_error(node); return; }

                var bound_index = find_bound_index(v.local_index, ctx);
                wasm_emit::emit(bound_index, body);
                return;
            }

            if (node.sem_kind == sk_macro_invoke) {
                // TODO: the stack can reallocate, but we don't free the old memory, so this works. Think of a better approach.
                let binds = curr_binds(ctx);
                begin_bind_scope(ctx); defer { end_bind_scope(ctx); };

                // bind arguments and embed the function
                if_ref(refed_macro, node.refed_macro); else { node_error(node); return; }
                let params     = params_of(refed_macro);
                var arg_node_p = node.first_child;
                for (var i = 0u; i < params.count; ++i, arg_node_p = arg_node_p->next) {
                    let param = params[i];
                    if_ref(arg_node, arg_node_p); else { dbg_fail_return; }

                    if (param.kind == vk_ref) {
                        if (arg_node.sem_kind == sk_var); else { dbg_fail_return; }
                        if_ref(refed_local, arg_node.refed_var); else { dbg_fail_return; }

                        let bound_index = find_bound_index(refed_local.local_index, binds);
                        add_bind(i, bound_index, ctx);

                        continue;
                    }

                    //TODO: support param by value and code
                    dbg_fail_return;
                }

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

        void begin_fn_bind_scope(fn & fn, context & ctx) {
            ref params = fn.params.data;

            begin_bind_scope(ctx);
            for(var i = 0u; i < params.count; ++i)
                add_bind(i, i, ctx);
        }

        void begin_bind_scope(context & ctx) {
            push(ctx.bind_scope_stack, {(size_t)0});
        }

        void end_bind_scope(context & ctx) {
            let scope = pop(ctx.bind_scope_stack);
            pop(ctx.bind_stack, scope.bind_count);
        }


        uint add_fn_local(vari& loc, context& ctx) {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return -1; }
            let index = (uint)fn.locals.data.count;
            push(fn.locals, loc.value_type);
            add_bind(loc.local_index, index, ctx);
            return index;
        }

        void add_bind(uint local_index, uint bound_index, context& ctx) {
            push(ctx.bind_stack, {.local_index = local_index, .bound_index = bound_index});
            if_ref(scope, last_of(ctx.bind_scope_stack)); else { dbg_fail_return; }
            scope.bind_count += 1;
        }

        auto curr_binds(context& ctx) -> arr_view<bind> {
            if_ref(scope, last_of(ctx.bind_scope_stack)); else { dbg_fail_return {}; }
            return last_of(ctx.bind_stack, scope.bind_count);
        }

        uint find_bound_index(uint local_index, arr_view<bind> binds) {
            for (var i = 0u; i < binds.count; i += 1) {
                let binding = binds[i];
                if (binding.local_index == local_index)
                    return binding.bound_index;
            }
            return -1;
        }

        uint find_bound_index(uint local_index, context& ctx) {
            let bindings = curr_binds(ctx);
            let index = find_bound_index(local_index, bindings);
            if (index != (uint)-1); else {
                printf("Failed to find bound index for local %u\n", local_index);
                print_bindings(ctx);
                dbg_fail_return -1;
            }
            return index;
        }

        void print_bindings(context& ctx) {
            let scopes = ctx.bind_scope_stack.data;
            for (var i = 0u; i < scopes.count; i += 1) {
                let bindings = last_of(ctx.bind_stack, scopes[i].bind_count);
                printf("Binding context %u: %u. ", i, (uint)bindings.count);
                for (var j = 0u; j < bindings.count; j += 1)
                    printf("%u -> %u, ", bindings[j].local_index, bindings[j].bound_index);
                printf("\n");
            }
        }
    }
}
#undef node_failed_return

#pragma clang diagnostic pop

#endif //FRANCA2_ASTS_CODEGEN_H
