#ifndef FRANCA2_ASTS_CODEGEN_OLD_H
#define FRANCA2_ASTS_CODEGEN_OLD_H

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/arr_dyns.h"
#include "utility/arr_bucks.h"
#include "utility/hex_ex.h"
#include "utility/wasm_emit.h"

#include "asts.h"

namespace asts {
    arr_view<u8> emit_wasm_old(ast &);

    namespace codegen_old {
        using namespace wasm_emit;
        using enum type_t;
        using enum binary_op;

        struct param_binding {
            uint index_in_fn;
            node* code;
        };

        struct macro_expansion {
            macro* macro;
            arr_dyn<param_binding> bindings;

            uint block_depth; // dynamic for now

            macro_expansion* parent;
        };

        struct context {
            ast& ast;
            fn*  curr_fn;

            macro_expansion* curr_exp;
            arr_buck<macro_expansion> exps;

            stream data;
        };

        void emit      (fn&  , context&);
        void emit_chain(node*, context&);
        void emit      (node&, context&);

        auto  begin_expansion(macro&, context&) -> void;
        auto    end_expansion(context &) -> void;

        #define tmp_new_exp(macro, ctx) \
            begin_expansion(macro, ctx); \
            defer { end_expansion(ctx); }

        #define tmp_switch_exp_ctx(ptr, ctx) \
            let line_var(return_exp_) = ctx.curr_exp; \
            ctx.curr_exp = ptr; \
            defer { ctx.curr_exp = line_var(return_exp_); }

        void bind_fn_params(context&);
        void bind(uint index_in_fn, context&);
        void bind(arr_view<param_binding>, context&);

        uint add_fn_local(local&, context&);

        uint add_data(string text, context&);

        auto find_local_offset(uint index_in_macro, context&) -> uint;
        auto find_local_index (uint index_in_macro, context&) -> uint;
        auto find_code        (uint index_in_macro, context&) -> node*;
        auto find_binding     (uint index_in_macro, context&) -> param_binding;

        void emit_local_set(uint offset, type_t, stream&);

        void print_macro_contexts(context&);
    }

