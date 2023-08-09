#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <cstdio>
#include "utility/syntax.h"
#include "utility/wgpu_ex.h"

using namespace wgpu_ex;

WGPUDevice         wgpu_device;
WGPUQueue          wgpu_queue;
WGPUSwapChain      wgpu_swapchain;
WGPURenderPipeline wgpu_pipeline;

WGPUBuffer      vert_buf; // vertex buffer with triangle position and colours
WGPUBuffer     index_buf; // index buffer
WGPUBuffer       uni_buf; // uniform buffer (containing the rotation angle)
WGPUBindGroup bind_group;

/**
 * Current rotation angle (in degrees, updated per frame).
 */
float rot_deg = 0.0f;

/**
 * WGSL equivalent of \c triangle_vert_spirv.
 */
static char const triangle_wgsl[] = CODE(
    struct VertexIn {
        @location(0) aPos : vec2<f32>,
        @location(1) aCol : vec3<f32>
    }
    struct VertexOut {
        @location(0) vCol : vec3<f32>,
        @builtin(position) Position : vec4<f32>
    }
    struct Rotation {
        @location(0) degs : f32
    }
    @group(0) @binding(0) var<uniform> uRot : Rotation;
    @vertex
    fn vs_main(input : VertexIn) -> VertexOut {
        var rads : f32 = radians(uRot.degs);
        var cosA : f32 = cos(rads);
        var sinA : f32 = sin(rads);
        var rot : mat3x3<f32> = mat3x3<f32>(
            vec3<f32>( cosA, sinA, 0.0),
            vec3<f32>(-sinA, cosA, 0.0),
            vec3<f32>( 0.0,  0.0,  1.0));
        var output : VertexOut;
        output.Position = vec4<f32>(rot * vec3<f32>(input.aPos, 1.0), 1.0);
        output.vCol = input.aCol;
        return output;
    }

    @fragment
    fn fs_main(@location(0) vCol : vec3<f32>) -> @location(0) vec4<f32> {
        return vec4<f32>(vCol, 1.0);
    }
);

// create the buffers (x, y, r, g, b)
float const vertex_data[] = {
    -0.8f, -0.8f, 0.0f, 0.0f, 1.0f, // BL
     0.8f, -0.8f, 0.0f, 1.0f, 0.0f, // BR
    -0.0f,  0.8f, 1.0f, 0.0f, 0.0f, // top
};
uint16_t const index_data[] = {
    0, 1, 2,
    0 // padding (better way of doing this?)
};

/**
 * Helper to create a buffer.
 *
 * \param[in] data pointer to the start of the raw data
 * \param[in] size number of bytes in \a data
 * \param[in] usage type of buffer
 */

/**
 * Bare minimum wgpu_pipeline to draw a triangle using the above shaders.
 */
