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
#include "color_scheme.h"
#include "code_views.h"
#include "compute_asts.h"
#include "compute_definitions.h"

namespace compute_asts {
    static void display(ast &ast, code_views::code_view_iterator &it, const arenas::array_view<definition> &definitions);

    namespace displaying {
        using namespace iterators;
        using namespace compute_asts;
        using namespace code_views;
        using namespace strings;

        struct context {
            code_view_iterator& iterator;
            const array_view<definition>& definitions;
            bool show_top_level_inlays_for_strings;
        };
#define using_display_ctx ref [it, defs, str_inlays] = ctx

        void display_nodes(const node*, context&);
        void display_node (const node*, context&);

        bool display_func(uint fn_id, const node *args, context&);
        void display_show_as(const node*, context&);
        void display_let    (const node*, context&);
        void display_param  (const node*, context&);
        void display_ref    (const node*, context&);
        void display_istring(const node*, context&);
        void display_plus   (const node*, context&);
        void display_print  (const node*, context&);
    }


    static void display(ast &ast, code_views::code_view_iterator &it, const arrays::array_view<definition> &definitions) {
        var ctx = displaying::context { it, definitions, true };
        display_nodes(ast.root, ctx);
    }

    namespace displaying {
        using enum node_type;

        void display_node(const node *node, context& ctx) {
            chk(node) else return;

            using_display_ctx;

            if (node->type != str_literal || str_inlays) set_inlay(ctx.iterator, 1);
            switch (node->type) {
                case int_literal: { put_uint(it, color_scheme::constants, node->int_value); break; }
                case str_literal: { put_text(it, color_scheme::strings  , node->str_value); break; }
                case list: {
                    let child = node->first_child;
                    chk(child) else { put_text(it, color_scheme::inlays, view_of(" ")); break; }

                    if (child->type == int_literal)
                        if (display_func(child->int_value, child->next_sibling, ctx))
                            break;

                    put_text(it, color_scheme::inlays, child->prefix);
                    str_inlays = true;
                    display_nodes(child, ctx);
                    break;
                }
            }

            if (node->type != str_literal || str_inlays) set_inlay_prev(it, 2);
        }

        void display_nodes(const node *node, context& ctx) {
            chk(node) else return;

            using_display_ctx;

            display_node(node, ctx);

            if (node->suffix.count > 0)
                put_text(it, color_scheme::inlays, node->suffix);

            if (node->next_sibling)
                display_nodes(node->next_sibling, ctx);
        }

        bool display_func(uint fn_id, const node *args, context& ctx) {
            using_display_ctx;

            if (fn_id ==   0) { display_show_as(args, ctx); return true; }
            if (fn_id ==   1) { display_let    (args, ctx); return true; }
            if (fn_id ==   2) { display_param  (args, ctx); return true; }
            if (fn_id ==   3) { display_ref    (args, ctx); return true; }
            if (fn_id ==   4) { display_istring(args, ctx); return true; }
            if (fn_id == 100) { display_plus   (args, ctx); return true; }
            if (fn_id == 200) { display_print  (args, ctx); return true; }

            return false;
        }

        void display_show_as(const node *args, context& ctx) {
            let id_node = args;
            assert(id_node->type == int_literal);

            using_display_ctx;

            let id = id_node->int_value;

            put_text(it, color_scheme::definitions, view_of("show "));
            put_uint(it, color_scheme::constants  , id);
            put_text(it, color_scheme::definitions, view_of(" as "));

            let pattern_node = id_node->next_sibling;

            str_inlays = true;
            display_node(pattern_node, ctx);
        }

        void display_let(const node *args, context& ctx) {
            let id_node = args;
            assert(id_node->type == int_literal);
            let name_node = id_node->next_sibling;
            assert(name_node->type == str_literal);
            let value_node = name_node->next_sibling;

            using_display_ctx;

            let name = name_node->str_value;

            put_text (it, color_scheme::regulars, view_of("let "));
            set_inlay(it, 1); put_text(it, color_scheme::identifiers, name); set_inlay_prev(it, 2);
            put_text (it, color_scheme::regulars, view_of(" = "));

            str_inlays = true;
            display_node(value_node, ctx);
        }

        void display_param(const node *arguments, context& ctx) {
            let id_node = arguments;
            assert(id_node->type == int_literal);
            let name_node = id_node->next_sibling;
            assert(name_node->type == str_literal);

            using_display_ctx;

            let name = name_node->str_value;
            set_inlay(it, 1); put_text(it, color_scheme::identifiers, name); set_inlay_prev(it, 2);
        }


        void display_ref(const node *arguments, context& ctx) {
            let id_node = arguments;
            assert(id_node->type == int_literal);

            using_display_ctx;

            let id = id_node->int_value;
            chk_var1(name, find_name(defs, id)) else { put_text(it, color_scheme::inlays, view_of(" ")); return; }

            set_inlay(it, 1); put_text(it, color_scheme::identifiers, name); set_inlay_prev(it, 2);
        }

        void display_istring(const node *arguments, context& ctx) {
            ctx.show_top_level_inlays_for_strings = false;
            display_nodes(arguments, ctx);
        }

        void display_plus(const node *arguments, context& ctx) {
            let a_node = arguments;
            let b_node = a_node->next_sibling;

            using_display_ctx;

            str_inlays = true;
            display_node(a_node, ctx);
            put_text(it, color_scheme::regulars, view_of(" + "));
            display_node(b_node, ctx);
        }

        void display_print(const node *arguments, context& ctx) {
            var text_node = arguments;

            using_display_ctx;
            put_text(it, color_scheme::regulars, view_of("print "));
            str_inlays = true;
            display_node(text_node, ctx);
        }
    }
}

#endif //FRANCA2_COMPUTE_VIEW_GEN_H
#pragma clang diagnostic pop