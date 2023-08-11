#ifndef FRANCA2_WEB_EMSC_EX_H
#define FRANCA2_WEB_EMSC_EX_H

#include <emscripten/html5.h>
#include "maths2.h"

namespace emsc_ex {
    namespace {
        int resize_cb(int /*event_type*/, const EmscriptenUiEvent* /*ui_event*/, void* cb) {
            let fcb = (EM_BOOL (*)())cb;
            return fcb();
        }

        EM_BOOL loop_cb(double time, void* cb) {
            let fcb = (int (*)(double time))cb;
            return fcb(time);
        }
    }

    inline usize2 get_element_css_usize2(const char *target) {
        double w, h;
        emscripten_get_element_css_size(target, &w, &h);
        return { .w = (uint)w, .h = (uint)h };
    }

    inline void set_canvas_element_size(const char *target, const usize2 size) {
        emscripten_set_canvas_element_size(target, (int)size.w, (int)size.h);
    }

    inline WGPUDevice get_wgpu_device() {
        return emscripten_webgpu_get_device();
    }

    int on_resize(EM_BOOL (*cb)()) {
        return emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, (void*)cb, false, resize_cb);
    }

    int loop(int (*cb)(double time)) {
        emscripten_request_animation_frame_loop(loop_cb, (void*)cb);
    }
}

#endif //FRANCA2_WEB_EMSC_EX_H
