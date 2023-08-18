#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_AST_GENERATION_H
#define FRANCA2_AST_GENERATION_H

#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/strings.h"
#include "utility/arenas.h"
#include "utility/iterators.h"
#include "utility/parsing.h"
#include "utility/files.h"
#include "visual_asts.h"

namespace visual_asts::parser {
    using namespace iterators;
    using namespace parsing;
    using namespace visual_asts;
    using namespace arenas;

    namespace {
        node* parse_expression(array_view<char>& it, color_scheme color, ast_storage& storage);
        void  parse_prefix    (array_view<char>& it, node& node);
        bool  parse_child     (array_view<char>& it, node& node, ast_storage& storage);
        void  parse_sibling   (array_view<char>& it, node& node, ast_storage& storage);
    }

    static ret1<ast> parse_text(const array_view<char>& text, ast_storage& storage) {
        var iterator = text;
        var root = parse_expression(iterator, color_scheme::regulars, storage);

        let a = visual_asts::ast {root, storage };
        return ret1_ok(a);
    }

    static ret1<ast> parse_file(const char* path) {
        var storage = ast_storage {
            arenas::make(1024 * sizeof(node)),
            arenas::make(1024 * 1024 * sizeof(char))
        };

        chk_var1(ast, parse_text(read_file_as_string(path, storage.text_arena), storage)) else { release(storage); return ret1_fail; }
        return ret1_ok(ast);
    }

    namespace {
        node* parse_expression(array_view<char>& it, color_scheme color, ast_storage& storage) {
            ref node = *make_node(storage, {});
            node.color_id = color;

                parse_prefix (it, node);
            chk(parse_child  (it, node, storage)) else return &node;
                parse_sibling(it, node, storage);

            return &node;
        }

        void parse_prefix(array_view<char>& it, node& node) {
            static let ends = view_of("[]");
            node.prefix_text = take_until_any(it, ends);
        }

        bool parse_child(array_view<char>& it, node& node, ast_storage& storage) {
            chk(take(it, '[')) else return false;
            let [d, d_ok]     = take_integer(it);
            if (d_ok) skip_whitespaces(it);
            node.first_child = parse_expression(it, d_ok ? (color_scheme)d : color_scheme::regulars, storage);
            node.first_child->parent = &node;
            chk(take(it, ']')) else return false;
            return true;
        }

        void parse_sibling(array_view<char>& it, node& node, ast_storage& storage) {
            node.next_sibling = parse_expression(it, node.color_id, storage);
        }
    }
}
#endif //FRANCA2_AST_GENERATION_H

#pragma clang diagnostic pop