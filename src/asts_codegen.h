#ifndef FRANCA2_ASTS_CODEGEN_H
#define FRANCA2_ASTS_CODEGEN_H

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
    arr_view<u8> emit_wasm(ast &);

    namespace codegen {
        using namespace wasm_emit;
        using enum type_t;
        using enum binary_op;

        void emit(fn&  , ast&);
        void emit(node&, ast&);

        void emit_local_set(uint offset, type_t, stream&);
    }

    inline arr_view<u8> emit_wasm(ast& ast) {
        using namespace codegen;
        using namespace wasm_emit;
        using enum fn::kind_t;

        ref temp_arena = ast.temp_arena;

        ref fns = ast.fns;

        for_arr_buck_begin(fns, fn, __fn_i) {
            emit(fn, ast);
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
                push(imports, wasm_func_import{.module = text_of(fn.import_module, ast), .name = text_of(fn.id, ast), .type_index = (uint)fn_i});
                continue;
            }

            let locals = fn.local_types.data;

            var w_locals = make_arr_dyn<wasm_type>(16, temp_arena);
            for_arr2(li, locals)
                push(w_locals, wasm_types_of(locals[li]));

            var wasm_func = wasm_emit::wasm_func {
                .type_index = (uint)fn_i,
                .locals     = w_locals.data,
                .body       = fn.body_wasm.data,
            };

            push(func_specs, wasm_func);

            if (fn.exported)
                push(exports, wasm_export {.name = text_of(fn.id, ast), .kind = ek_func, .obj_index = fn.index});
        } for_arr_buck_end

        var mem = wasm_memory {1,1};

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
        emit_data_section  (ast.data       , emitter);

        let wasm = emitter.wasm.data;
        print_hex(wasm);
        return wasm;
    }

    namespace codegen {
        using enum fn::kind_t;

        inline void emit(fn& fn, ast& ast) {
            if (fn.kind == fk_regular); else return;

            if_ref(exp, fn.expansion); else { dbg_fail_return; }
            for_chain(exp.generated_chain) emit(*it, ast);
        }

        inline void emit(node& node, ast& ast) {
            using enum local::kind_t;
            using enum node::sem_kind_t;

            if_ref(exp, node.exp); else { node_error(node); return; }
            if_ref(fn , exp .fn ); else { node_error(node); return; }

            ref wasm = fn.body_wasm;

            if (node.sem_kind == sk_macro_decl) { return; }

            if (node.sem_kind == sk_list) {
                for_chain(node.child_chain) emit(*it, ast);
                return;
            }

            if (node.sem_kind == sk_scope) {
                exp.wasm_block_depth += 1; defer { exp.wasm_block_depth -= 1; };
                for_chain(node.child_chain) emit(*it, ast);
                return;
            }

            if (node.sem_kind == sk_ret) {
                if_ref(value_node, node.child_chain)
                    emit(value_node, ast);

                emit(op_br, exp.wasm_block_depth, wasm);
                return;
            }

            if (node.sem_kind == sk_lit) {
                if (node.value_type == t_str) {
                    emit_const_get(node.str_data_offset, wasm);
                    emit_const_get(node.text.count     , wasm);
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

                for_chain(node.child_chain) {
                    ref arg = *it;
                    assert(arg.sem_kind == sk_lit);
                    wasm_emit::emit(arg.uint_value, wasm);
                }
                return;
            }

            if (node.sem_kind == sk_local_decl) {
                if_ref(init_node, node.init_node); else { node_error(node); return; }
                emit(init_node, ast);
                emit_local_set(fn.local_offsets[node.decl_local_index_in_fn], init_node.value_type, wasm);
                return;
            }

            if (node.sem_kind == sk_local_get) {
                var offset = fn.local_offsets[node.local_index_in_fn];
                let size = size_32_of(node.value_type);
                for(var i = 0u; i < size; ++i)
                    emit(op_local_get, offset + i, fn.body_wasm);

                return;
            }

            if (node.sem_kind == sk_local_ref) {
                if_ref (l, node.refed_local); else { node_error(node); return; }

                //TODO: support multi locals
                var offset = fn.local_offsets[node.local_index_in_fn];
                wasm_emit::emit(offset, wasm);
                return;
            }

            if (node.sem_kind == sk_code_embed) {
                if_ref(code, node.child_chain); else { node_error(node); return; }
                if_ref(code_exp, code.exp); else { node_error(node); return; }
                code_exp.wasm_block_depth += exp.wasm_block_depth; defer { code_exp.wasm_block_depth -= exp.wasm_block_depth; };
                emit(code, ast);
                return;
            }

            if (node.sem_kind == sk_chr) {
                emit_const_get(node.chr_value, wasm);
                return;
            }

            if (node.sem_kind == sk_sub) {
                emit(op_local_get, node.sub_fn_offset, wasm);
                return;
            }

            if (node.sem_kind == sk_each) {
                /*if_ref (l, node.each_list_local); else { node_error(node); return; }
                if_ref (code, find_code(l.index_in_macro, ast)); else { node_error(node); return; }
                //if (code.sem_kind == sk_block); else { printf("Only blocks can be used as arguments to 'each'\n"); node_error(node); return; }

                for_chain(code.child_chain) {
                    //emit(*it, ast);
                    //TODO: bind 'it', embed body
                    //TODO: need to type-check here...
                }
                */

                return;
            }

            if (node.sem_kind == sk_macro_expand) {
                if_ref(macro_exp, node.macro_expansion); else { dbg_fail_return; }
                if_ref(macro    , macro_exp.macro); else { dbg_fail_return; }

                for_rng(0u, macro.params_count) {
                    ref local = macro.locals[i];
                    if (local.kind == lk_value); else continue;
                    ref b = macro_exp.bindings[i];
                    if_ref(init_node, b.init); else continue; //NOTE: we're using the passed local directly
                    emit(init_node, ast);
                    emit_local_set(fn.local_offsets[b.index_in_fn], init_node.value_type, wasm);
                }

                        if (macro.has_returns) { emit(op_block, wasm_types_of(macro.result_type, false), wasm); }
                defer { if (macro.has_returns) { emit(op_end, wasm); }};

                for_chain(macro_exp.generated_chain) emit(*it, ast);
                return;
            }

            if (node.sem_kind == sk_fn_call) {
                if_cref(refed_fn, node.refed_fn); else { node_error(node); return; }
                for_chain(node.child_chain) emit(*it, ast);
                emit(op_call, refed_fn.index, wasm);
                return;
            }

            node_error(node);
        }

        inline void emit_local_set(uint offset, type_t type, stream& dst) {
            let size = size_32_of(type);
            for_rng(0u, size)
                emit(op_local_set, offset + (size - 1 - i), dst);
        }
    }
}

#undef tmp_new_exp
#undef tmp_switch_exp_ctx

#pragma clang diagnostic pop

#endif //FRANCA2_ASTS_CODEGEN_H
