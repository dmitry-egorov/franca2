#ifndef FRANCA2_WEB_TEST_PARSER_H
#define FRANCA2_WEB_TEST_PARSER_H
#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/iterators.h"

namespace code_view_generation {
    namespace {
        using namespace iterators;
    }

    struct code_view {
        array_view<uint8_t> glyphs;
        array_view<uint8_t> colors;
        array_view<uint8_t> inlays;
		uint line_count;
    };

    static ret1<code_view> generate_code_view(array_view<char> text, usize2 size, arena& arena = arenas::gta) {
        var glyphs = alloc<uint8_t>(arena, size.w * size.h);
        var colors = alloc<uint8_t>(arena, size.w * size.h);
        var inlays = alloc<uint8_t>(arena, size.w * size.h);

        var color = (uint8_t)0;
        var x = 0u, y = 0u;

        var it = iterate(text);
        while (!finished(it) && y < size.y) {
            if (x >= size.w) {
                // skip to next line
                chk(take_until(it, '\n')) else break;
            }

            chk_1(c, take(it)) else break;
            if (c == '\n') {
                x  = 0;
                y += 1;
                continue;
            }

            if (c == '[') {
                inlays[y * size.w + x] = 1;
                continue;
            }

            if (c == ']') {
                inlays[y * size.w + x - 1] = 2;
                continue;
            }

            if (c == '{') {
                let [d, d_ok] = take_digit(it);
                color = d_ok ? d : 1;
                continue;
            }

            if (c == '}') {
                color = 0;
                continue;
            }

            if (c >= ' ' && c <= '~'); else continue;

            glyphs[y * size.w + x] = c - ' ';
            colors[y * size.w + x] = color;
            x += 1;
        }

        return ret1_ok((code_view {
			.glyphs = glyphs, 
			.colors = colors, 
			.inlays = inlays,
			.line_count = y + 1
		}));
    }

    static ret1<code_view> parse_file(const char* path, usize2 size, arena& arena = arenas::gta) {
        let text = read_file_as_string(path, arena);
        return generate_code_view(text, size, arena);
    }
}



#endif //FRANCA2_WEB_TEST_PARSER_H
