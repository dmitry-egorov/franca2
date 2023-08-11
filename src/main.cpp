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
using namespace wgsl_ex;
using namespace arenas;

let canvas_name = "canvas";

let clear_color = WGPUColor {.r = 0.2f, .g = 0.2f, .b = 0.3f, .a = 1.0f};

struct state_t {
    struct wgpu_state {
        WGPUInstance  instance;
        WGPUDevice    device;
        WGPUQueue     queue;
        WGPUSwapChain swapchain;
        usize2        size;
    } wgpu;

    struct resources_state {
        WGPUBuffer         uni_buf;
        WGPURenderPipeline pipeline;
        WGPUBindGroup      bind_group;
    } res;
};

#define using_state ref[wgpu, res] = state

static state_t state;

struct gpu_uniforms {
    gpu_transform2 world_to_clip;
};

static char const triangle_wgsl[] = CODE(
    struct gpu_transform2 {
        m: mat2x4f
    };

    fn apply_to_point(t: gpu_transform2, p: vec2f) -> vec2f { return vec2f(
        t.m[0].x * p.x + t.m[0].y * p.y + t.m[0].z,
        t.m[1].x * p.x + t.m[1].y * p.y + t.m[1].z
    );}

    struct IO {
        @location(0) vCol: vec3f,
        @builtin(position) Position: vec4f
    }

    struct uniforms {
        world_to_clip : gpu_transform2
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
    using namespace emsc_ex;
    using_state;
    remake_swap_chain_from_html_canvas(wgpu.instance, wgpu.device, canvas_name, wgpu.swapchain, wgpu.size);
    return 1;
}

static bool init() {
    using namespace emsc_ex;
    gta_init(8 * 1024 * 1024); // 8 MB of global temp storage

    using_state;

    chk((wgpu.instance = make_instance  ({})         )) else return false;
    chk((wgpu.device   = get_wgpu_device()           )) else return false;
    chk((wgpu.queue    = get_queue      (wgpu.device))) else return false;

    resize(); on_resize(resize);

    tmp(shader_module, make_wgsl_shader_module(wgpu.device, triangle_wgsl));

    res.pipeline = make_render_pipeline(wgpu.device, {
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
        .fragment = &WGPUFragmentState{
            .module      = shader_module,
            .entryPoint  = "fs_main",
            .targetCount = 1,
            .targets     = &WGPUColorTargetState{
                .format    = WGPUTextureFormat_BGRA8Unorm,
                .writeMask = WGPUColorWriteMask_All,
            },
        },
    });

    res.uni_buf = make_buffer<gpu_uniforms>(wgpu.device, (WGPUBufferUsage) (WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform));

    res.bind_group = make_single_entry_bind_group(wgpu.device, get_bind_group_layout(res.pipeline, 0), {
        .buffer = res.uni_buf,
        .size   = sizeof(gpu_uniforms),
    });

    return true;
}

static int draw(double time) {
    using namespace transforms;
    using namespace wgsl_ex;
    using_state;

    let world_to_clip =
           identity   ()
        >> rotation   ({.v = (float) (0.0001 * time)})
        >> translation({.x = 0.3, .y = 0.3})
        >> rotation   ({.v = (float) (0.00013 * time)})
        >> uniform_clip_to_clip(wgpu.size)
    ;

    write_buffer(wgpu.queue, res.uni_buf, 0, to_gpu_transform2(world_to_clip));

    {
        tmp(encoder, run_single_pass_encoder(wgpu.device, wgpu.swapchain, wgpu.queue, { .clearValue = clear_color }));// Note: .clearValue might not work with Dawn
        ref pass = encoder.pass;

        set_pipeline  (pass, res.pipeline);
        set_bind_group(pass, 0, res.bind_group);
        draw          (pass, 3);

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