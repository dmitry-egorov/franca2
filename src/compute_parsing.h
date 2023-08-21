#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_PARSER_H
#define FRANCA2_COMPUTE_PARSER_H

#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/arrays.h"
#include "utility/iterators.h"
#include "utility/parsing.h"

#include "compute_asts.h"

namespace compute_asts {
    static ret1<ast> parse_file(const char* path);
    static ret1<ast> parse_text(const arrays::array_view<char>& text, ast_storage& storage);

    namespace parser {
        using namespace iterators;
        using namespace parsing;
        using namespace arenas;
        using namespace strings;
        using enum node::type_t;

        ret1<node*> parse_list         (array_view<char>&, ast_storage&);
        ret2<node*, node*> parse_list_items(array_view<char>&, ast_storage&);
        ret1<node*> parse_integer      (array_view<char>&, ast_storage&);
        ret1<node*> parse_pattern      (array_view<char>&, ast_storage&);
        ret1<node*> parse_pattern_part (array_view<char>&, ast_storage&);
        ret2<node*, node*> parse_pattern_parts(array_view<char>&, ast_storage&);

        void set_parent_to_chain(node* first_child, node* parent);

        array_view<char> take_whitespaces_and_comments(array_view<char>&);
    }


    static ret1<ast> parse_file(const char* path) {
        var storage = ast_storage {
            arenas::make(1024 * sizeof(node)),
            arenas::make(1024 * 1024 * sizeof(char))
        };

        if_var1(ast, parse_text(read_file_as_string(path, storage.text_arena), storage)); else {
            printf("Parsing %s failed\n", path);
            release(storage);
            return ret1_fail;
        }
        return ret1_ok(ast);
    }

    static ret1<ast> parse_text(const arrays::array_view<char>& text, ast_storage& storage) {
        using namespace parser;

        var iterator = text;
        if_var2(first_child, last_child, parse_list_items(iterator, storage)); else return ret1_fail;
        take(iterator, '\0');
        if(is_empty(iterator)); else return ret1_fail;

        let a = ast { first_child, storage };
        return ret1_ok(a);
    }

    namespace parser {
        ret1<node*> parse_list(array_view<char>& it, ast_storage &storage) {
            if(take(it, '[')); else return ret1_fail;
            if_var2(first_child, last_child, parse_list_items(it, storage)); else return ret1_fail;
            if(take(it, ']')); else return ret1_fail; //TODO: free nodes?

            var result = make_list_node(storage, first_child, last_child);

            set_parent_to_chain(first_child, result);

            return ret1_ok(result);
        }

        ret2<node*, node*> parse_list_items(array_view<char>& it, ast_storage & storage) {
            var first_child = (node*)nullptr;
            var  last_child = (node*)nullptr;

            var prefix = take_whitespaces_and_comments(it); // NOTE: we lose prefix whitespaces here

            while(!is_empty(it)) {
                if(!is_empty(it)); else break;

                node *next_node;
                {if_set1(next_node, parse_integer(it, storage)); else
                {if_set1(next_node, parse_pattern(it, storage)); else
                {if_set1(next_node, parse_list   (it, storage)); else
                    break; }}}

                next_node->prefix = prefix;
                next_node->suffix = prefix = take_whitespaces_and_comments(it);

                if (last_child) last_child->next = next_node;
                else            first_child = next_node;

                last_child = next_node;
            }

            return ret2_ok(first_child, last_child);
        }

        ret1<node*> parse_integer(array_view<char>& it, ast_storage &storage) {
            if_var1(result, take_integer(it)); else return ret1_fail;
            return ret1_ok(make_literal_node(storage, result));
        }

        ret1<node*> parse_pattern(array_view<char>& it, ast_storage & storage) {
            if     (take(it, '\"')); else return ret1_fail;
            if_var2(first_child, last_child, parse_pattern_parts(it, storage)); else return ret1_fail;
            if     (take(it, '\"')); else return ret1_fail;

            if (!first_child) {
                return ret1_ok(make_literal_node(storage, ""));
            }

            if (!first_child->next && is_str_literal(first_child)) {
                return ret1_ok(first_child);
            }

            var func_id_node = make_literal_node(storage, (uint)builtin_func_id::istring, first_child);
            var result = make_list_node(storage, func_id_node, last_child);
            set_parent_to_chain(first_child, result);

            return ret1_ok(result);
        }

        ret2<node*, node*> parse_pattern_parts(array_view<char>& it, ast_storage& storage){
            var first_child = (node*)nullptr;
            var last_child  = (node*)nullptr;

            while(!is_empty(it)) {
                node *next_node;
                {if_set1(next_node, parse_list        (it, storage)); else
                {if_set1(next_node, parse_pattern_part(it, storage)); else
                    return ret2_fail;}}

                if (last_child) last_child->next = next_node;
                else            first_child = next_node;
                last_child = next_node;

                if_var1(c, peek(it)); else return ret2_fail;
                     if(c != '\"'  ); else break;
            }

            return ret2_ok(first_child, last_child);
        }

        ret1<node*> parse_pattern_part(array_view<char>& it, ast_storage & storage) {
            static let ends = view_of("[\"");
            var data = take_until_any(it, ends);

            //TODO: handle escape sequences
            return ret1_ok(make_literal_node(storage, data));
        }

        ret1<array_view<char>> take_line_comment(array_view<char>& it) {
            return ret1_fail; // disable comments for now, use disabled node instead

            var result = array_view<char> { it.data, 0 };

            static let line_comment_prefix = view_of("//");
            if(take(it, line_comment_prefix)); else return ret1_fail;

            let line_ends = view_of("\r\n");
            var text = take_until_any(it, line_ends);

            result.count = line_comment_prefix.count + text.count;
            return ret1_ok(result);
        }

        array_view<char> take_whitespaces_and_comments(array_view<char>& it) {
            var result = array_view<char> { it.data, 0 };

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