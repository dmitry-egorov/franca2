#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma clang diagnostic ignored "-Wunused-variable"

#ifndef FRANCA2_COMPUTE_VIEW_GEN_H
#define FRANCA2_COMPUTE_VIEW_GEN_H

#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/iterators.h"
#include "utility/strings.h"
#include "color_palette.h"
#include "code_views.h"
#include "asts.h"
#include "asts_storage.h"

namespace compute_asts {

    inline void display(ast &ast, code_view_iterator &it, arena& arena = gta);

    namespace displaying {
        using namespace iterators;
        using namespace compute_asts;
        using namespace code_views;
        using enum palette_color;
        using enum inlay_type;

        struct context {
            code_view_iterator& iterator;
            storage storage;
            int inactive_level;
        };
#define using_display_ctx ref [it, storage, inactive_level] = ctx

        void display_node_chain(node*, context&);
        void display_node(node&, context&);
        void display_node(node*, context&);

        void display_wasm_node(node&, context&);

        void gather_definitions(node&, context&);

        void display_func_call(node&         , node* args, context&);
        void display_func_call(const st_func&, node* args, context&);

        void display_inactive  (node*, context&);
        void display_local_get (node*, context&);
        void display_ref       (node*, context&);
        void display_code      (node*, context&);
        void display_block     (node*, context&);
        void display_macro_show(node*, context&);
        void display_decl_local(node*, context&);
        void display_decl_param(node*, context&);
        void display_decl_func (node*, context&);
        void display_def_wasm  (node*, context&);
        void display_as        (node*, context&);
        void display_if        (node*, context&);
        void display_loop      (node*, context&);
        void display_istr      (node*, context&);
        void display_chr       (node*, context&);
        void display_bop_a     (node*, const string&, context&);
        void display_fn_print  (node*, context&);

        void  open_inlay_brackets(context &);
        void close_brackets(context&);
        void put_int  (palette_color color,   int value  , context&);
        void put_uint (palette_color color,  uint value  , context&);
        void put_float(palette_color color, float value  , context&);
        void put_text (palette_color color, const string&, context&);
        void put_text (palette_color color, cstr  , context&);
        void put_text_in_brackets(palette_color color, const string& text, context&);

        void put_wasm_op(const string& text, context&);
    }

    inline void display(ast& ast, code_view_iterator& it, arena& arena) {
        var storage = make_storage(arena);

        var ctx = displaying::context {it, storage, 0 };

        tmp_scope(ctx.storage);
        display_node_chain(ast.root, ctx);
    }

    namespace displaying {
        void display_node_chain(node* first_node, context& ctx) {
            using_display_ctx;

            for (var node_p = first_node; node_p; node_p = node_p->next) {
                gather_definitions(*node_p, ctx);
            }

            for (var node_p = first_node; node_p; node_p = node_p->next) {
                ref node = *node_p;
                display_node(node, ctx);
                put_text(inlays, sub_past_last(node.suffix, '\n'), ctx);
            }
        }

        void display_node(node *node, context& ctx) {
            if_ref(n, node); else return;
            display_node(n, ctx);
        }

        void display_node(node& node, context& ctx) {
            using enum node::type_t;
            using_display_ctx;
            open_inlay_brackets(ctx); defer { close_brackets(ctx); };

            if (node.type == literal && (node.text_is_quoted || node.can_be_uint || node.can_be_int || node.can_be_float)) { // literal
                put_text(node.text_is_quoted ? strings : constants, node.text, ctx);
                return;
            }

            var op = wasm_emit::find_op(node.text);
            if (op != wasm_emit::wasm_opcode::op_unreachable) {
                display_wasm_node(node, ctx);
                return;
            }

            //TODO: support strings as fn id
            if (is_func(node)) {
                display_func_call(node, node.first_child, ctx);
                return;
            }

            if_ref(v, find_var(storage, node.text)); else { printf("Variable not found: %.*s\n", (int)node.text.count, node.text.data); put_text(inlays, view(" "), ctx); dbg_fail_return; }
            put_text_in_brackets(identifiers, v.display_name, ctx);
        }

