#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

#ifndef FRANCA2_CODE_VIEWS_H
#define FRANCA2_CODE_VIEWS_H

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
        str_dyn builder;
        uint2 cell_idx;
        uint indent;
    };

#define using_cv_iterator(code_view_iterator) ref [view, builder, cell_idx, indent] = code_view_iterator; ref [arena, glyphs, colors, inlays, size, line_count] = view; ref [x, y] = cell_idx
#define tmp_indent(it) it.indent += 1; defer { it.indent -= 1; }

    inline code_view make_code_vew(const usize2& size, arena& arena = arenas::gta) { return {
        .scratch_arena = arena,
        .glyphs = alloc<u8>(arena, size.w * size.h),
        .colors = alloc<u8>(arena, size.w * size.h),
        .inlays = alloc<u8>(arena, size.w * size.h),
        .size = size
    };}

    inline code_view_iterator iterate(code_view& cv) { return { .view = cv, .builder = make_string_builder(1024, cv.scratch_arena) }; }

    inline bool finished (const code_view_iterator& it) { using_cv_iterator(it); return y >= size.h; }
    inline void next_line(code_view_iterator &it) {
        using_cv_iterator(it);
        if (finished(it)) return;

        x  = indent * 4; //TODO: make the indent size configurable configurable
        y += 1;
    }

    inline ret1<bool> next_cell(code_view_iterator &it) {
        using_cv_iterator(it);
        if (finished(it)) return ret1_ok(false);

        x += 1;
        let need_next_line = x >= size.w;
        if (need_next_line) next_line(it);
        return { need_next_line, !finished(it) };
    }

    void put_uint      (code_view_iterator &it, palette_color color, uint value);
    template<typename... Args> void put_text(code_view_iterator &it, palette_color color, cstr format, Args... args);
    void put_text      (code_view_iterator &it, palette_color color, const string& text);
    void set_glyph     (code_view_iterator &it, u8 glyph);
    void set_color     (code_view_iterator &it, palette_color color);
    void set_inlay     (code_view_iterator &it, inlay_type inlay);
    void set_inlay_prev(code_view_iterator &it, inlay_type inlay);

    inline u8* cell_at(arr_view<u8>& view, const uint2& idx, const code_view& cv) {
        if (idx.x < cv.size.w && idx.y < cv.size.h); else return nullptr;
        return &view[idx.y * cv.size.w + idx.x];
    }

    inline void put_float(code_view_iterator &it, palette_color color, float value) {
        put_text(it, color, "%.2f", value);
    }

    inline void put_uint(code_view_iterator &it, palette_color color, uint value) {
        put_text(it, color, "%u", value);
    }

    inline void put_int(code_view_iterator &it, palette_color color, int value) {
        put_text(it, color, "%d", value);
    }

    template<typename... Args> void put_text(code_view_iterator &it, palette_color color, cstr format, Args... args) {
        put_text(it, color, make_string(it.view.scratch_arena, format, args...));
    }

    inline void put_text_in_brackets(code_view_iterator &it, palette_color color, const string& text) {
        using enum inlay_type;
        set_inlay(it, open);
        put_text(it, color, text);
        set_inlay_prev(it, close);
    }

    inline void put_text(code_view_iterator &it, palette_color color, const string& text) {
        var text_it = text;
        while(!is_empty(text_it)) {
            var c = take(text_it).v0;
            if (c == '\n') {
                push(it.builder, c);
                for (var i = 0u; i < it.indent; ++i)
                    push(it.builder, view("    "));
                next_line(it);
                continue;
            }

            if (c >= ' ' && c <= '~'); else continue;

            push(it.builder, c);
            set_glyph(it, c - ' ');
            set_color(it, color);

            if_var1 (skipped_line, next_cell(it)); else break;

            if (skipped_line) {
                let s = take_past(text_it, '\n');
                push(it.builder, s);
            }
        }
    }

    inline void set_glyph(code_view_iterator &it, u8 glyph) {
        using_cv_iterator(it);
        if_ref(cell, cell_at(glyphs, cell_idx, view)); else return;
        cell = glyph;
    }

    inline void set_color(code_view_iterator &it, palette_color color) {
        using_cv_iterator(it);
        if_ref(cell, cell_at(colors, cell_idx, view)); else return;
        cell = (u8)color;
    }

    inline void set_inlay(code_view_iterator &it, inlay_type inlay) {
        using_cv_iterator(it);
        if_ref(cell, cell_at(inlays, cell_idx, view)); else return;
        cell |= (u8)inlay;
    }

    inline void set_inlay_prev(code_view_iterator &it, inlay_type inlay) {
        using_cv_iterator(it);
        var idx = cell_idx;
        idx.x -= 1;

        if_ref(cell, cell_at(inlays, idx, view)); else return;
        cell |= (u8)inlay;
    }
}


#endif //FRANCA2_CODE_VIEWS_H

#pragma clang diagnostic pop