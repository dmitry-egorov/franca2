//
// Created by degor on 15.09.2023.
//

#ifndef FRANCA2_ARR_BUCKS_H
#define FRANCA2_ARR_BUCKS_H

#include "syntax.h"
#include "arrays.h"
#include "arenas.h"
#include "arr_dyns.h"

namespace arrays {
    using namespace arenas;

    tt struct arr_buck {
        arr_dyn<t*> buckets;
        size_t bucket_size;
        size_t last_bucket_count;

              t& operator[](size_t index);
        const t& operator[](size_t index) const;
    };

    tt const t& arr_buck<t>::operator[](size_t index) const {
        let bucket_i    = index / bucket_size;
        let in_bucket_i = index % bucket_size;

        assert(bucket_i < buckets.count);
        assert((bucket_i != buckets.count - 1) || (in_bucket_i < last_bucket_count));

        return buckets[bucket_i][in_bucket_i];
    }

    tt t& arr_buck<t>::operator[](size_t index) {
        let bucket_i    = index / bucket_size;
        let in_bucket_i = index % bucket_size;

        assert(bucket_i < buckets.count);
        assert((bucket_i != buckets.count - 1) || (in_bucket_i < last_bucket_count));

        return buckets[bucket_i][in_bucket_i];
    }

    tt arr_buck<t> make_arr_buck(size_t bucket_size, arena& arena = gta) {
        var buckets = make_arr_dyn<t*>(32, arena);
        push(buckets, alloc<t>(arena, bucket_size).data);
        return {
            .buckets     = buckets,
            .bucket_size = bucket_size,
            .last_bucket_count = 0
        };
    }

    tt t& push(t value, arr_buck<t>& arr) {
        if (arr.last_bucket_count < arr.bucket_size); else {
            push(arr.buckets, alloc<t>(*arr.buckets.arena, arr.bucket_size).data);
            arr.last_bucket_count = 0;
        }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-dereference"
        if_ref(last_bucket, last_of(arr.buckets)); else { assert(false); return *(t*)nullptr; }
#pragma clang diagnostic pop

        last_bucket[arr.last_bucket_count++] = value;
        return last_bucket[arr.last_bucket_count - 1];
    }

    tt size_t count_of(const arr_buck<t>& arr) {
        return (arr.buckets.count - 1) * arr.bucket_size + arr.last_bucket_count;
    }
}

#define for_arr_buck(arr, it_name, body)  \
    for_arr(arr.buckets) {                \
        ref bucket = arr.buckets[i];      \
        let count = i == arr.buckets.count - 1 ? arr.last_bucket_count : arr.bucket_size; \
        for(var j = 0u; j < count; j++) { \
            ref it_name = bucket[j];      \
            body                          \
        }                                 \
    }
#define for_arr_buck_begin(arr, it_name, index_name)    \
    var index_name = (size_t)0;                         \
    for_arr2(line_var(buck_i_), arr.buckets) {          \
        ref bucket = arr.buckets[line_var(buck_i_)];    \
        let count = line_var(buck_i_) == arr.buckets.count - 1 ? arr.last_bucket_count : arr.bucket_size; \
        for(var line_var(in_buck_i_) = 0u; line_var(in_buck_i_) < count; line_var(in_buck_i_)++, index_name++) { \
            ref it_name = bucket[line_var(in_buck_i_)]; \

#define for_arr_buck_end                                \
        }                                               \
    }

#endif //FRANCA2_ARR_BUCKS_H
