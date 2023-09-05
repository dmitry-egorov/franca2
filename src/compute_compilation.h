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
    arr_view<u8> compile(const ast&, arena& = gta);

    namespace compilation {
        using namespace wasm_emit;
        using enum primitive_type;
        using enum binary_op;

        struct scope {
            arr_dyn<variable> locals;
            arr_dyn<func> funcs ;
            scope* parent;
        };

        struct scope_old {
            uint prev_locals_in_scope_count;
            uint prev_funcs_in_scope_count;
        };

        struct context {
            size_t non_inline_func_count;
            arr_dyn<func >   funcs;
            arr_dyn<func*>   func_stack;
            arr_dyn<scope_old> scope_stack;
            arr_dyn<func*>  funcs_in_scope;
            arr_dyn<variable> locals_in_scope;

            stream data;

            wasm_memory  mem;
            string exported_memory_id;

            wasm_emitter emitter;
            arena& arena;
        };

        static wasm_opcode bop_to_wasm_op[(u32)pt_enum_size * (u32)bop_enum_size];

        context make_context(arena& arena);

        void compile_fn_main(node *, context &);

        void gather_defs(node*, context&);
        void type_check (node*, context&);

        void emit_wasm            (wasm_opcode, node* args, context&);
        void emit_wasm_chain      (node*, context&);
        void emit_wasm_node       (node&, context&);
        void emit_wasm_body       (node*, context&);

        void emit_scoped_chain    (node*, context&);
        void emit_node            (node&, context&);
        void emit_body            (node&, context&);
        void emit_func_def        (node&, context&);
        void emit_func_call       (node&, context&);
        void emit_istr            (node&, context&);
        void emit_chr             (node&, context&);
        void emit_minus           (node&, context&);
        void emit_bop  (binary_op, node&, context&);
        void emit_bop_a(binary_op, node&, context&);

        void emit_func_call(const string& id, const arr_view<primitive_type>& params, context&);

        void push_scope(context&);
        void  pop_scope(context&);
        #define tmp_compilation_scope(ctx) push_scope(ctx); defer { pop_scope(ctx); }

        auto find_local     (string id, context&) -> variable*;
        auto find_func      (string id, const arr_view<primitive_type>& params, context&) -> func*;
        auto find_func_index(string id, const arr_view<primitive_type>& params, context&) -> ret1<uint>;

        func& declare_import(const char* module_id, const char* id, const std::initializer_list<primitive_type>& params, const std::initializer_list<primitive_type>& results, context& ctx);
        func& declare_import(string module_id, string id, const arr_view<primitive_type>& params, const arr_view<primitive_type>& results, context& ctx);

        func& declare_func(const char* id, node* body_node, bool is_inline_wasm, context&);
        func& declare_func(string id, node* body_node, bool is_inline_wasm, context&);

        void begin_func_definition(func& func, context&);
        void   end_func_definition(context&);
        #define tmp_func_definition_scope(func, ctx) begin_func_definition(func, ctx); defer { end_func_definition(ctx); }

        auto curr_func_of(context &) -> func&;

        void set_mem(const char* export_name, uint min, uint max, context&);
        void set_mem(string export_name, uint min, uint max, context&);

        void add_data(const char*   text, context&);
        auto add_data(const string& text, context&) -> uint;

        auto get_or_add_local(string id, primitive_type, context&) -> variable&;
        auto add_local       (string id, primitive_type, context&) -> variable&;

        void error_local_not_found(string id);
        void error_func_not_found (string id);

        auto add_param(string id, primitive_type, func&) -> uint;
        void add_result(primitive_type, func&);

        auto primitive_type_by(string name   ) -> primitive_type;
        auto wasm_type_of     (primitive_type) -> wasm_value_type;

        void fill_bop_to_wasm();
        auto get_bop_to_wasm(binary_op, primitive_type) -> wasm_opcode;
        void set_bop_to_wasm(binary_op, primitive_type, wasm_opcode);
    }

    arr_view<u8> compile(const ast& ast, arena& arena) {
        using namespace compilation;
        using namespace wasm_emit;
        using enum wasm_export_kind;

        fill_bop_to_wasm(); //TODO: do only once

        var ctx = make_context(arena);

        declare_import("env", "print_str", {pt_u32, pt_u32}, {   }, ctx);

        set_mem("memory", 1, 1, ctx);

        compile_fn_main(ast.root, ctx);

        // emit wasm
        ref emitter = ctx.emitter;

        var func_types = alloc<wasm_func_type>(arena, ctx.funcs.data.count);
        var imports    = make_arr_dyn<wasm_func_import>( 32, arena);
        var exports    = make_arr_dyn<wasm_export     >( 32, arena);
        var func_specs = make_arr_dyn<wasm_func       >(256, arena);

        push(exports, wasm_export {.name = ctx.exported_memory_id, .kind = ek_mem, .obj_index = 0});

        var func_i = 0u;
        for (var i = 0u; i < ctx.funcs.data.count; ++i) {
            let func = ctx.funcs.data[i];
            if (!func.is_inline_wasm); else continue;
            defer { func_i += 1; };

            func_types[func_i] = { // TODO: distinct
                .params  = alloc_g<wasm_value_type>(arena, func.params .data.count),
                .results = alloc_g<wasm_value_type>(arena, func.results.data.count),
            };

            for (var pi = 0u; pi < func.params.data.count; pi += 1)
                func_types[func_i].params[pi] = wasm_type_of(func.params.data[pi].type);

            for (var ri = 0u; ri < func.results.data.count; ri += 1)
                func_types[func_i].results[ri] = wasm_type_of(func.results.data[ri]);

            if (func.imported) {
                push(imports, {.module = func.import_module, .name = func.id, .type_index = func_i});
                assert(func.body_wasm.data.count == 0);
                continue;
            }

            //TODO: skip inline functions

            var wasm_func = wasm_emit::wasm_func {
                .type_index = func_i,
                .locals     = alloc_g<wasm_value_type>(arena, func.locals.data.count),
                .body       = func.body_wasm.data,
            };

            for (var li = 0u; li < func.locals.data.count; li += 1)
                wasm_func.locals[li] = wasm_type_of(func.locals.data[li].type);

            push(func_specs, wasm_func);

            if (func.exported)
                push(exports, wasm_export {.name = func.id, .kind = ek_func, .obj_index = func.index});
        }

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
        emit_memory_section(ctx.mem        , emitter);
        emit_export_section(exports   .data, emitter);
        emit_code_section  (func_specs.data, emitter);
        emit_data_section  (datas          , emitter);

        let wasm = emitter.wasm.data;
        print_hex(wasm);
        return wasm;
    }

    namespace compilation {
        void compile_fn_main(node* root, context & ctx) {
            using enum primitive_type;

            ref func = declare_func("main", root, false, ctx);
            add_result(pt_u32, func);
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

                node.func = &func;
                func.is_inline_wasm = is_wasm_func;

                for (var param_node_p = disp_node.first_child; param_node_p; param_node_p = param_node_p->next) {
                    ref param_node = *param_node_p;
                    if (is_func(param_node, decl_param_id)); else continue;
                    if_var2(prm_id_node, prm_type_node, deref2(param_node.first_child)); else { dbg_fail_return; }

                    add_param(prm_id_node.text, primitive_type_by(prm_type_node.text), func);
                }

                let res_type = primitive_type_by(type_node.text);
                if (res_type != pt_void)
                    add_result(res_type, func);

                if (is_wasm_func) {
                    tmp_func_definition_scope(func, ctx);
                    emit_wasm_body(body_node, ctx);
                    node.value_type = res_type;
                }

                //TODO: recurse to allow for function definitions inside expressions
            }
        }

        void type_check(node*, context&) {
            //TODO: implement
            //
            // using enum node::type_t;
            // for (var node_p = chain; node_p; node_p = node_p->next) {
            //     ref node = *node_p;
            //     if (node.type == literal) {
            //         if (node.text_is_quoted) { node.value_type = pt_str; continue; }
            //         node.value_type = pt_num_literal;
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
            //         node.value_type = local.type;
            //     }
            // }
        }

        void emit_scoped_chain(node* chain, context& ctx) {
            //TODO: maybe need to rethink this and gather definitions when creating a new scope?
            gather_defs(chain, ctx); // TODO: comptime here?
            //type_check (chain, ctx);

            for(var node = chain; node; node = node->next)
                emit_node(*node, ctx);
        }

        void emit_wasm(wasm_opcode op, node* args, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            ref body = curr_func.body_wasm;
            emit(op, body);

            var arg_p = args;
            while (arg_p) {
                ref arg = *arg_p;
                if_var1(val, get_uint(arg)); else { dbg_fail_return; }
                emit(val, body);

                arg_p = arg.next;
            }
        }

        void emit_wasm_chain(node* chain, context& ctx) {
            for(var node = chain; node; node = node->next)
                emit_wasm_node(*node, ctx);
        }

        void emit_node(node& node, context& ctx) {
            using namespace wasm_emit;
            using enum wasm_opcode;

            ref curr_func = curr_func_of(ctx);
            ref body = curr_func.body_wasm;

            if (is_str_literal(node)) {
                let str    = node.text;
                let offset = add_data(str, ctx);
                emit_const(offset   , body);
                emit_const(str.count, body);
                node.value_type = pt_str;
                return;
            }

            if (is_uint_literal(node)) {
                emit_const(node.uint_value, body);
                node.value_type = pt_u32;
                return;
            }

            if (is_int_literal(node)) {
                emit_const(node.int_value, body);
                node.value_type = pt_i32;
                return;
            }

            if (is_float_literal(node)) {
                emit_const(node.float_value, body);
                node.value_type = pt_f32;
                return;
            }

            var op = find_op(node.text);
            if (op != op_unreachable) {
                emit_wasm(op, node.first_child, ctx);
                return;
            }

            if (is_func(node)) {
                emit_func_call(node, ctx);
                return;
            }

            if_ref(local, find_local(node.text, ctx)); else { error_local_not_found(node.text); return; }
            emit(op_local_get, local.index, curr_func.body_wasm);
            node.value_type = local.type;
        }

        void emit_wasm_node(node& node, context& ctx) {
            using namespace wasm_emit;
            using enum wasm_opcode;

            let op = find_op(node.text);
            if (op != op_unreachable); else { printf("Wasm opcode not found: %.*s", (int)node.text.count, node.text.data); dbg_fail_return; }
            emit_wasm(op, node.first_child, ctx);
        }

        void emit_wasm_body(node* node, context& ctx) {
            if_ref(body, node); else return;

            if (is_func(body, block_id)) {
                emit_wasm_chain(body.first_child, ctx);
            } else {
                emit_wasm_node(body, ctx);
            }
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
            using enum wasm_value_type;

            if_var4(id_node, disp_node, type_node, body_node, deref4(node.first_child)); else { dbg_fail_return; }
            if_ref (func, node.func); else { dbg_fail_return; }

            tmp_func_definition_scope(func, ctx);

            var param_index = 0u;
            for (var param_node_p = disp_node.first_child; param_node_p; param_node_p = param_node_p->next) {
                ref param_node = *param_node_p;
                if (is_func(param_node, decl_param_id)); else continue;

                if_var2(param_id_node, param_type_node, deref2(param_node.first_child)); else { dbg_fail_return; }
                push(ctx.locals_in_scope, {
                    .id    = param_id_node.text,
                    .index = param_index,
                    .type  = primitive_type_by(param_type_node.text)
                });
                param_index++;
            }

            emit_body(body_node, ctx);
            node.value_type = pt_void;
        }

        void emit_func_call(node& node, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            ref body = curr_func.body_wasm;

            if_var1(fn_id, get_fn_id(node)); else { dbg_fail_return; }
            var args_node_p = node.first_child;

            if (fn_id == emit_local_get_id) {
                if_ref(id_node, args_node_p); else { dbg_fail_return; }
                let id = id_node.text;
                if_ref(v, find_local(id, ctx)); else { error_local_not_found(id); return; }
                emit(op_local_get, v.index, body);
                return;
            }

            if (fn_id == as_id) {
                if_var2(arg_node, type_node, deref2(args_node_p)); else { dbg_fail_return; }
                emit_node(arg_node, ctx);
                var src_type = arg_node.value_type;
                var dst_type = primitive_type_by(type_node.text);
                if (src_type == pt_f32 && dst_type == pt_u32)
                    emit(op_i32_trunc_f32_u, body);
                if (src_type == pt_f32 && dst_type == pt_i32)
                    emit(op_i32_trunc_f32_s, body);

                node.value_type = dst_type;
                return;
            }

            //Note: requires inline
            if (fn_id == ret_id) {
                if_ref(arg, args_node_p); else { dbg_fail_return; }
                emit_node(arg, ctx);
                emit(op_return, body);
                node.value_type = pt_void; //TODO: maybe some other type?
                return;
            }

            if (fn_id == decl_local_id) {
                if_var2(id_node, type_node, deref2(args_node_p)); else { dbg_fail_return; }
                ref local = add_local(id_node.text, primitive_type_by(type_node.text), ctx);
                if_ref(init_node, type_node.next) {
                    emit_node(init_node, ctx);
                    emit(op_local_set, local.index, body);
                    node.value_type = local.type; // TODO: check compatible and cast
                }
                return;
            }

            // Note: to def in source, requires compile-time execution, specifically, to be able to get the local variable id
            if (fn_id == assign_id) {
                if_var2(var_id_node, value_node, deref2(args_node_p)); else { dbg_fail_return; }
                let var_id = var_id_node.text;
                if_ref (local, find_local(var_id, ctx)); else { error_local_not_found(var_id); return; }
                emit_node(value_node, ctx);
                //TODO: return the value, but discard when used as statement
                //TODO: use op_local_tee for that and change the return type to the type of the local
                emit(op_local_set, local.index, body);
                node.value_type = pt_void;
                return;
            }

            //Note: requires overloads and inline
            if (fn_id ==       eq_id) { emit_bop  (bop_eq , node, ctx); return; }
            if (fn_id ==       ne_id) { emit_bop  (bop_ne , node, ctx); return; }
            if (fn_id ==       le_id) { emit_bop  (bop_le , node, ctx); return; }
            if (fn_id ==       ge_id) { emit_bop  (bop_ge , node, ctx); return; }
            if (fn_id ==       lt_id) { emit_bop  (bop_lt , node, ctx); return; }
            if (fn_id ==       gt_id) { emit_bop  (bop_gt , node, ctx); return; }
            if (fn_id ==      add_id) { emit_bop  (bop_add, node, ctx); return; }
            if (fn_id ==      sub_id) { emit_minus         (node, ctx); return; }
            if (fn_id ==      mul_id) { emit_bop  (bop_mul, node, ctx); return; }
            if (fn_id ==      div_id) { emit_bop  (bop_div, node, ctx); return; }
            if (fn_id ==      rem_id) { emit_bop  (bop_rem, node, ctx); return; }
            if (fn_id ==    add_a_id) { emit_bop_a(bop_add, node, ctx); return; }
            if (fn_id ==    sub_a_id) { emit_bop_a(bop_sub, node, ctx); return; }
            if (fn_id ==    mul_a_id) { emit_bop_a(bop_mul, node, ctx); return; }
            if (fn_id ==    div_a_id) { emit_bop_a(bop_div, node, ctx); return; }
            if (fn_id ==    rem_a_id) { emit_bop_a(bop_rem, node, ctx); return; }
            if (fn_id ==     istr_id) { emit_istr          (node, ctx); return; }
            if (fn_id ==      chr_id) { emit_chr           (node, ctx); return; }
            if (fn_id ==      def_id) { emit_func_def      (node, ctx); return; }
            if (fn_id == def_wasm_id) { return; }

            if (fn_id == block_id) {
                tmp_compilation_scope(ctx);

                emit(op_block, vt_void, body);
                emit_scoped_chain(args_node_p, ctx);
                emit(op_end, body);
                node.value_type = pt_void; // TODO: allow returning values from blocks
                return;
            }

            if (fn_id == if_id) {
                if_var2(cond_node, then_node, deref2(args_node_p)); else { dbg_fail_return; }
                emit_node(cond_node, ctx);

                emit(op_if, (u8)0x40, body);
                tmp_compilation_scope(ctx);
                emit_body(then_node, ctx);
                emit(op_end, body);

                node.value_type = pt_void; // TODO: allow returning from ifs
                return;
            }

            if (fn_id == while_id) {
                emit_while_scope(body);
                if_var2(cond_node, body_node, deref2(args_node_p)); else { dbg_fail_return; }

                emit_node(cond_node, ctx);
                emit(op_i32_eqz  , body);
                emit(op_br_if, 1u, body);

                tmp_compilation_scope(ctx);
                emit_body(body_node, ctx);
                node.value_type = pt_void; // TODO: allow returning from loops
                return;
            }

            if (fn_id == print_id) {
                emit_istr(node, ctx);
                emit_func_call(view("print_str"), view({pt_u32, pt_u32}), ctx);
                node.value_type = pt_void; // TODO: allow returning from prints
                return;
            }

            var params = make_arr_dyn<primitive_type>(4, ctx.arena);
            for (var arg_node_p = args_node_p; arg_node_p; arg_node_p = arg_node_p->next) {
                ref arg_node = *arg_node_p;
                emit_node(arg_node, ctx);
                push(params, arg_node.value_type);
            }

            if_ref(func, find_func(fn_id, params.data, ctx)); else { error_func_not_found(fn_id); return; }
            node.value_type = func.results.data.count == 0 ? pt_void : func.results.data[0];

            if (func.is_inline_wasm) {
                push(curr_func.body_wasm, func.body_wasm.data);
                return;
            }

            emit(op_call, func.index, curr_func.body_wasm);
        }

        void emit_istr(node& node, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            // TODO: implement with string builder
            if_var1(push_str, find_func_index(view("push"), view({pt_u32, pt_u32, pt_u32}), ctx)); else { dbg_fail_return; }
            if_var1(push_u32, find_func_index(view("push"), view({pt_u32        , pt_u32}), ctx)); else { dbg_fail_return; }
            if_var1(push_i32, find_func_index(view("push"), view({pt_i32        , pt_u32}), ctx)); else { dbg_fail_return; }
            if_var1(push_f32, find_func_index(view("push"), view({pt_f32        , pt_u32}), ctx)); else { dbg_fail_return; }

            ref body = curr_func.body_wasm;

            // write to memory, put the string on the stack
            // convert uints to strings

            // var mem_offset = 1024;
            var dst = get_or_add_local(view("istr_dst__"), pt_u32, ctx).index;
            emit_const(       1024, body);
            emit(op_local_set, dst, body);

            for(var node_p = node.first_child; node_p; node_p = node_p->next) {
                ref arg_node = *node_p;

                // dst = push_*(fn(), dst);
                emit_node(arg_node, ctx);

                let t  = arg_node.value_type;
                let fn = t == pt_str ? push_str : t == pt_i32 ? push_i32 : t == pt_f32 ? push_f32 : push_u32;
                emit(op_local_get, dst, body);
                emit(op_call,       fn, body);
                emit(op_local_set, dst, body);
            }

            // return string { 1024, mem_offset - 1024 };
            emit_const       (1024, body);
            emit(op_local_get, dst, body);
            emit_const       (1024, body);
            emit(op_i32_sub       , body);

            node.value_type = pt_str;
        }

        void emit_chr(node& node, context& ctx) {
            ref curr_func = curr_func_of(ctx);

            if_ref(value_node, node.first_child); else { dbg_fail_return; }
            if(!is_func(value_node)); else { dbg_fail_return; }
            let c = value_node.text[0];
            emit_const((uint)c, curr_func.body_wasm);
            node.value_type = pt_u32; // TODO: pt_u8
        }

        void emit_minus(node& node, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            ref body      = curr_func.body_wasm;

            if_ref(a_node, node.first_child); else { dbg_fail_return; }
            if (a_node.next); else {
                //TODO: need to determine the type before emitting the node for this
                //if (a_node.value_type == pt_f32)
                //    emit_const(0.0f, body);
                //else
                //    emit_const(0, body);

                emit_const(     0, body);
                emit_node (a_node, ctx );
                let vt = a_node.value_type;
                emit(get_bop_to_wasm(bop_sub, vt), body);
                node.value_type = vt; // TODO: check compatible and proper cast
                return;
            }

            emit_bop(bop_sub, node, ctx);
        }

        void emit_bop(binary_op op, node& node, context& ctx) {
            ref curr_func = curr_func_of(ctx);

            if_var2(a_node, b_node, deref2(node.first_child)); else { dbg_fail_return; }

            emit_node(a_node, ctx);
            emit_node(b_node, ctx);

            let vt = a_node.value_type;
            if (b_node.value_type == vt); else {
                printf("Type mismatch %u, %u\n", (uint)a_node.value_type, (uint)b_node.value_type);
                print_node(node);
                printf("\n");
                dbg_fail_return;
            }

            emit(get_bop_to_wasm(op, vt), curr_func.body_wasm);
            node.value_type = vt;
        }

        void emit_bop_a(binary_op bop, node& node, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            ref body      = curr_func.body_wasm;

            if_var2(var_id_node, value_node, deref2(node.first_child)); else { dbg_fail_return; }

            let var_id = var_id_node.text;
            if_ref   (v, find_local(var_id, ctx)); else { error_local_not_found(var_id); return; }
            emit     (op_local_get, v.index, body);
            emit_node(value_node           , ctx );
            if(v.type == value_node.value_type); else {
                printf("Type mismatch %u, %u\n", (uint)v.type, (uint)value_node.value_type);
                print_node(node);
                printf("\n");
                dbg_fail_return;
            }
            emit(get_bop_to_wasm(bop, v.type), body);
            emit(op_local_set,        v.index, body);
            node.value_type = v.type;
        }

        context make_context(arena& arena) {
            using namespace wasm_emit;
            return {
                .funcs           = make_arr_dyn<func>     ( 256, arena),
                .func_stack      = make_arr_dyn<func*>    (  16, arena),
                .scope_stack     = make_arr_dyn<scope_old>(32, arena),
                . funcs_in_scope = make_arr_dyn<func*>    ( 128, arena),
                .locals_in_scope = make_arr_dyn<variable> (  64, arena),
                .data            = make_stream        (1024, arena),
                .emitter         = make_emitter             (arena),
                .arena           =                           arena ,
            };
        }

        void emit_func_call(const string& id, const arr_view<primitive_type>& params, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            if_ref(func, find_func(id, params, ctx)); else { dbg_fail_return; }
            emit(op_call, func.index, curr_func.body_wasm);
        }

        void push_scope(context& ctx) { push(ctx.scope_stack, {
            .prev_locals_in_scope_count = ctx.locals_in_scope.data.count,
            . prev_funcs_in_scope_count = ctx. funcs_in_scope.data.count
        });}

        void pop_scope(context& ctx) { let scope = pop(ctx.scope_stack);
            ctx.locals_in_scope.data.count = scope.prev_locals_in_scope_count;
            ctx. funcs_in_scope.data.count = scope. prev_funcs_in_scope_count;
        }

        variable* find_local(string id, context& ctx) {
            for (var i = (int)ctx.locals_in_scope.data.count - 1; i >= 0; i--) {
                ref v = ctx.locals_in_scope.data[i];
                if (v.id == id) return &v;
            }
            return nullptr;
        }

        func* find_func(string id, const arr_view<primitive_type>& params, context& ctx) {
            printf("find_func: %.*s", (int)id.count, id.data);
            for (var i = 0u; i < params.count; ++i)
                printf(" %u", (uint)params[i]);
            printf("\n");

            //TODO: compare params
            for (var i = (int)ctx.funcs_in_scope.data.count - 1; i >= 0; i--) {
                if_ref(v, ctx.funcs_in_scope.data[i]); else { dbg_fail_return nullptr; }
                if (v.id == id); else continue;
                let f_params = v.params.data;
                if (f_params.count == params.count); else continue;

                var found = true;
                for (var prm_i = 0u; prm_i < params.count; ++prm_i)
                    if (f_params[prm_i].type != params[prm_i]) {
                        found = false;
                        break;
                    }

                if (found); else continue;

                return &v;
            }

            return nullptr;
        }

        ret1<uint> find_func_index(string id, const arr_view<primitive_type>& params, context& ctx) {
            if_ref(func, find_func(id, params, ctx)); else { dbg_fail_return {}; }
            return ret1_ok(func.index);
        }

        func& declare_import(const char* module_id, const char* id, const std::initializer_list<primitive_type>& params, const std::initializer_list<primitive_type>& results, context& ctx) {
            return declare_import(view(module_id), view(id), view(params), view(results), ctx);
        }

        func& declare_import(string module_id, string id, const arr_view<primitive_type>& params, const arr_view<primitive_type>& results, context& ctx) {
            let index = ctx.funcs.data.count;
            var prms = make_arr_dyn<variable>(params.count, ctx.arena);
            for (var prm_i = 0u; prm_i < params.count; ++prm_i)
                push(prms, {.id = view(""), .index = prm_i, .type = params[prm_i]});

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

        func& declare_func(const char* id, node* body_node, bool is_inline_wasm, context& ctx) {
            return declare_func(view(id), body_node, is_inline_wasm, ctx);
        }

        func& declare_func(string id, node* body_node, bool is_inline_wasm, context& ctx) {
            let index = ctx.non_inline_func_count;
            ref func  = push(ctx.funcs, {
                .id        = id,
                .index     = index,
                .body_node = body_node,
                .is_inline_wasm = is_inline_wasm,
                .params    = make_arr_dyn<variable>( 4, ctx.arena),
                .locals    = make_arr_dyn<variable>(32, ctx.arena),
                .results   = make_arr_dyn<primitive_type>( 2, ctx.arena),
                .body_wasm = make_stream(32, ctx.arena),
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

        func& curr_func_of(context & ctx) {
            var f_pp = last_of(ctx.func_stack);
            if_ref(f_p, f_pp); else { assert(false); }
            if_ref(f  , f_p ); else { assert(false); }
            return f;
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

        variable& get_or_add_local(string id, primitive_type type, context& ctx) {
            if_ref(v, find_local(id, ctx)) return v;
            return add_local(id, type, ctx);
        }

        variable& add_local(string id, primitive_type type, context& ctx) {
            ref curr_func = curr_func_of(ctx);
            var index = curr_func.params.data.count + curr_func.locals.data.count;
            ref v = push(curr_func.locals, {.id = id, .index = index, .type = type} );
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

        uint add_param(string id, primitive_type type, func& func) {
            assert(func.locals.data.count == 0); // add params before locals
            let index = func.params.data.count;
            push(func.params, {id, index, type});
            return index;
        }

        void add_result(primitive_type type, func& func) {
            push(func.results, type);
        }

        primitive_type primitive_type_by(string name){
            if (name == view("void")) return pt_void ;
            if (name == view("i8"  )) return pt_i8 ;
            if (name == view("i16" )) return pt_i16;
            if (name == view("i32" )) return pt_i32;
            if (name == view("u8 " )) return pt_u8 ;
            if (name == view("u16" )) return pt_u16;
            if (name == view("u32" )) return pt_u32;
            if (name == view("i64" )) return pt_i64;
            if (name == view("u64" )) return pt_u64;
            if (name == view("f32" )) return pt_f32;
            if (name == view("f64" )) return pt_f64;

            printf("Type %.*s not found.\n", (int)name.count, name.data);
            dbg_fail_return pt_invalid;
        }

        wasm_value_type wasm_type_of(primitive_type type) {
            switch (type) {
                case pt_void: return vt_void;
                case pt_i8 :
                case pt_i16:
                case pt_i32:
                case pt_u8 :
                case pt_u16:
                case pt_u32: return vt_i32;
                case pt_i64:
                case pt_u64: return vt_i64;
                case pt_f32: return vt_f32;
                case pt_f64: return vt_f64;

                default: dbg_fail_return wasm_value_type::vt_void;
            }
        }

        wasm_opcode get_bop_to_wasm(binary_op bop, primitive_type pt) {
            var result = bop_to_wasm_op[(u32)bop * (u32)pt_enum_size + (u32)pt];
            if (result != op_unreachable); else { printf("Unsupported combination of op %u and type %u\n", (uint)bop, (uint)pt); dbg_fail_return result; }
            return result;
        }

        void set_bop_to_wasm(binary_op bop, primitive_type pt, wasm_opcode op) {
            bop_to_wasm_op[(u32)bop * (u32)pt_enum_size + (u32)pt] = op;
        }

        void fill_bop_to_wasm() {
            set_bop_to_wasm(bop_eq  , pt_i32, op_i32_eq   );
            set_bop_to_wasm(bop_ne  , pt_i32, op_i32_ne   );
            set_bop_to_wasm(bop_lt  , pt_i32, op_i32_lt_s );
            set_bop_to_wasm(bop_gt  , pt_i32, op_i32_gt_s );
            set_bop_to_wasm(bop_le  , pt_i32, op_i32_le_s );
            set_bop_to_wasm(bop_ge  , pt_i32, op_i32_ge_s );
            set_bop_to_wasm(bop_add , pt_i32, op_i32_add  );
            set_bop_to_wasm(bop_sub , pt_i32, op_i32_sub  );
            set_bop_to_wasm(bop_mul , pt_i32, op_i32_mul  );
            set_bop_to_wasm(bop_div , pt_i32, op_i32_div_s);
            set_bop_to_wasm(bop_rem , pt_i32, op_i32_rem_s);
            set_bop_to_wasm(bop_and , pt_i32, op_i32_and  );
            set_bop_to_wasm(bop_or  , pt_i32, op_i32_or   );
            set_bop_to_wasm(bop_xor , pt_i32, op_i32_xor  );
            set_bop_to_wasm(bop_shl , pt_i32, op_i32_shl  );
            set_bop_to_wasm(bop_shr , pt_i32, op_i32_shr_s);
            set_bop_to_wasm(bop_rotl, pt_i32, op_i32_rotl );
            set_bop_to_wasm(bop_rotr, pt_i32, op_i32_rotr );

            set_bop_to_wasm(bop_eq  , pt_u32, op_i32_eq   );
            set_bop_to_wasm(bop_ne  , pt_u32, op_i32_ne   );
            set_bop_to_wasm(bop_lt  , pt_u32, op_i32_lt_u );
            set_bop_to_wasm(bop_gt  , pt_u32, op_i32_gt_u );
            set_bop_to_wasm(bop_le  , pt_u32, op_i32_le_u );
            set_bop_to_wasm(bop_ge  , pt_u32, op_i32_ge_u );
            set_bop_to_wasm(bop_add , pt_u32, op_i32_add  );
            set_bop_to_wasm(bop_sub , pt_u32, op_i32_sub  );
            set_bop_to_wasm(bop_mul , pt_u32, op_i32_mul  );
            set_bop_to_wasm(bop_div , pt_u32, op_i32_div_u);
            set_bop_to_wasm(bop_rem , pt_u32, op_i32_rem_u);
            set_bop_to_wasm(bop_and , pt_u32, op_i32_and  );
            set_bop_to_wasm(bop_or  , pt_u32, op_i32_or   );
            set_bop_to_wasm(bop_xor , pt_u32, op_i32_xor  );
            set_bop_to_wasm(bop_shl , pt_u32, op_i32_shl  );
            set_bop_to_wasm(bop_shr , pt_u32, op_i32_shr_u);
            set_bop_to_wasm(bop_rotl, pt_u32, op_i32_rotl );
            set_bop_to_wasm(bop_rotr, pt_u32, op_i32_rotr );

            set_bop_to_wasm(bop_eq  , pt_i64, op_i64_eq   );
            set_bop_to_wasm(bop_ne  , pt_i64, op_i64_ne   );
            set_bop_to_wasm(bop_lt  , pt_i64, op_i64_lt_s );
            set_bop_to_wasm(bop_gt  , pt_i64, op_i64_gt_s );
            set_bop_to_wasm(bop_le  , pt_i64, op_i64_le_s );
            set_bop_to_wasm(bop_ge  , pt_i64, op_i64_ge_s );
            set_bop_to_wasm(bop_add , pt_i64, op_i64_add  );
            set_bop_to_wasm(bop_sub , pt_i64, op_i64_sub  );
            set_bop_to_wasm(bop_mul , pt_i64, op_i64_mul  );
            set_bop_to_wasm(bop_div , pt_i64, op_i64_div_s);
            set_bop_to_wasm(bop_rem , pt_i64, op_i64_rem_s);
            set_bop_to_wasm(bop_and , pt_i64, op_i64_and  );
            set_bop_to_wasm(bop_or  , pt_i64, op_i64_or   );
            set_bop_to_wasm(bop_xor , pt_i64, op_i64_xor  );
            set_bop_to_wasm(bop_shl , pt_i64, op_i64_shl  );
            set_bop_to_wasm(bop_shr , pt_i64, op_i64_shr_s);
            set_bop_to_wasm(bop_rotl, pt_i64, op_i64_rotl );
            set_bop_to_wasm(bop_rotr, pt_i64, op_i64_rotr );

            set_bop_to_wasm(bop_eq  , pt_u64, op_i64_eq   );
            set_bop_to_wasm(bop_ne  , pt_u64, op_i64_ne   );
            set_bop_to_wasm(bop_lt  , pt_u64, op_i64_lt_u );
            set_bop_to_wasm(bop_gt  , pt_u64, op_i64_gt_u );
            set_bop_to_wasm(bop_le  , pt_u64, op_i64_le_u );
            set_bop_to_wasm(bop_ge  , pt_u64, op_i64_ge_u );
            set_bop_to_wasm(bop_add , pt_u64, op_i64_add  );
            set_bop_to_wasm(bop_sub , pt_u64, op_i64_sub  );
            set_bop_to_wasm(bop_mul , pt_u64, op_i64_mul  );
            set_bop_to_wasm(bop_div , pt_u64, op_i64_div_u);
            set_bop_to_wasm(bop_rem , pt_u64, op_i64_rem_u);
            set_bop_to_wasm(bop_and , pt_u64, op_i64_and  );
            set_bop_to_wasm(bop_or  , pt_u64, op_i64_or   );
            set_bop_to_wasm(bop_xor , pt_u64, op_i64_xor  );
            set_bop_to_wasm(bop_shl , pt_u64, op_i64_shl  );
            set_bop_to_wasm(bop_shr , pt_u64, op_i64_shr_u);
            set_bop_to_wasm(bop_rotl, pt_u64, op_i64_rotl );
            set_bop_to_wasm(bop_rotr, pt_u64, op_i64_rotr );

            set_bop_to_wasm(bop_eq  , pt_f32, op_f32_eq );
            set_bop_to_wasm(bop_ne  , pt_f32, op_f32_ne );
            set_bop_to_wasm(bop_lt  , pt_f32, op_f32_lt );
            set_bop_to_wasm(bop_gt  , pt_f32, op_f32_gt );
            set_bop_to_wasm(bop_le  , pt_f32, op_f32_le );
            set_bop_to_wasm(bop_ge  , pt_f32, op_f32_ge );
            set_bop_to_wasm(bop_add , pt_f32, op_f32_add);
            set_bop_to_wasm(bop_sub , pt_f32, op_f32_sub);
            set_bop_to_wasm(bop_mul , pt_f32, op_f32_mul);
            set_bop_to_wasm(bop_div , pt_f32, op_f32_div);

            set_bop_to_wasm(bop_eq  , pt_f64, op_f64_eq );
            set_bop_to_wasm(bop_ne  , pt_f64, op_f64_ne );
            set_bop_to_wasm(bop_lt  , pt_f64, op_f64_lt );
            set_bop_to_wasm(bop_gt  , pt_f64, op_f64_gt );
            set_bop_to_wasm(bop_le  , pt_f64, op_f64_le );
            set_bop_to_wasm(bop_ge  , pt_f64, op_f64_ge );
            set_bop_to_wasm(bop_add , pt_f64, op_f64_add);
            set_bop_to_wasm(bop_sub , pt_f64, op_f64_sub);
            set_bop_to_wasm(bop_mul , pt_f64, op_f64_mul);
            set_bop_to_wasm(bop_div , pt_f64, op_f64_div);
        }
    }
}

#endif //FRANCA2_COMPUTE_COMPILATION_H

#pragma clang diagnostic pop