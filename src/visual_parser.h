#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma clang diagnostic ignored "-Wunused-function"

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
    using namespace strings;

    namespace {
        node* parse_expression(string& it, palette_color color, ast_storage& storage);
        void  parse_prefix    (string& it, node& node);
        bool  parse_child     (string& it, node& node, ast_storage& storage);
        void  parse_sibling   (string& it, node& node, ast_storage& storage);
    }

    static ret1<ast> parse_text(const string& text, ast_storage& storage) {
        var iterator = text;
        var root = parse_expression(iterator, palette_color::regulars, storage);

        let a = visual_asts::ast {root, storage };
        return ret1_ok(a);
    }

    static ret1<ast> parse_file(const char* path) {
        var storage = ast_storage {
            arenas::make(1024 * sizeof(node)),
            arenas::make(1024 * 1024 * sizeof(char))
        };

        if_var1(ast, parse_text(read_file_as_string(path, storage.text_arena), storage)); else { release(storage); return ret1_fail; }
        return ret1_ok(ast);
    }

    namespace {
        node* parse_expression(string& it, palette_color color, ast_storage& storage) {
            ref node = *make_node(storage, {});
            node.color_id = color;

                parse_prefix (it, node);
            if (parse_child  (it, node, storage)); else return &node;
                parse_sibling(it, node, storage);

            return &node;
        }

        void parse_prefix(string& it, node& node) {
            static let ends = view_of("[]");
            node.prefix_text = take_until_any(it, ends);
        }

        bool parse_child(string& it, node& node, ast_storage& storage) {
            if (take(it, '[')); else return false;
            let [d, d_ok]     = take_uint(it);
            if (d_ok) skip_whitespaces(it);
            node.first_child = parse_expression(it, d_ok ? (palette_color)d : palette_color::regulars, storage);
            node.first_child->parent = &node;
            if (take(it, ']')); else return false;
            return true;
        }

        void parse_sibling(string& it, node& node, ast_storage& storage) {
            node.next_sibling = parse_expression(it, node.color_id, storage);
        }
    }
}
#endif //FRANCA2_AST_GENERATION_H

#pragma clang diagnostic pop