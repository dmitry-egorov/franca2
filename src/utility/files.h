#ifndef FRANCA2_FILES_H
#define FRANCA2_FILES_H
#include <cstdio>
#include "syntax.h"
#include "arenas.h"

void release(FILE*& f) {
    if (f) fclose(f);
    f = nullptr;
}

inline arrays::array_view<char> read_file_as_string(const char* path, arenas::arena& arena = arenas::gta) {
    tmp(f, fopen (path, "rb"));
    chk(f) else return {};

    fseek (f, 0, SEEK_END);
    let length = ftell(f);
    fseek (f, 0, SEEK_SET);

    var buffer = arenas::alloc_g<char>(arena, length + 1);
    fread(buffer.data, 1, length, f);

    buffer[length] = 0;

    return buffer;
}

#endif //FRANCA2_FILES_H
