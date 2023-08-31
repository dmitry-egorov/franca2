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
#include "compute_asts.h"
#include "compute_storage.h"

namespace compute_asts {

    inline void display(ast &ast, code_view_iterator &it, arena& arena = gta);

    namespace displaying {
        using namespace iterators;
        using namespace compute_asts;
        using namespace code_views;
        using enum palette_color;
        using enum poly_value::type_t;
        using enum inlay_type;
        using enum builtin_func_id;

        struct context {
            code_view_iterator& iterator;
            storage storage;
            int inactive_level;
        };
#define using_display_ctx ref [it, storage, inactive_level] = ctx

        void display_node_chain(node*, context&);
        void display_node(node&, context&);
        void display_node(node*, context&);

        void gather_definitions(node&, context&);

        bool display_func(string fn_id, node* args, context&);
        void display_func(const func& , node* args, context&);

        void display_inactive  (node*, context&);
        void display_block     (node*, context&);
        void display_macro_show(node*, context&);
        void display_decl_local  (node *, context &);
        void display_decl_param(node*, context&);
        void display_decl_func (node*, context&);
        void display_ref_var   (node*, context&);
        void display_return    (node*, context&);
        void display_as        (node*, context&);
        void display_if        (node*, context&);
        void display_loop      (node*, context&);
        void display_istr      (node*, context&);
        void display_chr       (node*, context&);
        void display_store8    (node*, context&);
        void display_op_assign (node*, context&);
        void display_minus     (node*, context&);
        void display_bop       (node*, const string&, context&);
        void display_bop_a     (node*, const string&, context&);
        void display_fn_print  (node*, context&);

        void  open_inlay_brackets(context &);
        void close_brackets(context&);
        void put_value(palette_color color, poly_value value, context&);
        void put_uint(palette_color color, uint  value  , context&);
        void put_text(palette_color color, const string&, context&);
        void put_text(palette_color color, const char*  , context&);
        void put_text_in_brackets(palette_color color, const string& text, context&);

        palette_color color_of(poly_value value);
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

            if (node.type == literal) { // literal
                open_inlay_brackets(ctx); defer { close_brackets(ctx); };

                put_text(is_uint_literal(node) ? constants : strings, node.text, ctx);
                return;
            }

            //func
            open_inlay_brackets(ctx); defer { close_brackets(ctx); };

            var args_node_p = node.first_child;
            var last_arg_p  = node. last_child;

            //TODO: support string literals as fn id
            let fn_id = node.text;

            //if (fn_id == "(uint)inactive"     ) { display_inactive     (args_node_p, ctx); return; } //TODO: parse comments as inactive node?
            if (fn_id ==      block_id) { display_block     (args_node_p, ctx); return; }
            if (fn_id == decl_local_id) { display_decl_local(args_node_p, ctx); return; }
            if (fn_id == decl_param_id) { display_decl_param(args_node_p, ctx); return; }
            if (fn_id ==        def_id) { display_decl_func (args_node_p, ctx); return; }
            if (fn_id ==        ref_id) { display_ref_var   (args_node_p, ctx); return; }
            if (fn_id ==        ret_id) { display_return    (args_node_p, ctx); return; }
            if (fn_id ==         as_id) { display_as        (args_node_p, ctx); return; }
            if (fn_id ==         if_id) { display_if        (args_node_p, ctx); return; }
            if (fn_id ==      while_id) { display_loop      (args_node_p, ctx); return; }
            if (fn_id ==       istr_id) { display_istr      (args_node_p, ctx); return; }
            if (fn_id ==        chr_id) { display_chr       (args_node_p, ctx); return; }
            if (fn_id == store_u8_id) { display_store8      (args_node_p, ctx); return; }
            if (fn_id ==     assign_id) { display_op_assign (args_node_p, ctx); return; }
            if (fn_id ==        sub_id) { display_minus     (args_node_p, ctx); return; }
            if (fn_id ==        add_id) { display_bop       (args_node_p, view(" + " ), ctx); return; }
            if (fn_id ==        mul_id) { display_bop       (args_node_p, view(" * " ), ctx); return; }
            if (fn_id ==        div_id) { display_bop       (args_node_p, view(" / " ), ctx); return; }
            if (fn_id ==        rem_id) { display_bop       (args_node_p, view(" % " ), ctx); return; }
            if (fn_id ==      add_a_id) { display_bop_a     (args_node_p, view(" += "), ctx); return; }
            if (fn_id ==      sub_a_id) { display_bop_a     (args_node_p, view(" -= "), ctx); return; }
            if (fn_id ==      mul_a_id) { display_bop_a     (args_node_p, view(" *= "), ctx); return; }
            if (fn_id ==      div_a_id) { display_bop_a     (args_node_p, view(" /= "), ctx); return; }
            if (fn_id ==      rem_a_id) { display_bop_a     (args_node_p, view(" %= "), ctx); return; }
            if (fn_id ==         eq_id) { display_bop       (args_node_p, view(" == "), ctx); return; }
            if (fn_id ==         ne_id) { display_bop       (args_node_p, view(" != "), ctx); return; }
            if (fn_id ==         le_id) { display_bop       (args_node_p, view(" <= "), ctx); return; }
            if (fn_id ==         ge_id) { display_bop       (args_node_p, view(" >= "), ctx); return; }
            if (fn_id ==         lt_id) { display_bop       (args_node_p, view(" < " ), ctx); return; }
            if (fn_id ==         gt_id) { display_bop       (args_node_p, view(" > " ), ctx); return; }
            if (fn_id ==      print_id) { display_fn_print  (args_node_p, ctx); return; }
            if (fn_id ==       show_id) { display_macro_show(args_node_p, ctx); return; }