        void display_wasm_node(node& node, context& ctx) {
            if_ref (local, find_var(ctx.storage, node.text)) {
                put_text_in_brackets(identifiers, local.display_name, ctx);
                return;
            }

            put_wasm_op(node.text, ctx);

            for (var arg_p = node.first_child; arg_p; arg_p = arg_p->next) {
                ref arg = *arg_p;

                put_text(regulars, view(" "), ctx);
                if_var1(vu, get_uint (arg)) { put_uint (constants, vu, ctx); continue; }
                if_var1(vi, get_int  (arg)) { put_int  (constants, vi, ctx); continue; }
                if_var1(vf, get_float(arg)) { put_float(constants, vf, ctx); continue; }
                if_ref (local_prm, find_var(ctx.storage, arg.text)) { put_text_in_brackets(identifiers, local.display_name, ctx); continue; }
            }
        }

        void gather_definitions(node& node, context& context) {
            using enum node::type_t;
            if (is_func(node, def_id) || is_func(node, def_wasm_id)); else return;

            if_var3(id_node, disp_node, type_node, deref3(node.first_child)); else { dbg_fail_return; }
            var id = id_node.text;

            var params = make_arr_dyn<prim_type>(8, context.storage.arena);
            var param_node_p = disp_node.first_child;
            while (param_node_p) {
                ref param_node = *param_node_p;
                if (param_node.type == func)
                    push(params, param_node.value_type);

                param_node_p = param_node.next;
            }

            push_func(context.storage, st_func { id, &disp_node, &type_node, params.data});
        }

        void display_func_call(node& n, node* args, context& ctx) {
            let fn_id = n.text;

            //if (fn_id == "(uint)inactive"     ) { display_inactive     (args_node_p, ctx); return; } //TODO: parse comments as inactive node?
            if (fn_id == emit_local_get_id) { display_local_get (args, ctx); return; }
            if (fn_id ==             in_id) { display_decl_param(args, ctx); return; }
            if (fn_id ==            ref_id) { display_ref       (args, ctx); return; }
            if (fn_id ==           code_id) { display_code      (args, ctx); return; }
            if (fn_id ==          block_id) { display_block     (args, ctx); return; }
            if (fn_id ==     decl_local_id) { display_decl_local(args, ctx); return; }
            if (fn_id ==            def_id) { display_decl_func (args, ctx); return; }
            if (fn_id ==       def_wasm_id) { display_def_wasm  (args, ctx); return; }
            if (fn_id ==             as_id) { display_as        (args, ctx); return; }
            if (fn_id ==             if_id) { display_if        (args, ctx); return; }
            if (fn_id ==          while_id) { display_loop      (args, ctx); return; }
            if (fn_id ==           istr_id) { display_istr      (args, ctx); return; }
            if (fn_id ==            chr_id) { display_chr       (args, ctx); return; }
            //if (fn_id ==          add_a_id) { display_bop_a     (args, view(" += "), ctx); return; }
            if (fn_id ==          sub_a_id) { display_bop_a     (args, view(" -= "), ctx); return; }
            if (fn_id ==          mul_a_id) { display_bop_a     (args, view(" *= "), ctx); return; }
            if (fn_id ==          div_a_id) { display_bop_a     (args, view(" /= "), ctx); return; }
            if (fn_id ==          rem_a_id) { display_bop_a     (args, view(" %= "), ctx); return; }
            if (fn_id ==          print_id) { display_fn_print  (args, ctx); return; }
            if (fn_id ==           show_id) { display_macro_show(args, ctx); return; }

            var params = make_arr_dyn<prim_type>(8, ctx.storage.arena);
            var arg_p = args;
            while (arg_p) {
                ref arg = *arg_p;
                push(params, arg.value_type);
                arg_p = arg.next;
            }

            if_ref(fn, find_func(ctx.storage, fn_id, params.data)); else {
                put_text_in_brackets(inlays, view(" "), ctx);
                printf("unknown function: %.*s\n", (int)fn_id.count, fn_id.data);
                dbg_fail_return;
            }

            display_func_call(fn, args, ctx);
        }

