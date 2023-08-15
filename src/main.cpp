#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <cstdio>
#include <array>

#include "utility/primitives.h"
#include "utility/maths2.h"
#include "utility/syntax.h"
#include "utility/transforms.h"
#include "utility/arenas.h"
#include "utility/strings.h"

#include "utility/wgpu_ex.h"
#include "utility/wgpu_png.h"
#include "utility/emsc_ex.h"
#include "utility/wgsl_ex.h"

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"

#include "code_view_generation.h"
#include "line_view_generation.h"

using namespace arenas;
using namespace wgpu_ex;
using namespace wgpu_ex::png;
using namespace wgsl_ex;
using namespace emsc_ex;
using namespace code_view_generation;
using namespace line_view_generation;

//const char *ascii_chars = R"end( !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~)end";

let canvas_name = "canvas";

let clear_color = WGPUColor {.r = 0.1f, .g = 0.1f, .b = 0.15f, .a = 1.0f};

let palette = array<uint, 256> {
    0xa7a7ebff, // 0 regular
    0xffffffff, // 1 identifier
    0x416ecaff, // 2 break statement
    0x3393ffff, // 3 return statements
    0xae76c0ff, // 4 def
    0xbbbbbbff, // 5 implicitly declared vars
    0xa1c289ff, // 6 strings
    0xc6b88cff, // 7 constants
    0xdddddd44, // 8 inlays
};

struct state_t {
    struct wgpu_state {
        WGPUInstance  instance;
        WGPUDevice    device;
        WGPUQueue     queue;
        WGPUSwapChain swapchain;
        usize2        px_size;
    } wgpu;

    struct resources_state {
        WGPUBuffer          uni_buf;
        WGPUBuffer    diag_text_buf;
        BoundRenderPipeline  bound_render_pipeline;
        BoundComputePipeline bound_timings_text_pipeline;
        WGPUQuerySet  perf_query_set;
    } res;
};

#define using_state ref[wgpu, res] = state
#define using_wgpu_state ref [instance, device, queue, swapchain, px_size] = state.wgpu

static let code_view_size = usize2 {.w = 120, .h = 40};
static let line_view_size = usize2 {.w = 5, .h = 40};

static state_t state;

static int resize();

struct gpu_uniforms {
    uint64_t draw_start_timestamp;
    uint64_t draw_end_timestamp;

    usize2  view_size;
    usize2  cell_size;
    usize2 glyph_size;
    uint   margin;
    uint   line_view_width;

    uint3  _pad0;
    uint   inlay_color;

    array<uint, 256> palette;
};

static bool init() {
    gta_init(8 * 1024 * 1024); // 8 MB of global temp storage

    using_state;
    using_wgpu_state;

    chk((instance = make_instance  ({}    ))) else return false;
    chk((device   = get_wgpu_device(      ))) else return false;
    chk((queue    = get_queue      (device))) else return false;

    res.uni_buf       = make_buffer<gpu_uniforms>(device, (WGPUBufferUsage) (WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform | WGPUBufferUsage_QueryResolve));
    res.diag_text_buf = make_buffer(device, (WGPUBufferUsage) (WGPUBufferUsage_Storage), 256);

    tmp(wrap_sampler, make_wrap_sampler(device));

    try_tmp2(font_texture, font_tv, load_8bit_texture_from_png_file(device, "embedded/jb.png")) else return(false);

    try_ret1(code_view, parse_file("embedded/test_program.fr", code_view_size)) else return false;
	
	try_ret1(line_view, generate_line_view(code_view.line_count, line_view_size)) else return false;

    try_tmp2(_0, line_glyph_tv, make_texture_2d(device, line_view_size, WGPUTextureFormat_R8Uint, line_view.glyphs.data)) else return false;
    try_tmp2(_1, code_glyph_tv, make_texture_2d(device, code_view_size, WGPUTextureFormat_R8Uint, code_view.glyphs.data)) else return false;
    try_tmp2(_2, code_color_tv, make_texture_2d(device, code_view_size, WGPUTextureFormat_R8Uint, code_view.colors.data)) else return false;
    try_tmp2(_3, code_inlay_tv, make_texture_2d(device, code_view_size, WGPUTextureFormat_R8Uint, code_view.inlays.data)) else return false;

    tmp(shader_module, make_wgsl_shader_module_from_file(device, "embedded/main.wgsl"));

    let pipeline = make_render_pipeline(device,
        make_vertex_state  (shader_module, "vs_main"),
        make_fragment_state(shader_module, "fs_main", premul_alpha_blend_state())
    );

    let bind_group = make_bind_group(device, pipeline, {
        {.buffer       = res.uni_buf      },
        {.sampler      = wrap_sampler     },
        {.texture_view = font_tv          },
        {.texture_view = line_glyph_tv    },
        {.texture_view = code_glyph_tv    },
        {.texture_view = code_color_tv    },
        {.texture_view = code_inlay_tv    },
        {.buffer       = res.diag_text_buf},
    });

    res.bound_render_pipeline = {pipeline, bind_group};

    let timing_text_pipeline = make_compute_pipeline(device, {
        .compute = {
            .module = shader_module,
            .entryPoint = "cs_update_timing_text",
        }
    });

    let timing_text_bind_group = make_bind_group(device, timing_text_pipeline, {
        {.buffer = res.uni_buf      },
        {.buffer = res.diag_text_buf},
    });

    res.bound_timings_text_pipeline = {timing_text_pipeline, timing_text_bind_group};

    res.perf_query_set = make_query_set(device, {
        .type  = WGPUQueryType_Timestamp,
        .count = 2,
    });

    resize(); on_resize(resize);

    return true;
}

static int resize() {
    using namespace transforms;

    using_state;
    using_wgpu_state;
    remake_swap_chain_from_html_canvas(instance, device, canvas_name, swapchain, px_size);

    write_buffer(queue, res.uni_buf, gpu_uniforms {
        . view_size  = px_size,
        . cell_size  = {.w = 16, .h = 40},
        .glyph_size  = {.w = 16, .h = 32},
        .margin      = 8,
        .line_view_width = line_view_size.w,
        .inlay_color = 8,
        .palette     = palette
    });

    return 1;
}

static int draw(double /*time*/) {
    using_state;
    using_wgpu_state;

    {
        tmp(encoder, make_command_encoder(device));

        write_timestamp(encoder, res.perf_query_set, 0);

        dispatch_compute_pass(encoder, res.bound_timings_text_pipeline, 1);
        draw_fullscreen_pass (encoder, swapchain, res.bound_render_pipeline, clear_color);

        write_timestamp(encoder, res.perf_query_set, 1);

        resolve_query_set(encoder, res.perf_query_set, 0, 2, res.uni_buf, offsetof(gpu_uniforms, draw_start_timestamp));

        submit(queue, encoder);

#ifndef __EMSCRIPTEN__
        present(swapchain);
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
            adapter["requestDevice"]({requiredFeatures: ["timestamp-query"]}).then( function (device) {
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