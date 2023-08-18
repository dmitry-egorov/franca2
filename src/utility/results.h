#ifndef FRANCA2_RESULTS_H
#define FRANCA2_RESULTS_H

#include "syntax.h"

tt1 struct ret1 {
    t0   v0;
    bool ok;
};

tt2 struct ret2 {
    t0   v0;
    t1   v1;
    bool ok;
};

tt3 struct ret3 {
    t0   v0;
    t1   v1;
    t2   v2;
    bool ok;
};

#define ret1_ok(v0) {v0, true}
#define ret2_ok(v0, v1) {v0, v1, true}
#define ret3_ok(v0, v1, v2) {v0, v1, v2, true}

#define ret1_fail {{}, false}
#define ret2_fail {{}, {}, false}
#define ret3_fail {{}, {}, {}, false}

#define chk_var1(name0, initializer) var [name0, stx_concat(ok, __LINE__)] = (initializer); if (stx_concat(ok, __LINE__));
#define chk_var2(name0, name1, initializer) var [name0, name1, stx_concat(ok, __LINE__)] = (initializer); if (stx_concat(ok, __LINE__));
#define chk_set1(name0, initializer) var [stx_concat(res, __LINE__), stx_concat(ok, __LINE__)] = (initializer); if (stx_concat(ok, __LINE__)) { name0 = stx_concat(res, __LINE__); }

#define chk_tmp1(name0, initializer) var [name0, stx_concat(ok, __LINE__)] = (initializer); defer_release(name0); if (stx_concat(ok, __LINE__));
#define chk_tmp2(name0, name1, initializer) var [name0, name1, stx_concat(ok, __LINE__)] = (initializer); defer_release2(name0, name1); if (stx_concat(ok, __LINE__));
#define chk_tmp3(name0, name1, name2, initializer) var [name0, name1, name2, stx_concat(ok, __LINE__)] = (initializer); defer_release2(name0, name1, name2); if (stx_concat(ok, __LINE__));

#define asr_1(name0, initializer) var [name0, stx_concat(ok, __LINE__)] = (initializer); assert(stx_concat(ok, __LINE__))

#endif //FRANCA2_RESULTS_H