        void display_func_call(const st_func& fn, node* args, context& ctx) {
            using enum node::type_t;

            var disp_node_p = fn.display->first_child;

            if (disp_node_p) put_text(inlays, disp_node_p->prefix, ctx);
            //TODO: support evaluating signature?
            while(disp_node_p) {
                ref disp_node = *disp_node_p;
                switch (disp_node.type) {
                    case literal: {
                        put_text(regulars, disp_node.text, ctx);
                        break;
                    }
                    case func: {
                        if(is_func(disp_node, view("$")) || is_func(disp_node, view("&")) || is_func(disp_node, view("#"))); else {
                            printf("Only parameter nodes are supported in display strings: %.*s\n", (int)disp_node.text.count, disp_node.text.data);
                            dbg_fail_return;
                        }
                        if_ref(arg_node, args); else {
                            printf("Missing parameter: %.*s\n", (int)disp_node.first_child->text.count, disp_node.first_child->text.data);
                            dbg_fail_return;
                        }
                        display_node(arg_node, ctx);
                        args = arg_node.next;
                        break;
                    }
                }

                put_text(inlays, disp_node_p->suffix, ctx);
                disp_node_p = disp_node.next;
            }
        }

        void display_inactive(node* args, context& ctx) {
            if_ref(args_list, args); else return;

            put_text(inlays, args_list.prefix, ctx);

            ctx.inactive_level += 1; defer { ctx.inactive_level -= 1; };
            display_node_chain(args, ctx);
        }

        void display_local_get(node* args, context& ctx) {
            if_ref(id_node, args); else return;
            let id = id_node.text;
            if_ref(v, find_var(ctx.storage, id)); else { printf("Variable not found: %.*s\n", (int) id.count, id.data); put_text(inlays, view(" "), ctx); dbg_fail_return; }
            put_text(regulars, view("local.get "), ctx);
            put_text_in_brackets(identifiers, v.display_name, ctx);
        }

        void display_ref(node* args, context& ctx) {
            if_ref(id_node, args); else return;
            let id = id_node.text;
            var name = id;
            if_ref(v, find_var(ctx.storage, id))
                name = v.display_name;

            put_text_in_brackets(identifiers, name, ctx); //TODO: mark as ref?
        }

        void display_code(node* args, context& ctx) {
            if_ref(id_node, args); else return;
            let id = id_node.text;
            var name = id;
            if_ref(v, find_var(ctx.storage, id))
                name = v.display_name;

            push_var(ctx.storage, {id, name});
            put_text_in_brackets(identifiers, name, ctx); //TODO: mark as ref?
        }

        void display_block(node* args, context& ctx) {
            using_display_ctx;

            if_ref(args_list, args); else return;

            tmp_scope (storage);
            tmp_indent(it);

            put_text(inlays, "\n", ctx);
            display_node_chain(args, ctx);
        }

        void display_decl_local(node* args, context& ctx) {
            using_display_ctx;

            if_var2(id_node, type_node, deref2(args)); else { dbg_fail_return; }
            var id   = id_node.text;
            var type_name = type_node.text;
            var name = id;
            var init_node_p = type_node.next;

            put_text(controls, view("var "), ctx);

            if (init_node_p) { if_ref(name_node, init_node_p->next) {
                name = name_node.text;
            }}

            put_text_in_brackets(identifiers, name, ctx);
            put_text(regulars, view(": "), ctx);
            put_text_in_brackets(definitions, type_name, ctx);

            if_ref(init_node, init_node_p) {
                put_text(regulars, view(" = "), ctx);
                display_node(init_node, ctx);
            }

            push_var(storage, {id, name});
        }

        void display_decl_param(node* args, context& ctx) {
            using_display_ctx;

            if_var2(id_node, type_node, deref2(args)); else { dbg_fail_return; }
            var id = id_node.text;
            var type_name = type_node.text;
            var name = id;
            if_ref(name_node, type_node.next)
                name = name_node.text;

            push_var(storage, {id, name});

            put_text_in_brackets(identifiers, name, ctx);
            put_text(regulars, view(": "), ctx);
            put_text_in_brackets(definitions, type_name, ctx);
        }

        void display_decl_func(node* args, context& ctx) {
            using_display_ctx;

            if_var4(__, disp_node, type_node, body_node, deref4(args)); else { dbg_fail_return; }
            var type_name = type_node.text;

            put_text(definitions, view("def "), ctx);

            tmp_scope(storage);
            display_node(disp_node, ctx);
            put_text(regulars, view(": "), ctx);
            put_text_in_brackets(definitions, type_name, ctx);
            put_text(regulars, view(" -> "), ctx);
            display_node(body_node, ctx);
        }

