#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wunknown-attributes"

#include <emscripten/em_js.h>
#include <emscripten/html5_webgpu.h>
#include <array>
#include <chrono>
#include <cstdio>

#define FRANCA2_IMPLS

#include "utility/maths2.h"
#include "utility/syntax.h"
#include "utility/transforms.h"
#include "utility/arrays.h"
#include "utility/arenas.h"

#include "utility/wgpu_ex.h"
#include "utility/wgpu_png.h"
#include "utility/emsc_ex.h"

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "../third_party/stb/stb_image.h"
#pragma clang diagnostic pop

#include "color_palette.h"

#include "code_views.h"

#include "visual_asts.h"
#include "visual_parser.h"
#include "visual_display.h"
#include "line_view_gen.h"

#include "asts.h"
#include "asts_parsing.h"
#include "asts_printing.h"
#include "asts_storage.h"
#include "asts_analysis.h"
#include "asts_codegen.h"
#include "asts_compilation.h"
#include "asts_display.h"

using namespace arenas;
using namespace wgpu_ex;
using namespace wgpu_ex::png;
using namespace emsc_ex;
using namespace visual_asts::view_gen;
using namespace line_view_gen;
using namespace visual_asts::parser;

//const char *ascii_chars = R"end( !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~)end";

let canvas_name = "canvas";

let clear_color = WGPUColor {.r = 0.1f, .g = 0.1f, .b = 0.15f, .a = 1.0f};

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

    struct view_state {
        bool dirty;
    } view;
};

#define using_state ref[wgpu, res, view] = state
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

    std::array<uint, 256> palette;
};

EM_JS(int, run_wasm, (u8* ptr, size_t size), {
    var text_decoder = new TextDecoder();
    var binary = new Uint8Array(wasmMemory.buffer, ptr, size);
    var module = new WebAssembly.Module(binary);
    var imports = {};
    imports["env"] = {};
    imports["env"]["print_str"] = (ptr, len) => console.log(text_decoder.decode(new Uint8Array(instance.exports.memory.buffer, ptr, len)));
    var instance = new WebAssembly.Instance(module, imports);

    return instance.exports["main"]();
});

