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

tt4 struct ret4 {
    t0   v0;
    t1   v1;
    t2   v2;
    t3   v3;
    bool ok;
};

#define ret1_ok(v0) {v0, true}
#define ret2_ok(v0, v1) {v0, v1, true}
#define ret3_ok(v0, v1, v2) {v0, v1, v2, true}
#define ret4_ok(v0, v1, v2, v3) {v0, v1, v2, v3, true}

#define ret1_fail {{}, false}
#define ret2_fail {{}, {}, false}
#define ret3_fail {{}, {}, {}, false}
#define ret4_fail {{}, {}, {}, false}

#define line_res stx_concat(res, __LINE__)
#define line_ok stx_concat(ok, __LINE__)
#define var1(name0, initializer) var [name0, line_ok] = (initializer)

#define if_var1(name0, initializer) var [name0, line_ok] = (initializer); if (line_ok)
#define if_var2(name0, name1, initializer) var [name0, name1, line_ok] = (initializer); if (line_ok)
#define if_var3(name0, name1, name2, initializer) var [name0, name1, name2, line_ok] = (initializer); if (line_ok)
#define if_var4(name0, name1, name2, name3, initializer) var [name0, name1, name2, name3, line_ok] = (initializer); if (line_ok)
#define if_set1(name0, initializer) var [line_res, line_ok] = (initializer); if (line_ok) { name0 = line_res; } if (line_ok)

#define if_vari2(name0, init0, name1, init1) var [name0, line_ok##_0] = (init0); var [name1, line_ok##_1] = (init1); if (line_ok##_0 && line_ok##_1)
#define if_vari3(name0, init0, name1, init1, name2, init2) var [name0, line_ok##_0] = (init0); var [name1, line_ok##_1] = (init1); var [name2, line_ok##_2] = (init2); if (line_ok##_0 && line_ok##_1 && line_ok##_2)


#define if_tmp1(name0, initializer) var [name0, line_ok] = (initializer); defer_release(name0); if (line_ok)
#define if_tmp2(name0, name1, initializer) var [name0, name1, line_ok] = (initializer); defer_release2(name0, name1); if (line_ok)
#define if_tmp3(name0, name1, name2, initializer) var [name0, name1, name2, line_ok] = (initializer); defer_release2(name0, name1, name2); if (line_ok)

#define asr_1(name0, initializer) var [name0, line_ok] = (initializer); assert(line_ok)

#endif //FRANCA2_RESULTS_H
