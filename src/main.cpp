#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <cstdio>

#include "utility/primitives.h"
#include "utility/maths2.h"
#include "utility/syntax.h"
#include "utility/transforms.h"

#include "utility/arenas.h"

#include "utility/wgpu_ex.h"
#include "utility/emsc_ex.h"
#include "utility/wgsl_ex.h"

using namespace wgpu_ex;
using namespace arenas;

struct state_t {
    struct wgpu_state {
        WGPUInstance       instance;
        WGPUDevice         device;
        WGPUQueue          queue;
        WGPUSwapChain      swapchain;
    } wgpu;

    struct canvas_state {
        const char *name;
        usize2 size;
    } canvas;

    struct resources_state {
        WGPUBuffer         uni_buf;
        WGPURenderPipeline pipeline;
        WGPUBindGroup      bind_group;
    } resources;
};

static state_t state;

struct shader_uniforms {
    wgsl_ex::transform2 world_to_clip;
};

static char const triangle_wgsl[] = CODE(
    struct transform2 {
        m: mat2x4f
    };

    fn apply_to_point(t: transform2, p: vec2f) -> vec2f {return vec2f(
        t.m[0].x * p.x + t.m[0].y * p.y + t.m[0].z,
        t.m[1].x * p.x + t.m[1].y * p.y + t.m[1].z
    );}

    struct IO {
        @location(0) vCol : vec3f,
        @builtin(position) Position : vec4f
    }

    struct uniforms {
        world_to_clip : transform2
    }

    @group(0) @binding(0) var<uniform> uni : uniforms;

    @vertex
    fn vs_main(@builtin(vertex_index) index : u32) -> IO {
        const poss = array<vec2f, 3>(
            vec2f(-0.5,  0.5),
            vec2f( 0.5,  0.5),
            vec2f(-0.5, -0.5)
        );

        const cols = array<vec3f, 3>(
            vec3f(0, 0, 1),
            vec3f(0, 1, 0),
            vec3f(1, 0, 0)
        );

        let pos = apply_to_point(uni.world_to_clip, poss[index]);
        return IO(
            cols[index],
            vec4f(pos, 0, 1)
        );
    }

    @fragment
    fn fs_main(@location(0) col : vec3f) -> @location(0) vec4f {
        return vec4f(col, 1.0);
    }
);

int resize() {
    state.canvas.size = emsc_ex::get_element_css_usize2(state.canvas.name);
    emsc_ex::set_canvas_element_size(state.canvas.name, state.canvas.size);

    release(state.wgpu.swapchain);

    tmp(surface, make_surface(state.wgpu.instance, {
        .nextInChain = &gta_one(WGPUSurfaceDescriptorFromCanvasHTMLSelector {
            .chain = { .sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector },
            .selector = state.canvas.name
        })->chain
    }));

    state.wgpu.swapchain = make_swap_chain(state.wgpu.device, surface, {
        .usage  = WGPUTextureUsage_RenderAttachment,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .width  = state.canvas.size.w,
        .height = state.canvas.size.h,
        .presentMode = WGPUPresentMode_Fifo,
    });

    return 1;
}

static bool init() {
    gta_init(8 * 1024 * 1024); // 8 MB of global temp storage

    chk((state.wgpu.instance = make_instance({})           )) else return false;
    chk((state.wgpu.device   = emsc_ex::get_wgpu_device()  )) else return false;
    chk((state.wgpu.queue    = get_queue(state.wgpu.device))) else return false;

    state.canvas.name = "canvas";
    resize();
    emsc_ex::on_resize(resize);

    // compile shaders
    tmp(shader_module, make_wgsl_shader_module(state.wgpu.device, triangle_wgsl));

    state.resources.pipeline = make_render_pipeline(state.wgpu.device, {
        .vertex = {
            .module     = shader_module,
            .entryPoint = "vs_main",
        },
        .primitive = {
            .topology = WGPUPrimitiveTopology_TriangleList,
        },
        .multisample = {
            .count = 1,
            .mask  = 0xFFFFFFFF,
        },
        .fragment = gta_one(WGPUFragmentState {
            .module      = shader_module,
            .entryPoint  = "fs_main",
            .targetCount = 1,
            .targets     = gta_one(WGPUColorTargetState {
                .format    = WGPUTextureFormat_BGRA8Unorm,
                .writeMask = WGPUColorWriteMask_All,
            }),
        }),
    });

    state.resources.uni_buf = make_buffer<shader_uniforms>(state.wgpu.device, (WGPUBufferUsage) (WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform));

    state.resources.bind_group = make_single_entry_bind_group(state.wgpu.device, get_bind_group_layout(state.resources.pipeline, 0), {
        .buffer = state.resources.uni_buf,
        .size   = sizeof(shader_uniforms),
    });

    return true;
}

/**
 * Draws using the above state.wgpu.pipeline and buffers.
 */
static int draw(double time) {
    using namespace transforms;
    let world_to_clip =
           identity   ()
        >> rotation   ({.v = (float) (0.0002 * time)})
        //>> translation({.x = 0.3, .y = 0.3})
        >> uniform_clip_to_clip(aspect_ratio_of(state.canvas.size))
    ;

    write_buffer(state.wgpu.queue, state.resources.uni_buf, 0, wgsl_ex::to_wgsl_transform2(world_to_clip));

    {
        tmp(encoder, run_single_pass_encoder(state.wgpu.device, state.wgpu.swapchain, state.wgpu.queue, {
            .clearValue = {.r = 0.2f, .g = 0.2f, .b = 0.3f, .a = 1.0f} // Note: might not work with Dawn
        }));

        set_pipeline  (encoder.pass, state.resources.pipeline);
        set_bind_group(encoder.pass, 0, state.resources.bind_group);
        draw          (encoder.pass, 3);

#ifndef __EMSCRIPTEN__
        present(state.wgpu.swapchain);
#endif
    }

    gta_reset();

    return true;
}

extern "C" __attribute__((used, visibility("default"))) void entry_point() {

    if (init()); else return;
    emsc_ex::loop(draw);
}

namespace impl {
/**
 * JavaScript async calls that need to finish before calling \c main().
 */
    EM_JS(void, glue_preint, (), {
        /*
         * None of the WebGPU properties appear to survive Closure, including
         * Emscripten's own `preinitializedWebGPUDevice` (which from looking at
         *`library_html5` is probably designed to be inited in script before
         * loading the Wasm).
         */
        if (navigator["gpu"]) {} else { console.error("No support for WebGPU; not starting"); return; }

        navigator["gpu"]["requestAdapter"]().then(function (adapter) {
            adapter["requestDevice"]().then( function (device) {
                Module["preinitializedWebGPUDevice"] = device;
                _entry_point();
            });
        }, function () {
            console.error("No WebGPU adapter; not starting");
        });
    })
}

int main(int /*argc*/, char* /*argv*/[]) {
    impl::glue_preint();
    return 0;
}