    arr_view<u8> emit_wasm_old(ast & ast) {
        using namespace codegen_old;
        using namespace wasm_emit;
        using enum fn::kind_t;

        ref temp_arena = ast.temp_arena;

        var ctx = context {
            .ast  = ast,
            .exps = make_arr_buck<macro_expansion>(1024, temp_arena),
            .data = make_stream(1024, temp_arena),
        };

        ref fns = ast.fns;

        for_arr_buck_begin(fns, fn, __fn_i) {
            emit(fn, ctx);
            if (fn.id == bi_main_id) {
                ref body = fn.body_wasm;
                emit(op_i32_const, 0u, body);
                emit(op_return       , body);
            }
        } for_arr_buck_end

        // emit wasm
        var emitter = make_emitter(temp_arena);

        var func_types = alloc<wasm_func_type>(temp_arena, count_of(fns));
        var imports    = make_arr_dyn<wasm_func_import>( 32, temp_arena);
        var exports    = make_arr_dyn<wasm_export     >( 32, temp_arena);
        var func_specs = make_arr_dyn<wasm_func       >(256, temp_arena);

        push(exports, wasm_export {.name = view("memory"), .kind = ek_mem, .obj_index = 0});

        for_arr_buck_begin(fns, fn, fn_i) {
            var w_params  = make_arr_dyn<wasm_type>(4, temp_arena);
            var w_results = make_arr_dyn<wasm_type>(4, temp_arena);

            let params = fn.param_types.data;
            for_arr2(pi, params)
                push(w_params, wasm_types_of(params[pi]));

            push(w_results, wasm_types_of(fn.result_type));

            func_types[fn_i] = { // TODO: distinct
                .params  = w_params .data,
                .results = w_results.data,
            };

            if (fn.kind == fk_imported) {
                push(imports, wasm_func_import{.module = text_of(fn.import_module, ast), .name = text_of(fn.id, ast), .type_index = fn_i});
                continue;
            }

            let locals = fn.local_types.data;

            var w_locals = make_arr_dyn<wasm_type>(16, temp_arena);
            for_arr2(li, locals)
                push(w_locals, wasm_types_of(locals[li]));

            var wasm_func = wasm_emit::wasm_func {
                .type_index = fn_i,
                .locals     = w_locals.data,
                .body       = fn.body_wasm.data,
            };

            push(func_specs, wasm_func);

            if (fn.exported)
                push(exports, wasm_export {.name = text_of(fn.id, ast), .kind = ek_func, .obj_index = fn.index});
        } for_arr_buck_end

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

    namespace codegen_old {
        using enum fn::kind_t;

        void emit(fn& fn, context& ctx) {
            if (fn.kind == fk_regular); else return;

            ctx.curr_fn = &fn;

            //if_ref(exp  , fn .expansion); else { dbg_fail_return; }
            //if_ref(macro, exp.macro    ); else { dbg_fail_return; }
            if_ref(macro, fn.macro); else { dbg_fail_return; }
            tmp_new_exp(macro, ctx);
            bind_fn_params(ctx);

            let body_ptr = macro.body_scope;
            if_ref(body , body_ptr); else { dbg_fail_return; }
            emit_chain(body.chain, ctx);
        }

        void emit_chain(node* chain, context& ctx) {
            for_chain(chain)
                emit(*it, ctx);
        }

        void emit(node& node, context& ctx) {
            using enum local::kind_t;
            using enum node::sem_kind_t;

            if_ref(fn , ctx.curr_fn ); else { node_error(node); return; }
            if_ref(exp, ctx.curr_exp); else { node_error(node); return; }
            ref wasm = fn.body_wasm;

            if (node.sem_kind == sk_macro_decl) { return; }

            if (node.sem_kind == sk_block) {
                emit(op_block, wasm_types_of(node.value_type, false), wasm); defer { emit(op_end, wasm); };
                emit_chain(node.child_chain, ctx);
                return;
            }

            if (node.sem_kind == sk_ret) {
                if_ref(value_node, node.child_chain)
                    emit(value_node, ctx);

                emit(op_br, exp.block_depth, wasm);
                return;
            }

            if (node.sem_kind == sk_lit) {
                if (node.value_type == t_str) {
                    let str    = node.text;
                    let offset = add_data(str, ctx);
                    emit_const_get(offset   , wasm);
                    emit_const_get(str.count, wasm);
                    return;
                }
                if (node.value_type == t_u32) {
                    emit_const_get(node.uint_value, wasm);
                    return;
                }
                if (node.value_type == t_i32) {
                    emit_const_get(node.int_value, wasm);
                    return;
                }
                if (node.value_type == t_f32) {
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

                if (node.wasm_op == op_block || node.wasm_op == op_loop || node.wasm_op == op_if) exp.block_depth += 1;
                if (node.wasm_op == op_end) exp.block_depth -= 1;

                for_chain(node.child_chain) {
                    ref arg = *it;
                    assert(arg.sem_kind == sk_lit);
                    wasm_emit::emit(arg.uint_value, wasm);
                }
                return;
            }

            if (node.sem_kind == sk_local_decl) {
                if_ref (loc, node.decled_local); else { node_error(node); return; }
                let offset = add_fn_local(loc, ctx);

                if_ref(init_node, node.init_node); else { node_error(node); return; }
                emit(init_node, ctx);
                emit_local_set(offset, loc.value_type, wasm);
                return;
            }

            if (node.sem_kind == sk_local_get) {
                if_ref (l, node.refed_local); else { node_error(node); return; }
                var offset = find_local_offset(l.index_in_macro, ctx);
                let size = size_32_of(l.value_type);
                for(var i = 0u; i < size; ++i)
                    emit(op_local_get, offset + i, fn.body_wasm);

                return;
            }

            if (node.sem_kind == sk_local_ref) {
                if_ref (l, node.refed_local); else { node_error(node); return; }

                //TODO: support multi locals
                var offset = find_local_offset(l.index_in_macro, ctx);
                wasm_emit::emit(offset, wasm);
                return;
            }

            if (node.sem_kind == sk_code_embed) {
                if_ref (l, node.refed_local); else { node_error(node); return; }
                if_ref (code, find_code(l.index_in_macro, ctx)); else { node_error(node); return; }

                let block_depth = exp.block_depth;
                tmp_switch_exp_ctx(exp.parent, ctx);
                if_ref(new_exp, ctx.curr_exp); else { node_error(node); return; }
                new_exp.block_depth += block_depth; defer { new_exp.block_depth -= block_depth; };

                emit(code, ctx);
                return;
            }

            if (node.sem_kind == sk_chr) {
                emit_const_get(node.chr_value, wasm);
                return;
            }

            if (node.sem_kind == sk_sub) {
                if_ref(n, node.sub_node); else { node_error(node); return; }
                if_ref(l, n.refed_local); else { node_error(node); return; }
                let offset = find_local_offset(l.index_in_macro, ctx);
                emit(op_local_get, offset + node.sub_index, wasm);
                return;

                dbg_fail_return;
            }

            if (node.sem_kind == sk_each) {
                if_ref (l, node.each_list_local); else { node_error(node); return; }
                if_ref (code, find_code(l.index_in_macro, ctx)); else { node_error(node); return; }
                //if (code.sem_kind == sk_block); else { printf("Only blocks can be used as arguments to 'each'\n"); node_error(node); return; }

                for_chain(code.child_chain) {
                    //emit(*it, ctx);
                    //TODO: bind 'it', embed body
                    //TODO: need to type-check here...
                }

                return;
            }

            if (node.sem_kind == sk_macro_expand) {
                // bind arguments and embed the function
                if_ref(refed_macro, node.refed_macro); else { node_error(node); return; }
                let params = params_of(refed_macro);
                var buffer = alloc<param_binding>(ctx.ast.temp_arena, params.count);
                var arg_node_p = node.child_chain;
                for_arr(params) {
                    defer { arg_node_p = arg_node_p->next; };

                    let param = params[i];
                    if_ref(arg_node, arg_node_p); else { dbg_fail_return; }

                    if (param.kind == lk_code) {
                        buffer[i] = { .index_in_fn = (uint)-1, .code = &arg_node };
                        continue;
                    }

                    if (arg_node.sem_kind == sk_local_get && param.kind != lk_code) {
                        if_ref(refed_local, arg_node.refed_local); else { dbg_fail_return; }
                        let fn_index = find_local_index(refed_local.index_in_macro, ctx);
                        buffer[i] = {.index_in_fn = fn_index};
                        continue;
                    }

                    if (arg_node.sem_kind != sk_local_get && param.kind == lk_value) {
                        emit(arg_node, ctx);
                        let type = arg_node.value_type;
                        let index_in_fn = add_fn_local(type, fn);
                        buffer[i] = {.index_in_fn = index_in_fn};
                        emit_local_set(fn.local_offsets[index_in_fn], type, wasm);
                        continue;
                    }

                    dbg_fail_return;
                }

                tmp_new_exp(refed_macro, ctx);
                bind(buffer, ctx);
                if (refed_macro.has_returns) emit(op_block, wasm_types_of(refed_macro.result_type, false), wasm);
                defer { if (refed_macro.has_returns) emit(op_end, wasm); };

                if_ref(body_scope, refed_macro.body_scope); else { node_error(node); return; }
                emit_chain(body_scope.chain, ctx);
                return;
            }

            if (node.sem_kind == sk_fn_call) {
                if_ref(refed_fn, node.refed_fn); else { node_error(node); return; }
                emit_chain(node.child_chain, ctx);
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

        void begin_expansion(macro & macro, context& ctx) {
            ctx.curr_exp = &push({
                .macro = &macro,
                .bindings = make_arr_dyn<param_binding>(4, ctx.ast.temp_arena),
                .parent = ctx.curr_exp,
            }, ctx.exps);
        }

        void end_expansion(context & ctx) {
            if_ref(curr_ctx, ctx.curr_exp) ; else { dbg_fail_return; }
            ctx.curr_exp = curr_ctx.parent;
        }

        void bind_fn_params(context& ctx) {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return; }
            ref params = fn.param_types.data;

            for(var i = 0u; i < params.count; ++i)
                bind(i, ctx);
        }

        auto add_fn_local(local& loc, context& ctx) -> uint /* offset */ {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return -1; }
            let index = add_fn_local(loc.value_type, fn);
            bind(index, ctx);
            return fn.local_offsets[index];
        }

        void bind(uint index_in_fn, context& ctx) {
            assert(index_in_fn != (uint)-1);
            if_ref(exp, ctx.curr_exp); else { dbg_fail_return; }
            push(exp.bindings, {.index_in_fn = index_in_fn});
        }

        void bind(arr_view<param_binding> bindings, context& ctx) {
            if_ref(exp, ctx.curr_exp); else { dbg_fail_return; }
            push(exp.bindings, bindings);
        }

        auto find_local_index(uint index_in_macro, context& ctx) -> uint {
            return find_binding(index_in_macro, ctx).index_in_fn;
        }

        auto find_local_offset(uint index_in_macro, context& ctx) -> uint {
            if_ref(fn, ctx.curr_fn); else { dbg_fail_return -1; }
            return fn.local_offsets[find_local_index(index_in_macro, ctx)];
        }

        auto find_code(uint index_in_macro, context& ctx) -> node* {
            return find_binding(index_in_macro, ctx).code;
        }

        param_binding find_binding(uint index_in_macro, context& ctx) {
            if_ref(exp, ctx.curr_exp); else { dbg_fail_return {}; }
            let bindings = exp.bindings.data;
            if (index_in_macro < bindings.count); else {
                printf("Failed to find binding for macro local %u\n", index_in_macro);
                print_macro_contexts(ctx);
                dbg_fail_return {};
            }

            return bindings[index_in_macro];
        }

        void emit_local_set(uint offset, type_t type, stream& dst) {
            let size = size_32_of(type);
            for_rng(0u, size)
                emit(op_local_set, offset + (size - 1 - i), dst);
        }

        void print_macro_contexts(context& ctx) {
            for_arr_buck_begin(ctx.exps, exp, exp_i) {
                if_ref(macro, exp.macro); else { dbg_fail_return; }
                let bindings = exp.bindings.data;
                printf("Macro expansion ");
                if (&exp == ctx.curr_exp) printf("*"); else printf(" ");

                printf("%zu", exp_i);
                let macro_name = text_of(macro.id, ctx.ast);
                printf(". %.*s: [%u] ", (int) macro_name.count, macro_name.data, (uint)bindings.count);
                for_arr2(j, bindings) {
                    if (j < count_of(macro.locals)) {
                        let local_name = text_of(macro.locals[j].id, ctx.ast);

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
            } for_arr_buck_end
        }
    }
}

#undef tmp_new_exp
#undef tmp_switch_exp_ctx

#pragma clang diagnostic pop

#endif //FRANCA2_ASTS_CODEGEN_OLD_H