        void display_def_wasm(node* args, context& ctx) {
            using_display_ctx;

            if_var3(__, disp_node, type_node, deref3(args)); else { dbg_fail_return; }
            var type_name = type_node.text;

            put_text(definitions, view("wasm "), ctx);

            tmp_scope(storage);
            display_node(disp_node, ctx);
            put_text(regulars, view(": "), ctx);
            put_text_in_brackets(definitions, type_name, ctx);
            if_ref(body_node, type_node.next); else return;
            put_text(regulars, view(" -> "), ctx);

            if (is_func(body_node, block_id)) {
                tmp_scope(storage);
                for (var node_p = body_node.first_child; node_p; node_p = node_p->next) {
                    display_wasm_node(*node_p, ctx);
                    put_text(inlays, sub_past_last(node_p->suffix, '\n'), ctx);
                }
                return;
            }

            display_wasm_node(body_node, ctx);
        }

        void display_as(node* args, context& ctx) {
            if_var2(arg_node, type_node, deref2(args)); else { dbg_fail_return; }

            display_node(arg_node, ctx);
            put_text(regulars, " as ", ctx);
            put_text(definitions, type_node.text, ctx);
        }

        void display_if(node* args, context& ctx) {
            using_display_ctx;

            if_var2(cond_node, body_node, deref2(args)); else { dbg_fail_return; }
            put_text(controls, view("if "), ctx);

            display_node(cond_node, ctx);
            put_text(regulars, cond_node.suffix, ctx);
            display_node(body_node, ctx);
        }

        void display_loop(node* args, context& ctx) {
            using_display_ctx;

            if_var2(cond_node, body_node, deref2(args)); else { dbg_fail_return; }
            put_text(controls, view("while "), ctx);

            display_node(cond_node, ctx);
            put_text(regulars, cond_node.suffix, ctx);
            display_node(body_node, ctx);
        }

        void display_istr(node* args, context& ctx) {
            using_display_ctx;

            if_ref(args_list, args); else { put_text(strings, view(" "), ctx); return;}
            display_node_chain(&args_list, ctx);
        }

        void display_chr(node* args, context& ctx) {
            using_display_ctx;

            if_ref(arg, args); else { put_text(strings, view(" "), ctx); return;}
            put_text(strings, "'", ctx);
            put_text(strings, sub(arg.text, 1), ctx);
            put_text(strings, "'", ctx);
        }

        void display_bop_a(node * args, const string& infix_text, context & ctx) {
            using_display_ctx;

            if_var2(name_node, b_node, deref2(args)); else { dbg_fail_return; }
            put_text(identifiers, name_node.text, ctx);
            put_text(regulars   , infix_text    , ctx);
            display_node(b_node, ctx);
        }

        void display_fn_print(node* args, context& ctx) {
            using_display_ctx;
            put_text(regulars, view("print "), ctx);
            display_istr(args, ctx);
        }

        void display_macro_show(node* args, context& ctx) {
            if_ref (id_node, args); else { dbg_fail_return; }
            var id = id_node.text;

            put_text(definitions, view("show "), ctx);
            put_text(inlays     , id_node.prefix, ctx);
            put_text(identifiers, id, ctx);
            put_text(inlays     , id_node.suffix, ctx);
            put_text(definitions, view("as "), ctx);

            if_ref(disp_node, id_node.next); else { dbg_fail_return; }
            display_node(disp_node, ctx);
        }

        void open_inlay_brackets(context & ctx) {
            set_inlay(ctx.iterator, open);
        }

        void close_brackets(context& ctx) {
            set_inlay_prev(ctx.iterator, close);
        }

        void put_int(palette_color color, int value, context& ctx) {
            put_int(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, value);
        }

        void put_uint(palette_color color, uint value, context& ctx) {
            put_uint(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, value);
        }

        void put_float(palette_color color, float value, context& ctx) {
            put_float(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, value);
        }

        void put_text(palette_color color, const string& text, context& ctx) {
            put_text(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, text);
        }

        void put_text(palette_color color, cstr text, context& ctx) {
            put_text(color, view(text), ctx);
        }

        void put_text_in_brackets(palette_color color, const string& text, context& ctx) {
            put_text_in_brackets(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, text);
        }

        void put_wasm_op(const string& text, context& ctx) {
            var t = text;
            advance(t, 3);
            while(!is_empty(t)) {
                let part = take_until(t, '_');
                put_text(regulars, part, ctx);
                if (!is_empty(t)); else break;
                take(t);
                put_text(regulars, view("."), ctx);
            }
        }
    }
}

#endif //FRANCA2_COMPUTE_VIEW_GEN_H
#pragma clang diagnostic pop