static bool init() {
    using namespace compute_asts;
    using namespace std::chrono;

    gta_init(8 * 1024 * 1024); // 16 MB of global temp storage

    init_palette();

    using_state;
    using_wgpu_state;

    var memory_used = gta.used_bytes;

    if ((instance = make_instance  ({}    ))); else return false;
    if ((device   = get_wgpu_device(      ))); else return false;
    if ((queue    = get_queue      (device))); else return false;

    res.uni_buf       = make_buffer<gpu_uniforms>(device, (WGPUBufferUsage) (WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform | WGPUBufferUsage_QueryResolve));
    res.diag_text_buf = make_buffer(device, (WGPUBufferUsage) (WGPUBufferUsage_Storage), 256);

    tmp(wrap_sampler, make_wrap_sampler(device));

    if_tmp2(font_texture, font_tv, load_8bit_texture_from_png_file(device, "embedded/jb.png")); else return(false);

    printf("Resources loaded. Total memory used: %zu, delta: %zu\n", gta.used_bytes, gta.used_bytes - memory_used); memory_used = gta.used_bytes;

    var code_view = make_code_vew(code_view_size);
    var cv_it = iterate(code_view);

    //if_var1(ast, parse_file("embedded/test_program.frv")) else return false;
    //print_ast(&ast);
    //display(ast, cv_it);

    var source_files = make_arr_dyn<cstr>(16);

    //push(source_files, (cstr)"embedded/hello_world.fr");
    //push(source_files, (cstr)"embedded/test0.fr");
    //push(source_files, (cstr)"embedded/primitives_test.fr");
    //push(source_files, (cstr)"embedded/fib.fr");
    //push(source_files, (cstr)"embedded/prelude.fr");

    //push(source_files, (cstr)"embedded/scope_vars_test.fr");
    //push(source_files, (cstr)"embedded/ref_test.fr");
    push(source_files, (cstr)"embedded/macro_test.fr");

    let old_compiler = false;

    wasm_emit::init();

    var ast_data_arena = arenas::make(4 * 1024 * 1024);
    var ast = make_ast(ast_data_arena);
    parse_files(source_files.data, ast);
    printf("\nAST:\n");
    print_ast(ast);
    printf("AST parsed. Nodes: %zu. Total memory used: %zu, delta: %zu\n", ast.nodes.count, gta.used_bytes, gta.used_bytes - memory_used); memory_used = gta.used_bytes;

    arr_view<u8> old_wasm = {};
    arr_view<u8> wasm = {};

    if (old_compiler) {
        let old_compile_start = std::chrono::high_resolution_clock::now();
        printf("\n(WASM) Compiling...\n");
        old_wasm = compile(ast);
        printf("(WASM) Compiled in %lldms. Size: %zu, total memory used: %zu, delta: %zu\n", duration_cast<milliseconds>(high_resolution_clock::now() - old_compile_start).count(), old_wasm.count, gta.used_bytes, gta.used_bytes - memory_used); memory_used = gta.used_bytes;
        let view_start = std::chrono::high_resolution_clock::now();

        let old_view_gen_start = std::chrono::high_resolution_clock::now();
        printf("\nGenerating code view...\n");
        display(ast, cv_it);
        code_view.line_count = cv_it.cell_idx.y + 1;
        printf(cv_it.builder.data);
        printf("Code view generated in %lldms. Total memory used: %zu, delta: %zu\n", duration_cast<milliseconds>(high_resolution_clock::now() - old_view_gen_start).count(), gta.used_bytes, gta.used_bytes - memory_used); memory_used = gta.used_bytes;
    } else {
        let compile_start = std::chrono::high_resolution_clock::now();
        printf("\nAnalyzing...\n");
        analyze(ast);
        printf("Analyzing in %lldms. Total memory used: %zu, delta: %zu\n", duration_cast<milliseconds>(high_resolution_clock::now() - compile_start).count(), gta.used_bytes, gta.used_bytes - memory_used); memory_used = gta.used_bytes;

        let generate_start = std::chrono::high_resolution_clock::now();
        printf("\nGenerating WASM...\n");
        wasm = emit_wasm(ast);
        printf("Generated in %lldms. Size: %zu, total memory used: %zu, delta: %zu\n", duration_cast<milliseconds>(high_resolution_clock::now() - generate_start).count(), wasm.count, gta.used_bytes, gta.used_bytes - memory_used); memory_used = gta.used_bytes;

    }



    if_var1(line_view, generate_line_view(code_view.line_count, line_view_size)); else return false;

    if_tmp2(_0, line_glyph_tv, make_texture_2d(device, line_view_size, WGPUTextureFormat_R8Uint, line_view.glyphs.data)); else return false;
    if_tmp2(_1, code_glyph_tv, make_texture_2d(device, code_view_size, WGPUTextureFormat_R8Uint, code_view.glyphs.data)); else return false;
    if_tmp2(_2, code_color_tv, make_texture_2d(device, code_view_size, WGPUTextureFormat_R8Uint, code_view.colors.data)); else return false;
    if_tmp2(_3, code_inlay_tv, make_texture_2d(device, code_view_size, WGPUTextureFormat_R8Uint, code_view.inlays.data)); else return false;

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

    //res.perf_query_set = make_query_set(device, {
    //    .type  = WGPUQueryType_Timestamp,
    //    .count = 2,
    //});

    resize(); on_resize(resize);

    printf("WGPU ready. Total memory used: %zu, delta: %zu\n", gta.used_bytes, gta.used_bytes - memory_used); memory_used = gta.used_bytes;

    if (old_compiler) {
        let wasm_start = std::chrono::high_resolution_clock::now();
        printf("\n(WASM) Running...\n");
        var wasm_result = run_wasm(old_wasm.data, old_wasm.count);
        printf("(WASM) Exited with code: %d\n", wasm_result);
        printf("(WASM) Finished in %lldms. Total memory used: %zu, delta: %zu\n", duration_cast<milliseconds>(high_resolution_clock::now() - wasm_start).count(), gta.used_bytes, gta.used_bytes - memory_used); memory_used = gta.used_bytes;
    }
    else {
        let wasm_start = std::chrono::high_resolution_clock::now();
        printf("\n(WASM) Running...\n");
        var wasm_result = run_wasm(wasm.data, wasm.count);
        printf("(WASM) Exited with code: %d\n", wasm_result);
        printf("(WASM) Finished in %lldms. Total memory used: %zu, delta: %zu\n", duration_cast<milliseconds>(high_resolution_clock::now() - wasm_start).count(), gta.used_bytes, gta.used_bytes - memory_used); memory_used = gta.used_bytes;
    }

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

    view.dirty = true;

    return 1;
}

static int draw(double /*time*/) {
    using_state;
    using_wgpu_state;

    if(view.dirty); else return true; defer { view.dirty = false; };

    {
        tmp(encoder, make_command_encoder(device));

        //write_timestamp(encoder, res.perf_query_set, 0);

        dispatch_compute_pass(encoder, res.bound_timings_text_pipeline, 1);
        draw_fullscreen_pass (encoder, swapchain, res.bound_render_pipeline, clear_color);

        //write_timestamp  (encoder, res.perf_query_set, 1);
        //resolve_query_set(encoder, res.perf_query_set, 0, 2, res.uni_buf, offsetof(gpu_uniforms, draw_start_timestamp));

        submit(queue, encoder);

#ifndef __EMSCRIPTEN__
        present(swapchain);
#endif
    }

    gta_reset();

    return true;
}

extern "C" __attribute__((used, visibility("default"))) void entry_point() {
    if (init()); else { printf("Failed to initialize\n"); return;}
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
            //var features = {requiredFeatures: ["timestamp-query"]};
            var features = {};
            adapter["requestDevice"](features).then( function (device) {
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

#pragma clang diagnostic pop