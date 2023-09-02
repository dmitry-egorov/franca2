#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"

#ifndef FRANCA2_COMPUTE_DEFINES_H
#define FRANCA2_COMPUTE_DEFINES_H

#include <cstring>
#include "utility/syntax.h"
#include "utility/primitives.h"
#include "utility/results.h"
#include "utility/maths2.h"
#include "utility/arrays.h"
#include "utility/arenas.h"
#include "utility/iterators.h"
#include "utility/strings.h"
#include "code_views.h"
#include "compute_asts.h"

namespace compute_asts {
    using enum builtin_func_id;

    struct variable {
        string id;
        string display_name;
        poly_value value;
    };

    struct func {
        string id;
        node* display;
        node* type;
        node* body;
    };

    struct scope {
        size_t prev_vars_count;
        size_t prev_funcs_count;
    };

    struct storage {
        arr_dyn<scope   > scopes;
        arr_dyn<variable> vars  ;
        arr_dyn<func    > funcs ;
    };

#define using_storage ref [scopes, vars, funcs] = storage

    static storage make_storage(arena& arena = gta);

    void push_var  (storage &, const variable&);
    void push_scope(storage&);
    void  pop_scope(storage&);
    variable* find_var (storage&, uint id);
    func*     find_func(storage&, uint id);

    static storage make_storage(arena& arena) {
        return {
            make_arr_dyn<scope   >(1024, arena, 8),
            make_arr_dyn<variable>(1024, arena, 8),
            make_arr_dyn<func    >(1024, arena, 8),
        };
    }

    void push_var(storage& storage, const variable& v) {
        using_storage;
        push(vars, v);
    }

    void push_func(storage& storage, const func& f) {
        using_storage;
        push(funcs, f);
    }

    void push_scope(storage& storage) {
        using_storage;
        push(scopes, {vars.data.count, funcs.data.count});
    }

    void pop_scope(storage& storage) {
        using_storage;
        let scope = pop(scopes);
        vars .data.count = scope.prev_vars_count;
        funcs.data.count = scope.prev_funcs_count;
    }
#define tmp_scope(storage) push_scope(storage); defer { pop_scope(storage); }

    variable* find_var(storage& storage, const string id) {
        using_storage;

        var vars_view = vars.data;
        for (var i = (int)vars_view.count - 1; i >= 0; --i) {
            ref v = vars_view[i];
            if (v.id == id) return &v;
        }

        return nullptr;
    }

    func* find_func(storage& storage, const string id) {
        using_storage;
        var funcs_view = funcs.data;
        for (var i = (int)funcs_view.count - 1; i >= 0; --i) {
            ref v = funcs_view[i];
            if (v.id == id) return &v;
        }

        return nullptr;
    }
}

#endif //FRANCA2_COMPUTE_DEFINES_H

#pragma clang diagnostic pop