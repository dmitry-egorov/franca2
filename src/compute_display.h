#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

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
        using enum node::type_t;
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
    }

    inline void display(ast& ast, code_view_iterator& it, arena& arena) {
        var storage = make_storage(arena);
        var ctx = displaying::context {it, storage, 0 };
        display_node_chain(ast.root, ctx);
    }

    namespace displaying {
        void display_node(node&, context&);
        void display_node(node*, context&);

        void display_literal      (node&, context&);
        void display_func         (node&, context&);
        void display_inactive     (node*, context&);
        void display_block        (node*, context&);
        void display_macro_show_as(node*, context&);
        void display_decl_var     (node*, context&);
        void display_decl_param   (node*, context&);
        void display_decl_func    (node*, context&);
        void display_ref_var      (node*, context&);
        void display_return       (node*, context&);
        void display_if           (node*, context&);
        void display_loop         (node*, context&);
        void display_istring      (node*, context&);
        void display_op_assign    (node*, context&);
        void display_int_binary_op(node*, context&, const string&);
        void display_fn_print     (node*, context&);

        void  open_brackets(context&);
        void close_brackets(context&);
        void put_value(palette_color color, poly_value value, context&);
        void put_uint(palette_color color, uint value, context&);
        void put_text(palette_color color, const string& text, context&);
        void put_text_in_brackets(palette_color color, const string& text, context&);

        palette_color color_of(poly_value value);

        void display_node(node *node, context& ctx) {
            if_ref(n, node); else return;
            display_node(n, ctx);
        }

        void display_node(node& node, context& ctx) {
            using_display_ctx;

            switch (node.type) {
                case literal: { display_literal(node, ctx); break; }
                case func:    { display_func   (node, ctx); break; }
            }
        }

        void display_node_chain(node* first_node, context& ctx) {
            using_display_ctx;

            for (var node_p = first_node; node_p; node_p = node_p->next) {
                ref node = *node_p;
                display_node(node, ctx);
                put_text(inlays, node.suffix, ctx);
            }
        }

        void display_literal(node& node, context& ctx) {
            var hide_brackets = is_str_literal(node) && parent_is_func(node, (uint)istring);
            if (!hide_brackets) open_brackets(ctx); defer { if (!hide_brackets) close_brackets(ctx); };

            put_value(color_of(node.value), node.value, ctx);
        }

        void display_func(node& node, context& ctx) {
            using_display_ctx;

            open_brackets(ctx); defer { close_brackets(ctx); };

            if_ref(first_child, node.first_child); else { put_text(inlays, view_of(" "), ctx); return; }
            if_ref( last_child, node. last_child); else { dbg_fail_return; }

            put_text(inlays, first_child.prefix, ctx);

            if_var1(fn_id, get_int(first_child)); else { dbg_fail_return; }

            let args = first_child.next;
            if (fn_id == (uint)inactive     ) { display_inactive     (args, ctx); return; }
            if (fn_id == (uint)block        ) { display_block        (args, ctx); return; }
            if (fn_id == (uint)decl_var     ) { display_decl_var     (args, ctx); return; }
            if (fn_id == (uint)decl_param   ) { display_decl_param   (args, ctx); return; }
            if (fn_id == (uint)decl_func    ) { display_decl_func    (args, ctx); return; }
            if (fn_id == (uint)ref_var      ) { display_ref_var      (args, ctx); return; }
            if (fn_id == (uint)ctr_return   ) { display_return       (args, ctx); return; }
            if (fn_id == (uint)ctr_if       ) { display_if           (args, ctx); return; }
            if (fn_id == (uint)ctr_loop     ) { display_loop         (args, ctx); return; }
            if (fn_id == (uint)istring      ) { display_istring      (args, ctx); return; }
            if (fn_id == (uint)op_assign    ) { display_op_assign    (args, ctx); return; }
            if (fn_id == (uint)op_add       ) { display_int_binary_op(args, ctx, view_of(" + " )); return; }
            if (fn_id == (uint)op_sub       ) { display_int_binary_op(args, ctx, view_of(" - " )); return; }
            if (fn_id == (uint)op_eq        ) { display_int_binary_op(args, ctx, view_of(" == ")); return; }
            if (fn_id == (uint)op_neq       ) { display_int_binary_op(args, ctx, view_of(" != ")); return; }
            if (fn_id == (uint)op_lte       ) { display_int_binary_op(args, ctx, view_of(" <= ")); return; }
            if (fn_id == (uint)op_gte       ) { display_int_binary_op(args, ctx, view_of(" >= ")); return; }
            if (fn_id == (uint)op_lt        ) { display_int_binary_op(args, ctx, view_of(" < " )); return; }
            if (fn_id == (uint)op_gt        ) { display_int_binary_op(args, ctx, view_of(" > " )); return; }
            if (fn_id == (uint)fn_print     ) { display_fn_print     (args, ctx); return; }
            if (fn_id == (uint)macro_show_as) { display_macro_show_as(args, ctx); return; }

            if_ref (fn, find_func(storage, fn_id)); else { put_text(inlays, view_of(" "), ctx); dbg_fail_return; }

            var args_node_p = first_child.next;
            var  sig_node_p = fn.display->first_child->next;

            //TODO: support evaluating signature?
            while(sig_node_p) {
                ref sig_node = *sig_node_p;
                switch (sig_node.type) {
                    case literal: {
                        put_value(regulars, sig_node.value, ctx);
                        break;
                    }
                    case func: {
                        assert(is_func(sig_node, (uint)decl_param));
                        if_ref(arg_node, args_node_p); else { dbg_fail_return; }
                        display_node(arg_node, ctx);
                        args_node_p = arg_node.next;
                        break;
                    }
                }

                sig_node_p = sig_node.next;
            }

            put_text(inlays, last_child.suffix, ctx);
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

        void display_decl_var(node* args, context& ctx) {
            using_display_ctx;

            if_var3 (id_node, mut_node, name_node, deref_list3(args)); else { dbg_fail_return; }
            if_vari3(id, get_int(id_node), mut, get_int(mut_node), name, get_str(name_node)); else { dbg_fail_return; }

            put_text(regulars, view_of(mut ? "var" : "let"), ctx);
            put_text(inlays, name_node.prefix, ctx);
            put_text_in_brackets(identifiers, name, ctx);
            put_text(inlays, name_node.suffix, ctx);

            if_ref(init_node, name_node.next) {
                put_text(regulars, view_of("= "), ctx);
                display_node(init_node, ctx);
            }

            push_var(storage, {id, mut != 0, name, {}});
        }

        void display_decl_param(node* args, context& ctx) {
            using_display_ctx;

            if_var2 (id_node, name_node, deref_list2(args)); else { dbg_fail_return; }
            if_vari2(id, get_int(id_node), name, get_str(name_node)); else { dbg_fail_return; }
            put_text_in_brackets(identifiers, name, ctx);

            push_var(storage, {id, false, name, {}});
        }

        void display_decl_func(node* args, context& ctx) {
            using_display_ctx;

            if_var3(id_node, disp_node, body_node, deref_list3(args)); else { dbg_fail_return; }
            if_var1(id, get_int(id_node)); else { dbg_fail_return; }

            push_func(storage, compute_asts::func { id, &disp_node, &body_node });

            put_text(regulars, view_of("fn "), ctx);

            tmp_scope(storage);
            display_node(disp_node, ctx);
            put_text(regulars, view_of(": "), ctx);
            display_node(body_node, ctx);
        }

        void display_ref_var(node* args, context& ctx) {
            using_display_ctx;

            if_ref (id_node, args)                 ; else { dbg_fail_return; }
            if_var1(id     , get_int(id_node))     ; else { dbg_fail_return; }
            if_ref (v      , find_var(storage, id)); else { put_text(inlays, view_of(" "), ctx); dbg_fail_return; }

            put_text_in_brackets(identifiers, v.name, ctx);
        }

        void display_return(node* args, context& ctx) {
            using_display_ctx;

            put_text(regulars, view_of("return "), ctx);

            if_ref (value_node, args); else { return; }
            display_node(value_node, ctx);
        }

        void display_if(node* args, context& ctx) {
            using_display_ctx;

            if_var2(cond_node, body_node, deref_list2(args)); else { dbg_fail_return; }
            put_text(regulars, view_of("if "), ctx);

            display_node(cond_node, ctx);
            put_text(regulars, cond_node.suffix, ctx);
            display_node(body_node, ctx);
        }

        void display_loop(node* args, context& ctx) {
            using_display_ctx;

            if_var2(cond_node, body_node, deref_list2(args)); else { dbg_fail_return; }
            put_text(regulars, view_of("while "), ctx);

            display_node(cond_node, ctx);
            put_text(regulars, cond_node.suffix, ctx);
            display_node(body_node, ctx);
        }

        void display_istring(node* args, context& ctx) {
            using_display_ctx;

            if_ref(args_list, args); else { put_text(strings, view_of(" "), ctx); return;}
            display_node_chain(&args_list, ctx);
        }
        void display_op_assign(node* args, context& ctx) {
            using_display_ctx;

            if_var2(id_node, value_node, deref_list2(args)); else { dbg_fail_return; }

            if_var1(id, get_int (id_node)    ); else { dbg_fail_return; }
            if_ref (v , find_var(storage, id)); else { put_text(inlays, view_of(" "), ctx); dbg_fail_return; }

            put_text(identifiers, v.name, ctx);
            put_text(regulars, view_of(" = "), ctx);
            display_node(value_node, ctx);
        }

        void display_int_binary_op(node* args, context& ctx, const string& infix_text) {
            using_display_ctx;

            if_var2(a_node, b_node, deref_list2(args)); else { dbg_fail_return; }
            display_node(a_node, ctx);
            put_text(regulars, infix_text, ctx);
            display_node(b_node, ctx);
        }

        void display_fn_print(node* args, context& ctx) {
            if_ref(text_node, args); else return;

            using_display_ctx;
            put_text(regulars, view_of("print "), ctx);
            display_node(text_node, ctx);
        }

        void display_macro_show_as(node* args, context& ctx) {
            using_display_ctx;

            if_ref (id_node, args       ); else { dbg_fail_return; }
            if_var1(id, get_int(id_node)); else { dbg_fail_return; }

            put_text(definitions, view_of("show"), ctx);
            put_text(inlays, id_node.prefix, ctx);
            put_uint(constants, id, ctx);
            put_text(inlays, id_node.suffix, ctx);
            put_text(definitions, view_of("as "), ctx);

            if_ref(disp_node, id_node.next); else { dbg_fail_return; }
            display_node(disp_node, ctx);
        }

        void open_brackets(context& ctx) {
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