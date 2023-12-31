#ifndef FRANCA2_ITERATORS_H
#define FRANCA2_ITERATORS_H

#include "syntax.h"
#include "results.h"
#include "arrays.h"
#include "arenas.h"
#include "strings.h"

namespace iterators {
    using namespace arenas;
    using namespace arrays;

    tt bool is_empty(const arr_view<t>& it) { return it.count == 0; }

    tt ret1<t> peek(arr_view<t>& it) {
        if (it.count != 0); else return ret1_fail;
        return ret1_ok(it[0]);
    }

    tt bool peek(arr_view<t>& it, const t& value) {
        if (it.count != 0); else return false;
        return it[0] == value;
    }

    tt ret1<t> take(arr_view<t>& it) {
        if (it.count != 0); else return ret1_fail;

        let result = it[0];
        advance(it);
        return ret1_ok(result);
    }

    tt bool take(arr_view<t>& it, const t& value) {
        if_var1 (c, peek(it)); else return false;
             if (c == value ); else return false;
        take(it);
        return true;
    }

    tt bool take(arr_view<t>& it, const arr_view<t>& sequence) {
        if (it.count >= sequence.count); else return false;

        var it_copy    = it;
        var targets_it = sequence;

        while (!is_empty(targets_it)) {
            if(it_copy[0] == targets_it[0]); else return false;
            advance(it_copy);
            advance(targets_it);
        }

        it = it_copy;
        return true;
    }

    tt arr_view<t> take_until(arr_view<t>& it, const t& target) {
        var result = it;
        result.count = 0;

        while (true) {
            if_var1(c, peek(it)); else return result;
                 if(c != target); else return result;
            take(it);
            enlarge(result);
        }
    }

    tt arr_view<t> take_past(arr_view<t>& it, const t& target) {
        var result = arr_view<t> {it.data, 0 };
        while (true) {
            if_var1(c, take(it)); else return result;
            enlarge(result);
            if (c != target); else return result;
        }
    }

    tt arr_view<t> take_until_any(arr_view<t>& it, const arr_view<t>& targets) {
        var result = arr_view<t> {it.data, 0 };
        while (true) {
            if_var1 (c, peek(it)          ); else return result;
                 if (!contains(targets, c)); else return result;

            take(it);
            enlarge(result);
        }
    }

    tt arr_view<t> take_past_any(arr_view<t>& it, const arr_view<t>& targets) {
        var result = arr_view<t> {it.data, 0 };
        while (true) {
            if_var1(c, take(it)); else return result;
            enlarge(result);
            if (!contains(targets, c)); else return result;
        }
    }

    tt arr_view<t> take_while_any(arr_view<t>& it, const arr_view<t>& targets) {
        var result = arr_view<t> {it.data, 0 };
        while (true) {
            if_var1 (c, peek(it)         ); else return result;
                 if (contains(targets, c)); else return result;

            advance(it);
            enlarge(result);
        }
    }

    tt bool skip_until(arr_view<t>& it, const t& target) {
        while (true) {
            if_var1(c, peek(it)); else return false;
            if (c == target) return true;
            take(it);
        }
    }

    tt bool skip_past(arr_view<t>& it, const t& target) {
        while (true) {
            if_var1 (c, take(it)); else return false;
            if (c == target) return true;
        }
    }

    tt void skip_while(arr_view<t>& it, const t& target) {
        while (true) {
            if_var1 (c, peek(it)); else return;
            if (c != target) return;
            take(it);
        }
    }

    tt void skip_while_any(arr_view<t>& it, const arr_view<t>& targets) {
        while (true) {
            if_var1 (c, peek(it));          else return;
                 if (contains(targets, c)); else return;
            advance(it);
        }
    }
}

#endif //FRANCA2_ITERATORS_H
