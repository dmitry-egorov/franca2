//
// Created by degor on 15.09.2023.
//

#ifndef FRANCA2_SWAP_QUEUE_H
#define FRANCA2_SWAP_QUEUE_H

#include "arenas.h"

namespace swap_queues {
    using namespace arenas;

    tt struct swap_queue {
        arr_dyn<t> queues[2];
        size_t next_item_index = 0;
        u32 current_push_queue_index = 0;
    };

    tt swap_queue<t> make_swap_queue(arena& arena) {
        return {
            .queues = {
                make_arr_dyn<t>(1024, arena, 8),
                make_arr_dyn<t>(1024, arena, 8),
            },
            .next_item_index = 0,
            .current_push_queue_index = 0
        };
    }

    tt void push(swap_queue<t>& queue, t value) {
        push(queue.queues[queue.current_push_queue_index], value);
    }

    tt ret1<t> try_pop(swap_queue<t>& queue) {
        ref push_queue_index = queue.current_push_queue_index;
        ref pop_queue = queue.queues[1 - push_queue_index];
        ref index = queue.next_item_index;
        if (index < pop_queue.count)
            return ret1_ok(pop_queue[index++]);

        clear(pop_queue);
        index = 0;
        push_queue_index = 1 - push_queue_index;

        return try_pop(queue);
    }
}
#endif //FRANCA2_SWAP_QUEUE_H
