#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

#ifndef FRANCA2_CODE_VIEWS_H
#define FRANCA2_CODE_VIEWS_H
#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/iterators.h"
#include "utility/strings.h"

namespace code_views {
    using namespace iterators;
    using namespace strings;

    struct code_view {
        arena& scratch_arena;
        array_view<uint8_t> glyphs;
        array_view<uint8_t> colors;
        array_view<uint8_t> inlays;

        usize2 size;
        uint   line_count;
    };

    struct code_view_iterator {
        code_view& view;
        uint2 cell_idx;
    };

#define using_cv_iterator(code_view_iterator) ref [view, cell_idx] = code_view_iterator; ref [arena, glyphs, colors, inlays, size, line_count] = view; ref [x, y] = cell_idx

    code_view make_code_vew(const usize2& size, arena& arena = arenas::gta) { return {
        .scratch_arena = arena,
        .glyphs = alloc<uint8_t>(arena, size.w * size.h),
        .colors = alloc<uint8_t>(arena, size.w * size.h),
        .inlays = alloc<uint8_t>(arena, size.w * size.h),
        .size = size
    };}

    code_view_iterator iterate(code_view& cv) { return { .view = cv }; }


    bool finished (const code_view_iterator& it) { using_cv_iterator(it); return y >= size.h; }
    void next_line(code_view_iterator &it) { using_cv_iterator(it); x = 0; y += 1; }
    ret1<bool> next_cell(code_view_iterator &it) {
        using_cv_iterator(it);
        if (finished(it)) return ret1_ok(false);

        x += 1;
        let need_next_line = x >= size.w;
        if (need_next_line) next_line(it);
        return { need_next_line, !finished(it) };
    }

    void put_uint      (code_view_iterator &it, color_scheme color, uint value);
    template<typename... Args> void put_text(code_view_iterator &it, color_scheme color, const char* format, Args... args);
    void put_text      (code_view_iterator &it, color_scheme color, const array_view<char>& text);
    void set_glyph     (code_view_iterator &it, u8 glyph) { using_cv_iterator(it); glyphs[y * size.w + x] = glyph; }
    void set_color     (code_view_iterator &it, color_scheme color) { using_cv_iterator(it); colors[y * size.w + x] = (uint8_t)color; }
    void set_inlay     (code_view_iterator &it, u8 inlay) { using_cv_iterator(it); inlays[y * size.w + x] |= inlay; }
    void set_inlay_prev(code_view_iterator &it, u8 inlay) { using_cv_iterator(it); inlays[y * size.w + x - 1] |= inlay; }


    void put_uint(code_view_iterator &it, color_scheme color, uint value) {
        put_text(it, color, "%d", value);
    }

    template<typename... Args> void put_text(code_view_iterator &it, color_scheme color, const char* format, Args... args) {
        put_text(it, color, make_string(it.view.scratch_arena, format, args...));
    }

    void put_text(code_view_iterator &it, color_scheme color, const array_view<char>& text) {
        var text_it = text;
        while(!is_empty(text_it) && !finished(it)) {
            chk_var1(c, take(text_it)) else break;
            if (c == '\n') {
                next_line(it);
                continue;
            }

            if (c >= ' ' && c <= '~'); else continue;

            set_glyph(it, c - ' ');
            set_color(it, color);

            chk_var1(skipped_line, next_cell(it)) else break;

            if (skipped_line) {
                chk(skip_past(text_it, '\n')) else break;
            }
        }
    }
}


#endif //FRANCA2_CODE_VIEWS_H

#pragma clang diagnostic pop