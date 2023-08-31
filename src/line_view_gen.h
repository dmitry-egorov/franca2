#ifndef FRANCA2_LINE_NUM_VIEW_GENERATOR_H
#define FRANCA2_LINE_NUM_VIEW_GENERATOR_H

#include <algorithm>

#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/maths2.h"
#include "utility/results.h"
#include "utility/arrays.h"
#include "utility/iterators.h"


namespace line_view_gen {
    namespace {
        using namespace arrays;
        using namespace iterators;
    }

    struct line_view {
        arr_view <uint8_t> glyphs;
        uint width;
    };

    static ret1<line_view> generate_line_view(uint line_count, usize2 view_size, arena &arena = arenas::gta) {
        let max_width = view_size.w - 2;
        var height    = std::min(line_count, view_size.h);

        var width     = 1u;
        var max_order = 1u;

        while (max_order * 10 < height && width < max_width) {
            max_order *= 10;
            width++;
        }

        var glyphs = alloc<uint8_t>(arena, view_size.w * view_size.h);

        for (var y = 0u; y < height; y++) {
            var trailing = true;
            var order = max_order;
            for (var x = 0u; x < width ; x++, order /= 10) {
                let digit = ((y + 1) / order) % 10;
                if (!trailing || digit != 0) {
                    trailing = false;
                    glyphs[y * view_size.w + x + 1] = '0' + digit - ' ';
                }
            }
        }

        let result = line_view { .glyphs = glyphs, .width = width + 2 };
        return ret1_ok(result);
    }
}

#endif //FRANCA2_LINE_NUM_VIEW_GENERATOR_H
