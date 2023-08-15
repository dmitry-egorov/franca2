#ifndef FRANCA2_WEB_EMSC_EX_H
#define FRANCA2_WEB_EMSC_EX_H

#include <emscripten/html5.h>
#include "syntax.h"
#include "maths2.h"
#include "wgpu_ex.h"

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

    inline void remake_swap_chain_from_html_canvas(WGPUInstance instance, WGPUDevice device, const char *selector, WGPUSwapChain& swap_chain, usize2& px_size) {
        using namespace wgpu_ex;

        release(swap_chain);

        let dips_size = get_element_css_usize2(selector);
        set_canvas_element_size(selector, dips_size);

        tmp(surface, make_surface_from_html_canvas(instance, selector));

        let dpi_ratio = emscripten_get_device_pixel_ratio();
        px_size    = to_usize2(dips_size * (float)dpi_ratio);
        swap_chain = make_swap_chain(device, surface, {
            .usage  = WGPUTextureUsage_RenderAttachment,
            .format = WGPUTextureFormat_BGRA8Unorm,
            .width  = px_size.w,
            .height = px_size.h,
            .presentMode = WGPUPresentMode_Fifo,
        });
    }

    int on_resize(EM_BOOL (*cb)()) {
        return emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, (void*)cb, false, resize_cb);
    }

    void loop(int (*cb)(double time)) {
        emscripten_request_animation_frame_loop(loop_cb, (void*)cb);
    }
}

#endif //FRANCA2_WEB_EMSC_EX_H
