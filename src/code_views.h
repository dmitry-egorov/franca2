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

    enum class inlay_type {
        none  = 0,
        open  = 1,
        close = 2,
        both  = 3
    };

    struct code_view {
        arena& scratch_arena;
        arr_view<u8> glyphs;
        arr_view<u8> colors;
        arr_view<u8> inlays;

        usize2 size;
        uint   line_count;
    };

    struct code_view_iterator {
        code_view& view;
        uint2 cell_idx;
        uint indent;
    };

#define using_cv_iterator(code_view_iterator) ref [view, cell_idx, indent] = code_view_iterator; ref [arena, glyphs, colors, inlays, size, line_count] = view; ref [x, y] = cell_idx
#define tmp_indent(it) it.indent += 1; defer { it.indent -= 1; }
    code_view make_code_vew(const usize2& size, arena& arena = arenas::gta) { return {
        .scratch_arena = arena,
        .glyphs = alloc<u8>(arena, size.w * size.h),
        .colors = alloc<u8>(arena, size.w * size.h),
        .inlays = alloc<u8>(arena, size.w * size.h),
        .size = size
    };}

    code_view_iterator iterate(code_view& cv) { return { .view = cv }; }

    bool finished (const code_view_iterator& it) { using_cv_iterator(it); return y >= size.h; }
    void next_line(code_view_iterator &it) {
        using_cv_iterator(it);
        x  = indent * 4; //TODO: make the indent size configurable configurable
        y += 1;
    }

    ret1<bool> next_cell(code_view_iterator &it) {
        using_cv_iterator(it);
        if (finished(it)) return ret1_ok(false);

        x += 1;
        let need_next_line = x >= size.w;
        if (need_next_line) next_line(it);
        return { need_next_line, !finished(it) };
    }

    void put_uint      (code_view_iterator &it, palette_color color, uint value);
    template<typename... Args> void put_text(code_view_iterator &it, palette_color color, const char* format, Args... args);
    void put_text      (code_view_iterator &it, palette_color color, const string& text);
    void set_glyph     (code_view_iterator &it, u8 glyph);
    void set_color     (code_view_iterator &it, palette_color color);
    void set_inlay     (code_view_iterator &it, inlay_type inlay);
    void set_inlay_prev(code_view_iterator &it, inlay_type inlay);

    u8* cell_at(arr_view<u8>& view, const uint2& idx, const code_view& cv) {
        if (idx.x >= 0 && idx.x < cv.size.w && idx.y >= 0 && idx.y < cv.size.h); else return nullptr;
        return &view[idx.y * cv.size.w + idx.x];
    }

    void put_uint(code_view_iterator &it, palette_color color, uint value) {
        put_text(it, color, "%d", value);
    }

    template<typename... Args> void put_text(code_view_iterator &it, palette_color color, const char* format, Args... args) {
        put_text(it, color, make_string(it.view.scratch_arena, format, args...));
    }

    void put_text_in_brackets(code_view_iterator &it, palette_color color, const string& text) {
        using enum inlay_type;
        set_inlay(it, open);
        put_text(it, color, text);
        set_inlay_prev(it, close);
    }

    void put_text(code_view_iterator &it, palette_color color, const string& text) {
        var text_it = text;
        while(!is_empty(text_it) && !finished(it)) {
            if_var1 (c, take(text_it)); else break;
            if (c == '\n') {
                next_line(it);
                continue;
            }

            if (c >= ' ' && c <= '~'); else continue;

            set_glyph(it, c - ' ');
            set_color(it, color);

            if_var1 (skipped_line, next_cell(it)); else break;

            if (skipped_line) {
                if(skip_past(text_it, '\n')); else break;
            }
        }
    }

    void set_glyph(code_view_iterator &it, u8 glyph) {
        using_cv_iterator(it);
        if_ref(cell, cell_at(glyphs, cell_idx, view)); else return;
        cell = glyph;
    }

    void set_color(code_view_iterator &it, palette_color color) {
        using_cv_iterator(it);
        if_ref(cell, cell_at(colors, cell_idx, view)); else return;
        cell = (u8)color;
    }

    void set_inlay(code_view_iterator &it, inlay_type inlay) {
        using_cv_iterator(it);
        if_ref(cell, cell_at(inlays, cell_idx, view)); else return;
        cell |= (u8)inlay;
    }

    void set_inlay_prev(code_view_iterator &it, inlay_type inlay) {
        using_cv_iterator(it);
        var idx = cell_idx;
        idx.x -= 1;

        if_ref(cell, cell_at(inlays, idx, view)); else return;
        cell |= (u8)inlay;
    }
}


#endif //FRANCA2_CODE_VIEWS_H

#pragma clang diagnostic pop