#ifndef FRANCA2_COLOR_PALETTE_H
#define FRANCA2_COLOR_PALETTE_H

#include <array>

#include "utility/syntax.h"
#include "utility/primitives.h"

enum struct palette_color {
    regulars         = 0,
    identifiers      = 1,
    break_statements = 2,
    controls         = 3,
    definitions      = 4,
    implicit_vars    = 5,
    strings          = 6,
    constants        = 7,
    inlays           = 8,
};

inline var palette = std::array<uint, 256>();

void set_palette_color(palette_color color, uint value) { palette[(size_t)color] = value; }

void init_palette() {
    using enum palette_color;

    set_palette_color(regulars         , 0xa7a7ebff);
    set_palette_color(identifiers      , 0xffffffff);
    set_palette_color(break_statements , 0x416ecaff);
    set_palette_color(controls, 0x3393ffff);
    set_palette_color(definitions      , 0xae76c0ff);
    set_palette_color(implicit_vars    , 0xbbbbbbff);
    set_palette_color(strings          , 0x6aab73ff);
    set_palette_color(constants        , 0xc6b88cff);
    set_palette_color(inlays           , 0xdddddd44);
}

#endif //FRANCA2_COLOR_PALETTE_H
