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

    inline void display(ast &ast, const arrays::array_view<variable> &storage, code_views::code_view_iterator &it);

    namespace displaying {
        using namespace iterators;
        using namespace compute_asts;
        using namespace code_views;
        using namespace strings;
        using enum palette_color;
        using enum node::type_t;
        using enum poly_value::type_t;
        using enum inlay_type;
        using enum builtin_func_id;

        struct context {
            code_view_iterator& iterator;
            array_view<variable> storage;
            bool show_top_level_inlays_for_strings;
            uint inactive_level;
        };
#define using_display_ctx ref [____, defs, str_inlays, inactive_level] = ctx

        void display_list(const node&, context&);
        void display_list(const node*, context&);
    }

    inline void display(ast& ast, const arrays::array_view<variable>& storage, code_views::code_view_iterator& it) {
        var ctx = displaying::context {it, storage, true, 0 };
        display_list(ast.root, ctx);
    }

    namespace displaying {
        void display_node (const node&, context&);
        void display_node (const node*, context&);

        bool display_func(const node& node, context&);
        void display_inactive     (const node*, context&);
        void display_macro_show_as(const node*, context&);
        void display_decl_var     (const node*, context&);
        void display_decl_param   (const node*, context&);
        void display_var_ref      (const node*, context&);
        void display_istring      (const node*, context&);
        void display_op_assign    (const node*, context&);
        void display_op_plus      (const node*, context&);
        void display_fn_print     (const node*, context&);

        void  open_brackets(context&);
        void close_brackets(context&);
        void put_value(context&, palette_color color, poly_value value);
        void put_uint(palette_color color, uint value, context&);
        void put_text(palette_color color, const array_view<char>& text, context&);
        void put_text_in_brackets(palette_color color, const array_view<char>& text, context&);


        palette_color color_of(poly_value value);

        void display_node(const node *node, context& ctx) {
            if_ref(n, node); else return;
            display_node(n, ctx);
        }

        void display_node(const node& node, context& ctx) {
            using_display_ctx;

            if (is_str_literal(node) && str_inlays); else open_brackets(ctx);
            switch (node.type) {
                case literal: { put_value(ctx, color_of(node.value), node.value); break; }
                case list: {
                    if_ref(first_child, node.first_child); else { put_text(inlays, view_of(" "), ctx); break; }
                    if_ref( last_child, node. last_child); else { dbg_fail_break; }

                    put_text(inlays, first_child.prefix, ctx);

                    if(!display_func(first_child, ctx)) {
                        str_inlays = true;
                        display_list(first_child, ctx);
                        break;
                    }

                    put_text(inlays, last_child.suffix, ctx);

                    break;
                }
            }

            if (is_str_literal(node) && str_inlays); else close_brackets(ctx);
        }

        void display_list(const node& node, context& ctx) {
            display_node(node, ctx);

            if (node.next) {
                put_text(inlays, node.suffix, ctx);
                display_list(node.next, ctx);
            }
        }

        void display_list(const node* node_list, context& ctx) {
            using_display_ctx;

            if_ref(node, node_list); else return;
            display_list(node, ctx);
        }

        bool display_func(const node& node, context& ctx) {
            using_display_ctx;
            if_var1(fn_id, get_int(node)); else return false;
            let args = node.next;

            if (fn_id == (uint)inactive     ) { display_inactive     (args, ctx); return true; }
            if (fn_id == (uint)macro_show_as) { display_macro_show_as(args, ctx); return true; }
            if (fn_id == (uint)decl_var     ) { display_decl_var     (args, ctx); return true; }
            if (fn_id == (uint)decl_param   ) { display_decl_param   (args, ctx); return true; }
            if (fn_id == (uint)var_ref      ) { display_var_ref      (args, ctx); return true; }
            if (fn_id == (uint)istring      ) { display_istring      (args, ctx); return true; }
            if (fn_id == (uint)op_assign    ) { display_op_assign    (args, ctx); return true; }
            if (fn_id == (uint)op_plus      ) { display_op_plus      (args, ctx); return true; }
            if (fn_id == (uint)fn_print     ) { display_fn_print     (args, ctx); return true; }
            return false;
        }

        void display_inactive(const node* args, context& ctx) {
            if_ref(args_list, args); else return;

            put_text(inlays, args_list.prefix, ctx);

            ctx.inactive_level += 1;
            display_list(args, ctx);
            ctx.inactive_level -= 1;
        }

        void display_macro_show_as(const node* args, context& ctx) {
            using_display_ctx;

            if_ref (id_node, args       ); else { dbg_fail_return; }
            if_var1(id, get_int(id_node)); else { dbg_fail_return; }

            put_text(definitions, view_of("show"), ctx);
            put_text(inlays, id_node.prefix, ctx);
            put_uint(constants, id, ctx);
            put_text(inlays, id_node.suffix, ctx);
            put_text(definitions, view_of("as "), ctx);

            let pattern_node = id_node.next;

            str_inlays = true;
            display_node(pattern_node, ctx);
        }

        void display_decl_var(const node* args, context& ctx) {
            using_display_ctx;

            if_var3(id_node, mut_node, name_node, deref_list3(args)); else { dbg_fail_return; }
            if_var1(name, get_str(name_node)); else { dbg_fail_return; }

            if_var1(mut, get_int(mut_node)); else { dbg_fail_return; }
            put_text(regulars, view_of(mut ? "var" : "let"), ctx);
            put_text(inlays, name_node.prefix, ctx);
            put_text_in_brackets(identifiers, name, ctx);
            put_text(inlays, name_node.suffix, ctx);

            if_ref(init_node, name_node.next); else return;
            put_text(regulars, view_of("= "), ctx);
            str_inlays = true;
            display_node(init_node, ctx);
        }

        void display_decl_param(const node* args, context& ctx) {
            using_display_ctx;

            if_var2(id_node, name_node, deref_list2(args)); else { dbg_fail_return; }
            if_var1(name, get_str(name_node)); else { name = view_of(" "); assert(false);  }
            put_text_in_brackets(identifiers, name, ctx);
        }

        void display_var_ref(const node* args, context& ctx) {
            using_display_ctx;

            if_ref (id_node, args            ); else { dbg_fail_return; }
            if_var1(id, get_int(id_node)     ); else { dbg_fail_return; }
            if_var1(name, find_name(defs, id)); else { put_text(inlays, view_of(" "), ctx); dbg_fail_return; }

            put_text_in_brackets(identifiers, name, ctx);
        }

        void display_istring(const node* args, context& ctx) {
            ctx.show_top_level_inlays_for_strings = false;
            display_list(args, ctx);
        }

        void display_op_assign(const node* args, context& ctx) {
            using_display_ctx;

            if_var2(id_node, value_node, deref_list2(args)); else { dbg_fail_return; }

            if_var1(id  , get_int (id_node )); else { dbg_fail_return; }
            if_var1(name, find_name(defs, id)); else { put_text(inlays, view_of(" "), ctx); dbg_fail_return; }

            str_inlays = true;
            put_text(identifiers, name, ctx);
            put_text(regulars, view_of(" = "), ctx);
            display_node(&value_node, ctx);
        }

        void display_op_plus(const node* args, context& ctx) {
            using_display_ctx;

            if_var2(a_node, b_node, deref_list2(args)); else { dbg_fail_return; }
            str_inlays = true;
            display_node(a_node, ctx);
            put_text(regulars, view_of(" + "), ctx);
            display_node(b_node, ctx);
        }

        void display_fn_print(const node* args, context& ctx) {
            if_ref(text_node, args); else return;

            using_display_ctx;
            put_text(regulars, view_of("print "), ctx);
            str_inlays = true;
            display_node(text_node, ctx);
        }

        void open_brackets(context& ctx) {
            set_inlay(ctx.iterator, open);
        }

        void close_brackets(context& ctx) {
            set_inlay_prev(ctx.iterator, close);
        }

        void put_value(context& ctx, palette_color color, poly_value value) {
            switch (value.type) {
                case integer: { put_uint(color, value.integer, ctx); break; }
                case string : { put_text(color, value.string , ctx); break; }
                default: { dbg_fail_return; }
            }
        }

        void put_uint(palette_color color, uint value, context& ctx) {
            put_uint(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, value);
        }

        void put_text(palette_color color, const array_view<char>& text, context& ctx) {
            put_text(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, text);
        }

        void put_text_in_brackets(palette_color color, const array_view<char>& text, context& ctx) {
            put_text_in_brackets(ctx.iterator, ctx.inactive_level > 0 ? inlays : color, text);
        }

        palette_color color_of(poly_value value) {
            switch (value.type) {
                case integer: return constants;
                case string : return strings;
            }
            dbg_fail_return regulars; // unreachable
        }
    }
}

#endif //FRANCA2_COMPUTE_VIEW_GEN_H
#pragma clang diagnostic pop