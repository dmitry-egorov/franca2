#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma clang diagnostic ignored "-Wunused-function"

#ifndef FRANCA2_COMPUTE_PARSER_H
#define FRANCA2_COMPUTE_PARSER_H

#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/arrays.h"
#include "utility/strings.h"
#include "utility/iterators.h"
#include "utility/parsing.h"
#include "out/utility/files.h"

#include "asts.h"

namespace compute_asts {
    static bool parse_file (cstr path, ast&);
    static void parse_files(arr_view<cstr> paths, ast&);
    static auto parse_code (string code, ast&) -> bool;

    namespace parser {
        using namespace iterators;
        using namespace parsing;

        auto parse_file_chain(cstr path, ast&) -> ret2<node*, node*>;
        auto parse_chain(string&, string file_path, ast&) -> ret2<node*, node*>;
        auto parse_node (string&, string file_path, ast&) -> node*;

        void set_parent_to_chain(node* first_child, node* parent);

        auto take_whitespaces_and_comments(string&) -> string;
    }

    static auto parse_file(cstr path, ast& ast) -> bool {
        if(parse_code(files::read_file_as_string(path, ast.data_arena), ast)); else {
            printf("Parsing %s failed\n", path);
            return false;
        }
        return true;
    }

    static void parse_files(arr_view<cstr> paths, ast& ast) {
        var first_node = (node*)nullptr;
        var  last_node = (node*)nullptr;
        for(var i = 0u; i < paths.count; ++i) {
            let path = paths[i];
            if_var2(first, last, parser::parse_file_chain(path, ast)); else { printf("Parsing %s failed\n", paths[i]); }

            if (!first_node) first_node       = first;
            if (  last_node)  last_node->next = first;
            last_node = last;
        }

        ast.root = first_node;
    }

    static bool parse_code(string code, ast& ast) {
        using namespace parser;

        var iterator = code;
        if_var2(first_child, last_child, parse_chain(iterator, view(""), ast)); else { dbg_fail_return false;}
        if(is_empty(iterator)); else { dbg_fail_return false; }

        ast.root = first_child;

        return true;
    }

    namespace parser {
        ret2<node*, node*> parse_file_chain(cstr path, ast& ast) {
            var code = files::read_file_as_string(path, ast.data_arena);
            var path_str = make_string(ast.data_arena, "%s", path);
            return parser::parse_chain(code, path_str, ast);
        }

        ret2<node*, node*> parse_chain(string& it, string file_path, ast& ast) {
            var first_child = (node*)nullptr;
            var  last_child = (node*)nullptr;

            var prefix = take_whitespaces_and_comments(it);

            while(!is_empty(it) && !peek(it, ']')) {
                if_ref(node, parse_node(it, file_path, ast)); else break;

                node.prefix = prefix;
                prefix = node.suffix;

                if (last_child) last_child->next = &node;
                else            first_child = &node;

                last_child = &node;
            }

            return ret2_ok(first_child, last_child);
        }

        node* parse_node(string& it, string file_path, ast& ast) {
            var text = string {};
            var text_is_quoted = true;
            if_set1(text, take_str(it)); else {
                text = take_until_any(it, view(" \t\n\r[]\""));
                text_is_quoted = false;
            }

            var text_copy = text;
            var [ int_value, can_be_int]  = take_int (text_copy);
            text_copy = text;
            var [uint_value, can_be_uint] = take_uint(text_copy);
            text_copy = text;
            var [float_value, can_be_float] = take_float(text_copy);

            // TODO: record line and column
            // a clean way to do that is to make a separate iterator
            ref result = make_node(ast, node {
                .text           = text,
                .text_is_quoted = text_is_quoted,
                .can_be_int     = can_be_int,
                .can_be_uint    = can_be_uint,
                .can_be_float   = can_be_float,
                .file_path      = file_path
            });

            if (can_be_int  ) result.  int_value =   int_value;
            if (can_be_uint ) result. uint_value =  uint_value;
            if (can_be_float) result.float_value = float_value;

            let next_text = take_whitespaces_and_comments(it);

            if(take(it, '[')) {
                if_var2(first_child, last_child, parse_chain(it, file_path, ast)); else { dbg_fail_return nullptr; }
                if(take(it, ']')); else { dbg_fail_return nullptr; } //TODO: free nodes?
                set_parent_to_chain(first_child, &result);
                result.type = node::type_t::func;
                result.first_child = first_child;
                result. last_child =  last_child;
                result.infix  = next_text;
                result.suffix = take_whitespaces_and_comments(it);
            }
            else {
                result.type   = node::type_t::literal;
                result.suffix = next_text;
            }

            return &result;
        }

        ret1<string> take_line_comment(string& it) {
            var result = string { it.data, 0 };

            static let line_comment_prefix = view("//");
            if(take(it, line_comment_prefix)); else return ret1_fail;

            let line_ends = view("\r\n");
            var text = take_until_any(it, line_ends);

            result.count = line_comment_prefix.count + text.count;
            return ret1_ok(result);
        }

        string take_whitespaces_and_comments(string& it) {
            var result = string { it.data, 0 };

            while (true) {
                var ws = take_whitespaces(it);
                result.count += ws.count;

                var [comment, ok] = take_line_comment(it); // skip line comments
                result.count += comment.count;

                if(ws.count || ok); else break;
            }

            return result;
        }

        void set_parent_to_chain(node* first_child, node* parent) {
            var child = first_child;
            while (child) {
                child->parent = parent;
                child = child->next;
            }
        }
    }
}
#endif //FRANCA2_COMPUTE_PARSER_H

#pragma clang diagnostic pop