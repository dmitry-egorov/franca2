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
#include "asts.h"

namespace compute_asts {
    struct st_variable {
        string id;
        string display_name;
    };

    struct st_func {
        string id;
        node* display;
        node* type;

        arr_view<prim_type> params;
    };

    struct st_scope {
        size_t prev_vars_count;
        size_t prev_funcs_count;
    };

    struct storage {
        arr_dyn<st_scope   > scopes;
        arr_dyn<st_variable> vars  ;
        arr_dyn<st_func    > funcs ;

        arena& arena;
    };

#define using_storage ref [scopes, vars, funcs, arena] = storage

    static storage make_storage(arena& arena = gta);

    void push_var  (storage &, const st_variable&);
    void push_scope(storage&);
    void  pop_scope(storage&);
    auto find_var  (storage& storage, string id) -> st_variable*;
    auto find_func (storage& storage, string id, arr_view<prim_type> params) -> st_func*;

    static storage make_storage(arena& arena) {
        return {
            make_arr_dyn<st_scope   >(1024, arena, 8),
            make_arr_dyn<st_variable>(1024, arena, 8),
            make_arr_dyn<st_func    >(1024, arena, 8),
            arena
        };
    }

    void push_var(storage& storage, const st_variable& v) {
        using_storage;
        push(vars, v);
    }

    void push_func(storage& storage, const st_func& f) {
        using_storage;
        push(funcs, f);
    }

    void push_scope(storage& storage) {
        using_storage;
        push(scopes, {vars.count, funcs.count});
    }

    void pop_scope(storage& storage) {
        using_storage;
        let scope = pop(scopes);
        vars .count = scope.prev_vars_count;
        funcs.count = scope.prev_funcs_count;
    }
#define tmp_scope(storage) push_scope(storage); defer { pop_scope(storage); }

    st_variable* find_var(storage& storage, const string id) {
        using_storage;

        var vars_view = vars.data;
        for (var i = (int)vars_view.count - 1; i >= 0; --i) {
            ref v = vars_view[i];
            if (v.id == id) return &v;
        }

        return nullptr;
    }

    st_func* find_func(storage& storage, const string id, const arr_view<prim_type> params) {
        using_storage;
        var funcs_view = funcs.data;
        for (var i = (int)funcs_view.count - 1; i >= 0; --i) {
            ref v = funcs_view[i];

            if (v.id == id); else continue;
            let f_params = v.params;
            if (f_params.count == params.count); else continue;

            var found = true;
            for (var prm_i = 0u; prm_i < params.count; ++prm_i)
                if (f_params[prm_i] != params[prm_i]) {
                    found = false;
                    break;
                }

            if (found); else continue;

            return &v;
        }

        return nullptr;
    }
}

#endif //FRANCA2_COMPUTE_DEFINES_H

#pragma clang diagnostic pop