            if(display_func(fn_id, args_node_p, ctx)); else {
                put_text_in_brackets(inlays, view(" "), ctx);
                printf("unknown function: %.*s\n", (int)fn_id.count, fn_id.data);
                dbg_fail_return;
            }
        }

        void gather_definitions(node& node, context& context) {
            if (is_func(node, def_id)); else return;

            if_var4(id_node, disp_node, tyoe_node, body_node, deref4(node.first_child)); else { dbg_fail_return; }
            var id = id_node.text;

            push_func(context.storage, compute_asts::func { id, &disp_node, &body_node, &tyoe_node });
        }

        bool display_func(string fn_id, node* args, context& ctx) {
            if_ref(fn, find_func(ctx.storage, fn_id)); else return false;
            display_func(fn, args, ctx);
            return true;
        }

        void display_func(const func& fn, node* args, context& ctx) {
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
                        assert(is_func(disp_node, view("$")));
                        if_ref(arg_node, args); else { dbg_fail_return; }
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

        void display_block(node* args, context& ctx) {
            using_display_ctx;

            if_ref(args_list, args); else return;

            tmp_scope (storage);
            tmp_indent(it);

            next_line(it);
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

            push_var(storage, {id, name, {}});
        }

        void display_decl_param(node* args, context& ctx) {
            using_display_ctx;

            if_var2(id_node, type_node, deref2(args)); else { dbg_fail_return; }
            var id = id_node.text;
            var type_name = type_node.text;
            var name = id;
            if_ref(name_node, type_node.next)
                name = name_node.text;

            push_var(storage, {id, name, {}});

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

        void display_ref_var(node* args, context& ctx) {
            using_display_ctx;

            if_ref(id_node, args); else { dbg_fail_return; }
            var id = id_node.text;

            if_ref(v, find_var(storage, id)); else { put_text(inlays, view(" "), ctx); dbg_fail_return; }

            put_text_in_brackets(identifiers, v.display_name, ctx);
        }

        void display_return(node* args, context& ctx) {
            using_display_ctx;

            put_text(controls, view("return "), ctx);

            if_ref (value_node, args); else { return; }
            display_node(value_node, ctx);
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

        void display_store8(node* args, context& ctx) {
            using_display_ctx;

            if_var2(mem_offset_node, value_node, deref2(args)); else { dbg_fail_return; }

            put_text(regulars, view("store "), ctx);
            display_node(value_node, ctx);
            put_text(regulars, view(" at "), ctx);
            display_node(mem_offset_node, ctx);
        }

        void display_op_assign(node* args, context& ctx) {
            using_display_ctx;

            if_var2(id_node, value_node, deref2(args)); else { dbg_fail_return; }
            var id = id_node.text;
            if_ref (v , find_var(storage, id)); else { put_text(inlays, view(" "), ctx); dbg_fail_return; }

            put_text(identifiers, v.display_name, ctx);
            put_text(regulars, view(" = "), ctx);
            display_node(value_node, ctx);
        }

        void display_minus(node* args, context& ctx) {
            using_display_ctx;

            if_ref(a_node, args); else { dbg_fail_return; }
            if_ref(b_node, a_node.next); else {
                put_text(regulars, "-", ctx);
                display_node(a_node, ctx);
                return;
            }

            display_node(a_node, ctx);
            put_text(regulars, " - ", ctx);
            display_node(b_node, ctx);
        }

        void display_bop(node* args, const string& infix_text, context& ctx) {
            using_display_ctx;

            if_var2(a_node, b_node, deref2(args)); else { dbg_fail_return; }
            display_node(a_node, ctx);
            put_text(regulars, infix_text, ctx);
            display_node(b_node, ctx);
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
            using_display_ctx;

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

        void put_value(palette_color color, poly_value value, context& ctx) {
            switch (value.type) {
                case poly_integer: { put_uint(color, value.integer, ctx); break; }
                case poly_string : { put_text(color, value.string , ctx); break; }
                default: { dbg_fail_return; }
            }
        }

        void put_uint(palette_color color, uint value, context& ctx) {
            put_uint(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, value);
        }

        void put_text(palette_color color, const string& text, context& ctx) {
            put_text(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, text);
        }

        void put_text(palette_color color, const char* text, context& ctx) {
            put_text(color, view(text), ctx);
        }

        void put_text_in_brackets(palette_color color, const string& text, context& ctx) {
            put_text_in_brackets(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, text);
        }

        palette_color color_of(poly_value value) {
            switch (value.type) {
                case poly_integer: return constants;
                case poly_string : return strings;
            }
            dbg_fail_return regulars; // unreachable
        }
    }
}

#endif //FRANCA2_COMPUTE_VIEW_GEN_H
#pragma clang diagnostic pop