#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_TEST_PARSER_H
#define FRANCA2_TEST_PARSER_H
#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/iterators.h"
#include "code_views.h"
#include "visual_asts.h"

namespace visual_asts::view_gen {
    using namespace iterators;
    using namespace visual_asts;
    using namespace code_views;

    void display(const node& node, code_view_iterator& it);

    void display(ast& ast, code_view_iterator& it) {
        display(*ast.root, it);
    }

    void display(const node& node, code_view_iterator& it) {
        put_text(it, node.color_id, node.prefix_text);

        var child = node.first_child;
        if (child) {
            set_inlay(it, 1);
            display(*child, it);
            if (child->prefix_text.count == 0 && child->first_child == nullptr)
                set_inlay(it, 2);
            else
                set_inlay_prev(it, 2);
        }

        if (node.next_sibling)
            display(*node.next_sibling, it);
    }
}

#endif //FRANCA2_TEST_PARSER_H

#pragma clang diagnostic pop