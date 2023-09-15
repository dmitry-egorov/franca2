//
// Created by degor on 15.09.2023.
//

#ifndef FRANCA2_SWAP_QUEUE_H
#define FRANCA2_SWAP_QUEUE_H

#include "syntax.h"
#include "results.h"
#include "arenas.h"
#include "arr_dyns.h"

namespace swap_queuess {
    using namespace arenas;

    tt struct swap_queues {
        arr_dyn<t>  push_queue;
        arr_dyn<t>   pop_queue;
        size_t next_item_index;
    };

    tt swap_queues<t> make_swap_queue(arena& arena) {
        return {
            .push_queue = make_arr_dyn<t>(1024, arena),
            . pop_queue = make_arr_dyn<t>(1024, arena),
            .next_item_index = 0
        };
    }

    tt void push(swap_queues<t>& queue, t value) {
        push(queue.push_queue, value);
    }

    tt ret1<t> try_pop(swap_queues<t>& queue) {
        ref pop_queue = queue.pop_queue;
        ref index = queue.next_item_index;
        if (index < pop_queue.count); else return ret1_fail;

        return ret1_ok(pop_queue[index++]);
    }

    tt void swap(swap_queues<t>& queue) {
        clear(queue.pop_queue);
        queue.next_item_index = 0;

        var tmp = queue.push_queue;
        queue.push_queue = queue.pop_queue;
        queue. pop_queue = tmp;
    }

    tt bool queues_identical(swap_queues<t>& queue) {
        return equals(queue.pop_queue, queue.push_queue);
    }

    tt bool push_queue_is_empty(swap_queues<t>& queue) {
        return queue.push_queue.count == 0;
    }

}
#endif //FRANCA2_SWAP_QUEUE_H
