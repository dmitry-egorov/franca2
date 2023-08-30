#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_PARSER_H
#define FRANCA2_COMPUTE_PARSER_H

#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/arrays.h"
#include "utility/strings.h"
#include "utility/iterators.h"
#include "utility/parsing.h"

#include "compute_asts.h"

namespace compute_asts {
    static auto parse_file(const char* path) -> ret1<ast>;
    static auto parse_code(const string& code, ast_storage& storage) -> ret1<ast>;

    namespace parser {
        using namespace iterators;
        using namespace parsing;

        auto parse_chain(string&, ast_storage&) -> ret2<node*, node*>;
        auto parse_node (string&, ast_storage&) -> node*;

        void set_parent_to_chain(node* first_child, node* parent);

        auto take_whitespaces_and_comments(string&) -> string;
    }

    static auto parse_file(const char* path) -> ret1<ast> {
        var storage = ast_storage {
            arenas::make(1024 * sizeof(node)),
            arenas::make(1024 * 1024 * sizeof(char))
        };

        if_var1(ast, parse_code(read_file_as_string(path, storage.text_arena), storage)); else {
            printf("Parsing %s failed\n", path);
            release(storage);
            return ret1_fail;
        }
        return ret1_ok(ast);
    }

    static ret1<ast> parse_code(const string& code, ast_storage& storage) {
        using namespace parser;

        var iterator = code;
        if_var2(first_child, last_child, parse_chain(iterator, storage)); else { dbg_fail_return ret1_fail;}
        take(iterator, '\0');
        if(is_empty(iterator)); else { dbg_fail_return ret1_fail; }

        let a = ast { first_child, storage };
        return ret1_ok(a);
    }

    namespace parser {
        ret2<node*, node*> parse_chain(string& it, ast_storage& storage) {
            var first_child = (node*)nullptr;
            var  last_child = (node*)nullptr;

            var prefix = take_whitespaces_and_comments(it);

            while(!is_empty(it) && !peek(it, ']')) {
                if_ref(node, parse_node(it, storage)); else break;

                node.prefix = prefix;
                node.suffix = prefix = take_whitespaces_and_comments(it);

                if (last_child) last_child->next = &node;
                else            first_child = &node;

                last_child = &node;
            }

            return ret2_ok(first_child, last_child);
        }

        node* parse_node(string & it, ast_storage & storage) {
            var text = string {};
            var text_is_quoted = true;
            if_set1(text, take_str(it)); else {
                text = take_until_any(it, view(" \t\n\r[]\""));
                text_is_quoted = false;
            }

            var text_copy = text;
            var [uint_value, can_be_uint] = take_uint(text_copy);

            var result = make_node(storage, node {
                .text = text,
                .text_is_quoted = text_is_quoted,
                .can_be_uint = can_be_uint,
                .uint_value  = uint_value,
            });

            if(take(it, '[')) {
                if_var2(first_child, last_child, parse_chain(it, storage)); else { dbg_fail_return nullptr; }
                if(take(it, ']')); else { dbg_fail_return nullptr; } //TODO: free nodes?
                set_parent_to_chain(first_child, result);
                result->type = node::type_t::func;
                result->first_child = first_child;
                result-> last_child =  last_child;
            }
            else {
                result->type = node::type_t::literal;
            }

            return result;
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