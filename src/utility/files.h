#ifndef FRANCA2_FILES_H
#define FRANCA2_FILES_H
#include <cstdio>
#include "syntax.h"
#include "arenas.h"
#include "strings.h"

namespace files {
    using namespace arrays;
    using namespace arenas;
    using namespace strings;

    void release(FILE*& f);
    inline string read_file_as_string(cstr path, arena& arena = gta);
    inline string read_files_as_string(const arr_view<cstr>& paths, string separator = view(""), arena& arena = gta);

    void release(FILE*& f) {
        if (f) fclose(f);
        f = nullptr;
    }

    string read_file_as_string(cstr path, arena& arena) {
        tmp(f, fopen (path, "rb"));
        if (f); else return {};

        fseek (f, 0, SEEK_END);
        let length = ftell(f);
        fseek (f, 0, SEEK_SET);

        var buffer = arenas::alloc_g<char>(arena, length);
        fread(buffer.data, 1, length, f);

        return buffer;
    }

    string read_files_as_string(const arr_view<cstr>& paths, string separator, arena& arena) {
        var length = (size_t)0;
        for_arr(paths) {
            tmp(f, fopen (paths[i], "rb"));
            if (f); else { printf("Failed to open file %s\n", paths[i]); continue; }

            fseek (f, 0, SEEK_END);
            length += ftell(f);

            if (i < paths.count - 1)  length += separator.count;
        }

        var loaded_bytes = 0u;
        var buffer = arenas::alloc_g<char>(arena, length);
        for_arr(paths) {
            tmp(f, fopen (paths[i], "rb"));
            if (f); else return {};
            loaded_bytes += fread(buffer.data + loaded_bytes, 1, length - loaded_bytes, f);
            if (i < paths.count - 1) memcpy(buffer.data + loaded_bytes, separator.data, separator.count);
            loaded_bytes += separator.count;
        }

        return buffer;
    }



}

#endif //FRANCA2_FILES_H