static void init() {
    wgpu_queue = get_queue(wgpu_device);

    let canvas_desc = WGPUSurfaceDescriptorFromCanvasHTMLSelector {
        .chain = { .sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector },
        .selector = "canvas"
    };

    let surface = make_surface(nullptr, {
        .nextInChain = &canvas_desc.chain
    });

    wgpu_swapchain = make_swap_chain(wgpu_device, surface, {
        .usage  = WGPUTextureUsage_RenderAttachment,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .width  = 800,
        .height = 450,
        .presentMode = WGPUPresentMode_Fifo,
    });

    // compile shaders
    // NOTE: these are now the WGSL shaders (tested with Dawn and Chrome Canary)
    tmp(shader_module, make_wgsl_shader_module(wgpu_device, triangle_wgsl));

    // bind group layout (used by both the wgpu_pipeline layout and uniform bind group, released at the end of this function)
    tmp(bind_group_layout, make_bind_group_layout(wgpu_device, {
        .binding    = 0,
        .visibility = WGPUShaderStage_Vertex,
        .buffer     = {.type = WGPUBufferBindingType_Uniform},
    }));

    tmp(pipeline_layout, make_pipeline_layout(wgpu_device, {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bind_group_layout,
    }));

    // describe buffer layouts
    WGPUVertexAttribute vertex_attributes[2] = {{
        .format = WGPUVertexFormat_Float32x2,
        .offset = 0,
        .shaderLocation = 0,
    },                                          {
        .format = WGPUVertexFormat_Float32x3,
        .offset = 2 * sizeof(float),
        .shaderLocation = 1,
    }};

    let vb_layout = WGPUVertexBufferLayout {
        .arrayStride    = 5 * sizeof(float),
        .attributeCount = 2,
        .attributes     = vertex_attributes,
    };

    // Fragment state
    let blend = WGPUBlendState {
        .color = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_One,
        },
        .alpha = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_One,
        }
    };

    let colorTarget = WGPUColorTargetState {
        .format = WGPUTextureFormat_BGRA8Unorm,
        .blend  = &blend,
        .writeMask = WGPUColorWriteMask_All,
    };

    let fragment = WGPUFragmentState {
        .module = shader_module,
        .entryPoint = "fs_main",
        .targetCount = 1,
        .targets = &colorTarget,
    };

    // Other state
    wgpu_pipeline = make_render_pipeline(wgpu_device, {
        .layout = pipeline_layout,
        .vertex = {
            .module      = shader_module,
            .entryPoint  = "vs_main",
            .bufferCount = 1,//0,
            .buffers     = &vb_layout,
        },
        .primitive = {
            .topology         = WGPUPrimitiveTopology_TriangleList,
            .stripIndexFormat = WGPUIndexFormat_Undefined,
            .frontFace        = WGPUFrontFace_CCW,
            .cullMode         = WGPUCullMode_None,
        },
        .depthStencil = nullptr,
        .multisample = {
            .count = 1,
            .mask = 0xFFFFFFFF,
            .alphaToCoverageEnabled = false,
        },
        .fragment = &fragment,
    });

     vert_buf = make_buffer(wgpu_device, wgpu_queue, vertex_data, sizeof(vertex_data), WGPUBufferUsage_Vertex );
    index_buf = make_buffer(wgpu_device, wgpu_queue,  index_data, sizeof(index_data ), WGPUBufferUsage_Index  );
      uni_buf = make_buffer(wgpu_device, wgpu_queue, &rot_deg   , sizeof(rot_deg    ), WGPUBufferUsage_Uniform);

    bind_group = make_bind_group(wgpu_device, bind_group_layout, {
        .binding = 0,
        .buffer  = uni_buf,
        .offset  = 0,
        .size    = sizeof(rot_deg),
    });
}

/**
 * Draws using the above wgpu_pipeline and buffers.
 */
static EM_BOOL draw(double /*time*/, void*) {
    tmp(back_buf_view, get_current_texture_view(wgpu_swapchain));

    var color_desc = WGPURenderPassColorAttachment {
        .view    = back_buf_view,
        .loadOp  = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
    };
    set_clear_color(color_desc, { .r = 0.3f, .g = 0.3f, .b = 0.3f, .a = 1.0f });

    tmp(encoder, make_command_encoder(wgpu_device));
    tmp(pass   , begin_pass(encoder, color_desc));

    // update the rotation
    rot_deg += 0.1f;
    write_buffer(wgpu_queue, uni_buf, 0, &rot_deg, sizeof(rot_deg));

    // draw the triangle (comment these five lines to simply clear the screen)
    set_pipeline     (pass, wgpu_pipeline);
    set_bind_group   (pass, 0, bind_group, 0, nullptr);
    set_vertex_buffer(pass, 0, vert_buf, 0, sizeof(vertex_data));
    set_index_buffer (pass, index_buf, WGPUIndexFormat_Uint16, 0, sizeof(index_data));
    draw_indexed     (pass, 3, 1, 0, 0, 0);

    end(pass);
    submit(wgpu_queue, encoder);

#ifndef __EMSCRIPTEN__
    /*
     * TODO: wgpuSwapChainPresent is unsupported in Emscripten, so what do we do?
     */
    present(wgpu_swapchain);
#endif

    return true;
}

extern "C" __attribute__((used, visibility("default"))) int entry_point() {
    if ((wgpu_device = emscripten_webgpu_get_device())); else return -1;

    init();
    emscripten_request_animation_frame_loop(draw, nullptr);
    return 0;
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