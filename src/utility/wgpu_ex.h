#ifndef FRANCA2_WEB_WGPU_EX_H
#define FRANCA2_WEB_WGPU_EX_H

#include <webgpu/webgpu.h>
#include "syntax.h"
#include "results.h"
#include "arrays.h"
#include "mem_ex.h"
#include "files.h"

namespace wgpu_ex {
    inline WGPUInstance make_instance(const WGPUInstanceDescriptor& descriptor) { return wgpuCreateInstance(&descriptor); }
    inline WGPUProc     get_proc_address(WGPUDevice device, char const * proc_name);

    // Methods of Adapter
    inline size_t enumerate_features(WGPUAdapter adapter, WGPUFeatureName * features);
    inline bool   get_limits        (WGPUAdapter adapter, WGPUSupportedLimits * limits);
    inline void   get_properties    (WGPUAdapter adapter, WGPUAdapterProperties * properties);
    inline bool   has_feature       (WGPUAdapter adapter, WGPUFeatureName feature);
    inline void   request_device    (WGPUAdapter adapter, WGPU_NULLABLE const WGPUDeviceDescriptor& descriptor, WGPURequestDeviceCallback callback, void * userdata);
    inline void   reference         (WGPUAdapter adapter);
    inline void   release           (WGPUAdapter adapter);

    // Methods of Bind_group
    inline void set_label(WGPUBindGroup bind_group, char const * label);
    inline void reference(WGPUBindGroup bind_group);
    inline void release  (WGPUBindGroup bind_group);

    // Methods of BindGroupLayout
    inline void set_label(WGPUBindGroupLayout bind_group_layout, char const * label);
    inline void reference(WGPUBindGroupLayout bind_group_layout);
    inline void release  (WGPUBindGroupLayout& bind_group_layout) { if (bind_group_layout) wgpuBindGroupLayoutRelease(bind_group_layout); bind_group_layout = nullptr; }

    // Methods of Buffer
    inline void const *         get_const_mapped_range(WGPUBuffer buffer, size_t offset, size_t size);
    inline WGPUBufferMapState   get_map_state         (WGPUBuffer buffer);
    inline void *               get_mapped_range      (WGPUBuffer buffer, size_t offset, size_t size);
    inline uint64_t             get_size              (WGPUBuffer buffer) { return wgpuBufferGetSize(buffer); }
    inline WGPUBufferUsageFlags get_usage             (WGPUBuffer buffer);

    inline void map_async(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapCallback callback, void * userdata);
    inline void set_label(WGPUBuffer buffer, char const * label);
    inline void unmap    (WGPUBuffer buffer);
    inline void reference(WGPUBuffer buffer);
    inline void release  (WGPUBuffer buffer);
    inline void destroy  (WGPUBuffer buffer);

    // Methods of CommandBuffer
    inline void set_label(WGPUCommandBuffer command_buffer, char const * label);
    inline void reference(WGPUCommandBuffer command_buffer);
    inline void release  (WGPUCommandBuffer& command_buffer) { if (command_buffer) wgpuCommandBufferRelease(command_buffer); command_buffer = nullptr; }

    // Methods of CommandEncoder
    inline WGPUComputePassEncoder begin_compute_pass(WGPUCommandEncoder command_encoder) { return wgpuCommandEncoderBeginComputePass(command_encoder, nullptr); }
    inline WGPUComputePassEncoder begin_compute_pass(WGPUCommandEncoder command_encoder, const WGPUComputePassDescriptor& descriptor) { return wgpuCommandEncoderBeginComputePass(command_encoder, &descriptor); }
    inline WGPURenderPassEncoder  begin_render_pass (WGPUCommandEncoder command_encoder, const WGPURenderPassDescriptor& descriptor) { return wgpuCommandEncoderBeginRenderPass(command_encoder, &descriptor); }
    inline WGPUCommandBuffer      finish            (WGPUCommandEncoder command_encoder) { return wgpuCommandEncoderFinish(command_encoder, nullptr); }
    inline WGPUCommandBuffer      finish            (WGPUCommandEncoder command_encoder, const WGPUCommandBufferDescriptor& desc) { return wgpuCommandEncoderFinish(command_encoder, (desc.label == nullptr && desc.nextInChain == nullptr) ? nullptr : &desc); }
    inline void clear_buffer           (WGPUCommandEncoder command_encoder, WGPUBuffer buffer, uint64_t offset, uint64_t size);
    inline void copy_buffer_to_buffer  (WGPUCommandEncoder command_encoder, WGPUBuffer source, uint64_t source_offset, WGPUBuffer destination, uint64_t destination_offset, uint64_t size);
    inline void copy_buffer_to_texture (WGPUCommandEncoder command_encoder, const WGPUImageCopyBuffer& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copy_size);
    inline void copy_texture_to_buffer (WGPUCommandEncoder command_encoder, const WGPUImageCopyTexture& source, const WGPUImageCopyBuffer& destination, const WGPUExtent3D& copy_size);
    inline void copy_texture_to_texture(WGPUCommandEncoder command_encoder, const WGPUImageCopyTexture& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copy_size);
    inline void insert_debug_marker    (WGPUCommandEncoder command_encoder, char const * marker_label);
    inline void pop_debug_group        (WGPUCommandEncoder command_encoder);
    inline void push_debug_group       (WGPUCommandEncoder command_encoder, char const * group_label);
    inline void resolve_query_set      (WGPUCommandEncoder command_encoder, WGPUQuerySet query_set, uint32_t first_query, uint32_t query_count, WGPUBuffer destination, uint64_t destination_offset) { wgpuCommandEncoderResolveQuerySet(command_encoder, query_set, first_query, query_count, destination, destination_offset); }
    inline void set_label              (WGPUCommandEncoder command_encoder, char const * label);
    inline void write_timestamp        (WGPUCommandEncoder command_encoder, WGPUQuerySet query_set, uint32_t query_index) { wgpuCommandEncoderWriteTimestamp(command_encoder, query_set, query_index); }
    inline void reference              (WGPUCommandEncoder command_encoder);
    inline void release                (WGPUCommandEncoder& command_encoder) { if (command_encoder) {wgpuCommandEncoderRelease(command_encoder);} command_encoder = nullptr; }

    // Methods of ComputePassEncoder
    inline void begin_pipeline_statistics_query(WGPUComputePassEncoder compute_pass_encoder, WGPUQuerySet query_set, uint32_t query_index);
    inline void dispatch_workgroups            (WGPUComputePassEncoder compute_pass_encoder, uint32_t workgroup_count_x, uint32_t workgroup_count_y = 1, uint32_t workgroup_count_z = 1) { wgpuComputePassEncoderDispatchWorkgroups(compute_pass_encoder, workgroup_count_x, workgroup_count_y, workgroup_count_z); }
    inline void dispatch_workgroups_indirect   (WGPUComputePassEncoder compute_pass_encoder, WGPUBuffer indirect_buffer, uint64_t indirect_offset);
    inline void end                            (WGPUComputePassEncoder compute_pass_encoder) { wgpuComputePassEncoderEnd(compute_pass_encoder); }
    inline void end_pipeline_statistics_query  (WGPUComputePassEncoder compute_pass_encoder);
    inline void insert_debug_marker            (WGPUComputePassEncoder compute_pass_encoder, char const * marker_label);
    inline void pop_debug_group                (WGPUComputePassEncoder compute_pass_encoder);
    inline void push_debug_group               (WGPUComputePassEncoder compute_pass_encoder, char const * group_label);
    inline void set_bind_group                 (WGPUComputePassEncoder compute_pass_encoder, uint32_t group_index, WGPU_NULLABLE WGPUBindGroup group, size_t dynamic_offset_count = 0, uint32_t const * dynamic_offsets = nullptr) { wgpuComputePassEncoderSetBindGroup(compute_pass_encoder, group_index, group, dynamic_offset_count, dynamic_offsets); }
    inline void set_label                      (WGPUComputePassEncoder compute_pass_encoder, char const * label);
    inline void set_pipeline                   (WGPUComputePassEncoder compute_pass_encoder, WGPUComputePipeline pipeline) { wgpuComputePassEncoderSetPipeline(compute_pass_encoder, pipeline); }
    inline void write_timestamp                (WGPUComputePassEncoder compute_pass_encoder, WGPUQuerySet query_set, uint32_t query_index);
    inline void reference                      (WGPUComputePassEncoder compute_pass_encoder);
    inline void release                        (WGPUComputePassEncoder compute_pass_encoder) { if (compute_pass_encoder) { wgpuComputePassEncoderEnd(compute_pass_encoder); wgpuComputePassEncoderRelease(compute_pass_encoder); } compute_pass_encoder = nullptr; }

    // Methods of ComputePipeline
    inline WGPUBindGroupLayout get_bind_group_layout(WGPUComputePipeline compute_pipeline, uint32_t group_index) { return wgpuComputePipelineGetBindGroupLayout(compute_pipeline, group_index); }
    inline void set_label(WGPUComputePipeline compute_pipeline, char const * label);
    inline void reference(WGPUComputePipeline compute_pipeline);
    inline void release  (WGPUComputePipeline compute_pipeline);

    // Methods of Device
    inline WGPUBindGroup           make_bind_group            (WGPUDevice device, const WGPUBindGroupDescriptor& descriptor) { return wgpuDeviceCreateBindGroup(device, &descriptor); }
    inline WGPUBindGroupLayout     make_bind_group_layout     (WGPUDevice device, const WGPUBindGroupLayoutDescriptor& descriptor) { return wgpuDeviceCreateBindGroupLayout(device, &descriptor); }
    inline WGPUBuffer              make_buffer                (WGPUDevice device, const WGPUBufferDescriptor& descriptor) {return wgpuDeviceCreateBuffer(device, &descriptor); }
    inline WGPUCommandEncoder      make_command_encoder       (WGPUDevice device) { return wgpuDeviceCreateCommandEncoder(device, nullptr); }
    inline WGPUCommandEncoder      make_command_encoder       (WGPUDevice device, const WGPUCommandEncoderDescriptor& desc) { return wgpuDeviceCreateCommandEncoder(device, &desc); }
    inline WGPUComputePipeline     make_compute_pipeline      (WGPUDevice device, const WGPUComputePipelineDescriptor& descriptor) { return wgpuDeviceCreateComputePipeline(device, &descriptor); }
    inline void                    make_compute_pipeline_async(WGPUDevice device, const WGPUComputePipelineDescriptor& descriptor, WGPUCreateComputePipelineAsyncCallback callback, void * userdata);
    inline WGPUPipelineLayout      make_pipeline_layout       (WGPUDevice device, const WGPUPipelineLayoutDescriptor& descriptor) { return wgpuDeviceCreatePipelineLayout(device, &descriptor); }
    inline WGPUQuerySet            make_query_set             (WGPUDevice device, const WGPUQuerySetDescriptor& descriptor) { return wgpuDeviceCreateQuerySet(device, &descriptor); }
    inline WGPURenderBundleEncoder make_render_bundle_encoder (WGPUDevice device, const WGPURenderBundleEncoderDescriptor& descriptor);
    inline WGPURenderPipeline      make_render_pipeline       (WGPUDevice device, const WGPURenderPipelineDescriptor& descriptor) { return wgpuDeviceCreateRenderPipeline(device, &descriptor); }
    inline void                    make_render_pipeline_async (WGPUDevice device, const WGPURenderPipelineDescriptor& descriptor, WGPUCreateRenderPipelineAsyncCallback callback, void * userdata);
    inline WGPUSampler             make_sampler               (WGPUDevice device, WGPU_NULLABLE const WGPUSamplerDescriptor& descriptor) { return wgpuDeviceCreateSampler(device, &descriptor); }
    inline WGPUShaderModule        make_shader_module         (WGPUDevice device, const WGPUShaderModuleDescriptor& descriptor) { return wgpuDeviceCreateShaderModule(device, &descriptor); }
    inline WGPUSwapChain           make_swap_chain            (WGPUDevice device, WGPUSurface surface, const WGPUSwapChainDescriptor& descriptor) { return wgpuDeviceCreateSwapChain(device, surface, &descriptor); }
    inline WGPUTexture             make_texture               (WGPUDevice device, const WGPUTextureDescriptor& descriptor) {return wgpuDeviceCreateTexture(device, &descriptor); }

    inline bool      has_feature       (WGPUDevice device, WGPUFeatureName feature);
    inline size_t    enumerate_features(WGPUDevice device, WGPUFeatureName * features);
    inline bool      get_limits        (WGPUDevice device, WGPUSupportedLimits * limits);
    inline WGPUQueue get_queue         (WGPUDevice device) { return wgpuDeviceGetQueue(device); }

    inline void set_uncaptured_error_callback(WGPUDevice device, WGPUErrorCallback callback, void * userdata);
    inline void pop_error_scope (WGPUDevice device, WGPUErrorCallback callback, void * userdata);
    inline void push_error_scope(WGPUDevice device, WGPUErrorFilter filter);
    inline void set_label       (WGPUDevice device, char const * label);
    inline void reference(WGPUDevice device);
    inline void release  (WGPUDevice device);
    inline void destroy  (WGPUDevice device);

    // Methods of Instance
    inline WGPUSurface make_surface(WGPUInstance instance, const WGPUSurfaceDescriptor& descriptor) { return wgpuInstanceCreateSurface(instance, &descriptor); }
    inline void process_events (WGPUInstance instance);
    inline void request_adapter(WGPUInstance instance, WGPU_NULLABLE const WGPURequestAdapterOptions& options, WGPURequestAdapterCallback callback, void * userdata);
    inline void reference      (WGPUInstance& instance);
    inline void release        (WGPUInstance& instance) { if (instance) wgpuInstanceRelease(instance); instance = nullptr; }

    // Methods of PipelineLayout
    inline void set_label(WGPUPipelineLayout pipeline_layout, char const * label);
    inline void reference(WGPUPipelineLayout pipeline_layout);
    inline void release  (WGPUPipelineLayout& pipeline_layout) { if (pipeline_layout) wgpuPipelineLayoutRelease(pipeline_layout); pipeline_layout = nullptr; }

    // Methods of QuerySet
    inline uint32_t      get_count(WGPUQuerySet query_set);
    inline WGPUQueryType get_type (WGPUQuerySet query_set);
    inline void set_label(WGPUQuerySet query_set, char const * label);
    inline void reference(WGPUQuerySet query_set);
    inline void release  (WGPUQuerySet& query_set);
    inline void destroy  (WGPUQuerySet& query_set);

    // Methods of Queue
    inline void on_submitted_work_done(WGPUQueue queue, uint64_t signal_value, WGPUQueueWorkDoneCallback callback, void * userdata);
    inline void set_label    (WGPUQueue queue, char const * label);
    inline void submit       (WGPUQueue queue, WGPUCommandBuffer command_buffer) { wgpuQueueSubmit(queue, 1, &command_buffer); }
    inline void submit       (WGPUQueue queue, size_t command_count, const WGPUCommandBuffer* commands) { wgpuQueueSubmit(queue, command_count, commands); }
    inline void write_buffer (WGPUQueue queue, WGPUBuffer buffer, uint64_t buffer_offset, void const * data, size_t size) { return wgpuQueueWriteBuffer(queue, buffer, buffer_offset, data, size); }
    inline void write_texture(WGPUQueue queue, const WGPUImageCopyTexture& destination, void const * data, size_t data_size, const WGPUTextureDataLayout& data_layout, const WGPUExtent3D& write_size) {
        return wgpuQueueWriteTexture(queue, &destination, data, data_size, &data_layout, &write_size);
    }
    inline void reference    (WGPUQueue queue);
    inline void release      (WGPUQueue& queue) { if (queue) wgpuQueueRelease(queue); queue = nullptr;}

    // Methods of RenderBundle
    inline void set_label(WGPURenderBundle render_bundle, char const * label);
    inline void reference(WGPURenderBundle render_bundle);
    inline void release  (WGPURenderBundle& render_bundle);

    // Methods of RenderBundleEncoder
    inline WGPURenderBundle finish   (WGPURenderBundleEncoder render_bundle_encoder, WGPU_NULLABLE const WGPURenderBundleDescriptor& descriptor);
    inline void draw                 (WGPURenderBundleEncoder render_bundle_encoder, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
    inline void draw_indexed         (WGPURenderBundleEncoder render_bundle_encoder, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance);
    inline void draw_indexed_indirect(WGPURenderBundleEncoder render_bundle_encoder, WGPUBuffer indirect_buffer, uint64_t indirect_offset);
    inline void draw_indirect        (WGPURenderBundleEncoder render_bundle_encoder, WGPUBuffer indirect_buffer, uint64_t indirect_offset);
    inline void insert_debug_marker  (WGPURenderBundleEncoder render_bundle_encoder, char const * marker_label);
    inline void pop_debug_group      (WGPURenderBundleEncoder render_bundle_encoder);
    inline void push_debug_group     (WGPURenderBundleEncoder render_bundle_encoder, char const * group_label);
    inline void set_bind_group       (WGPURenderBundleEncoder render_bundle_encoder, uint32_t group_index, WGPU_NULLABLE WGPUBindGroup group, size_t dynamic_offset_count, uint32_t const * dynamic_offsets);
    inline void set_index_buffer     (WGPURenderBundleEncoder render_bundle_encoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size);
    inline void set_label            (WGPURenderBundleEncoder render_bundle_encoder, char const * label);
    inline void set_pipeline         (WGPURenderBundleEncoder render_bundle_encoder, WGPURenderPipeline pipeline);
    inline void set_vertex_buffer    (WGPURenderBundleEncoder render_bundle_encoder, uint32_t slot, WGPU_NULLABLE WGPUBuffer buffer, uint64_t offset, uint64_t size);
    inline void reference            (WGPURenderBundleEncoder render_bundle_encoder);
    inline void release              (WGPURenderBundleEncoder render_bundle_encoder);

    // Methods of RenderPassEncoder
    inline void begin_occlusion_query          (WGPURenderPassEncoder render_pass_encoder, uint32_t query_index);
    inline void begin_pipeline_statistics_query(WGPURenderPassEncoder render_pass_encoder, WGPUQuerySet query_set, uint32_t queryIndex);
    inline void draw                           (WGPURenderPassEncoder render_pass_encoder, uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0) { wgpuRenderPassEncoderDraw(render_pass_encoder, vertex_count, instance_count, first_vertex, first_instance); }
    inline void draw_indexed                   (WGPURenderPassEncoder render_pass_encoder, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance) { wgpuRenderPassEncoderDrawIndexed(render_pass_encoder, index_count, instance_count, first_index, base_vertex, first_instance); }
    inline void draw_indexed_indirect          (WGPURenderPassEncoder render_pass_encoder, WGPUBuffer indirect_buffer, uint64_t indirect_offset);
    inline void draw_indirect                  (WGPURenderPassEncoder render_pass_encoder, WGPUBuffer indirect_buffer, uint64_t indirect_offset);
    inline void end                            (WGPURenderPassEncoder render_pass_encoder) { wgpuRenderPassEncoderEnd(render_pass_encoder); }
    inline void end_occlusion_query            (WGPURenderPassEncoder render_pass_encoder);
    inline void end_pipeline_statistics_query  (WGPURenderPassEncoder render_pass_encoder);
    inline void execute_bundles                (WGPURenderPassEncoder render_pass_encoder, size_t bundle_count, const WGPURenderBundle& bundles);
    inline void insert_debug_marker            (WGPURenderPassEncoder render_pass_encoder, char const * marker_label);
    inline void pop_debug_group                (WGPURenderPassEncoder render_pass_encoder);
    inline void push_debug_group               (WGPURenderPassEncoder render_pass_encoder, char const * group_label);
    inline void set_bind_group                 (WGPURenderPassEncoder render_pass_encoder, uint32_t group_index, WGPU_NULLABLE WGPUBindGroup group, size_t dynamic_offset_count = 0, uint32_t const * dynamic_offsets = nullptr) { wgpuRenderPassEncoderSetBindGroup(render_pass_encoder, group_index, group, dynamic_offset_count, dynamic_offsets); }
    inline void set_blend_constant             (WGPURenderPassEncoder render_pass_encoder, WGPUColor const * color);
    inline void set_index_buffer               (WGPURenderPassEncoder render_pass_encoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) { wgpuRenderPassEncoderSetIndexBuffer(render_pass_encoder, buffer, format, offset, size); }
    inline void set_label                      (WGPURenderPassEncoder render_pass_encoder, char const * label);
    inline void set_pipeline                   (WGPURenderPassEncoder render_pass_encoder, WGPURenderPipeline pipeline) { wgpuRenderPassEncoderSetPipeline(render_pass_encoder, pipeline); }
    inline void set_scissor_rect               (WGPURenderPassEncoder render_pass_encoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    inline void set_stencil_reference          (WGPURenderPassEncoder render_pass_encoder, uint32_t reference);
    inline void set_vertex_buffer              (WGPURenderPassEncoder render_pass_encoder, uint32_t slot, WGPU_NULLABLE WGPUBuffer buffer, uint64_t offset, uint64_t size) { wgpuRenderPassEncoderSetVertexBuffer(render_pass_encoder, slot, buffer, offset, size); }
    inline void set_viewport                   (WGPURenderPassEncoder render_pass_encoder, float x, float y, float width, float height, float min_depth, float max_depth);
    inline void write_timestamp                (WGPURenderPassEncoder render_pass_encoder, WGPUQuerySet query_set, uint32_t query_index) { wgpuRenderPassEncoderWriteTimestamp(render_pass_encoder, query_set, query_index); }
    inline void reference                      (WGPURenderPassEncoder render_pass_encoder);
    inline void release                        (WGPURenderPassEncoder& render_pass_encoder) { chk(render_pass_encoder) else return; wgpuRenderPassEncoderEnd(render_pass_encoder); wgpuRenderPassEncoderRelease(render_pass_encoder); render_pass_encoder = nullptr; }

    // Methods of RenderPipeline
    inline WGPUBindGroupLayout get_bind_group_layout(WGPURenderPipeline render_pipeline, uint32_t group_index) { return wgpuRenderPipelineGetBindGroupLayout(render_pipeline, group_index); }
    inline void set_label(WGPURenderPipeline render_pipeline, char const * label);
    inline void reference(WGPURenderPipeline render_pipeline);
    inline void release  (WGPURenderPipeline render_pipeline);

    // Methods of Sampler
    inline void set_label(WGPUSampler sampler, char const * label);
    inline void reference(WGPUSampler sampler);
    inline void release  (WGPUSampler& sampler) {  if (sampler) wgpuSamplerRelease(sampler); sampler = nullptr; }

    // Methods of ShaderModule
    inline void get_compilation_info(WGPUShaderModule shader_module, WGPUCompilationInfoCallback callback, void * userdata);
    inline void set_label           (WGPUShaderModule shader_module, char const * label);
    inline void reference           (WGPUShaderModule shader_module);
    inline void release             (WGPUShaderModule& shader_module) { if (shader_module) wgpuShaderModuleRelease(shader_module); shader_module = nullptr; }

    // Methods of Surface
    inline WGPUTextureFormat get_preferred_format(WGPUSurface surface, WGPUAdapter adapter);
    inline void reference(WGPUSurface surface);
    inline void release  (WGPUSurface& surface) { if (surface) wgpuSurfaceRelease(surface); surface = nullptr; }

    // Methods of SwapChain
    inline WGPUTextureView get_current_texture_view(WGPUSwapChain swap_chain) { return wgpuSwapChainGetCurrentTextureView(swap_chain); }
    inline void present  (WGPUSwapChain swap_chain) { wgpuSwapChainPresent(swap_chain); }
    inline void reference(WGPUSwapChain swap_chain);
    inline void release  (WGPUSwapChain& swap_chain) { if (swap_chain) wgpuSwapChainRelease(swap_chain); swap_chain = nullptr; }

    // Methods of Texture
    inline WGPUTextureView       make_view    (WGPUTexture texture, const WGPUTextureViewDescriptor& descriptor) { return wgpuTextureCreateView(texture, &descriptor); }
    inline WGPUTextureDimension  get_dimension(WGPUTexture texture);
    inline WGPUTextureFormat     get_format   (WGPUTexture texture) { return wgpuTextureGetFormat(texture); }
    inline WGPUTextureUsageFlags get_usage    (WGPUTexture texture);
    inline uint32_t get_depth_or_array_layers (WGPUTexture texture);
    inline uint32_t get_width          (WGPUTexture texture) { return wgpuTextureGetWidth (texture); }
    inline uint32_t get_height         (WGPUTexture texture) { return wgpuTextureGetHeight(texture); }
    inline uint32_t get_mip_level_count(WGPUTexture texture);
    inline uint32_t get_sample_count   (WGPUTexture texture);
    inline void set_label(WGPUTexture texture, char const * label);
    inline void reference(WGPUTexture texture);
    inline void release  (WGPUTexture& texture) { if (texture) wgpuTextureRelease(texture); texture = nullptr; }
    inline void destroy  (WGPUTexture texture);

    // Methods of TextureView
    inline void set_label(WGPUTextureView texture_view, char const * label);
    inline void reference(WGPUTextureView texture_view);
    inline void release  (WGPUTextureView& texture_view) { if (texture_view) wgpuTextureViewRelease(texture_view); texture_view = nullptr; }

    // extras

    inline WGPUSurface make_surface_from_html_canvas(WGPUInstance instance, const char *selector) {
        return make_surface(instance, {
            .nextInChain = (WGPUChainedStruct*)&WGPUSurfaceDescriptorFromCanvasHTMLSelector {
                .chain    = {.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector},
                .selector = selector
            }
        });
    }

    inline WGPUColorTargetState make_target(const WGPUBlendState& blend, arenas::arena& arena = arenas::gta) { return {
        .format    = WGPUTextureFormat_BGRA8Unorm,
        .blend     = alloc_one(arena, blend),
        .writeMask = WGPUColorWriteMask_All,
    };}

    inline WGPUColorTargetState make_target() { return {
        .format    = WGPUTextureFormat_BGRA8Unorm,
        .writeMask = WGPUColorWriteMask_All,
    };}

    inline WGPUVertexState make_vertex_state(WGPUShaderModule shader_module, const char* entry_point) { return {
        .module = shader_module,
        .entryPoint = entry_point
    };}

    inline WGPUFragmentState make_fragment_state(WGPUShaderModule shader_module, const char* entry_point, const WGPUBlendState& blend, arenas::arena& arena = arenas::gta) { return {
        .module      = shader_module, .entryPoint  = entry_point,
        .targetCount = 1,
        .targets     = alloc_one(arena, make_target(blend, arena)),
    };}

    inline WGPUBindGroup make_single_entry_bind_group(WGPUDevice device, WGPUBindGroupLayout layout, const WGPUBindGroupEntry& descriptor) {
        return make_bind_group(device, {
            .layout = layout,
            .entryCount = 1,
            .entries = &descriptor,
        });
    }

    inline WGPURenderPipeline make_render_pipeline(WGPUDevice device, const WGPUVertexState& vertex_state, const WGPUFragmentState& fragment_state) {
        return make_render_pipeline(device, {
            .vertex      = vertex_state,
            .primitive   = { .topology = WGPUPrimitiveTopology_TriangleList },
            .multisample = { .count    = 1, .mask = 0xFFFFFFFF },
            .fragment    = &fragment_state,
        });
    }


    struct BindGroupEntry {
        WGPUBuffer      buffer;
        uint64_t        offset;
        WGPUSampler     sampler;
        WGPUTextureView texture_view;
    };

    inline WGPUBindGroup make_bind_group(WGPUDevice device, WGPUBindGroupLayout layout, std::initializer_list<BindGroupEntry> entries) {
        let count    = entries.size();
        var iterator = entries.begin();
        var wgpu_entries = stackalloc(count, WGPUBindGroupEntry);
        var entries_i = 0;
        for (size_t i = 0; i < count; ++i) {
            if (iterator->buffer) {
                wgpu_entries[entries_i] = {
                    .binding = i,
                    .buffer = iterator->buffer,
                    .offset = iterator->offset,
                    .size   = get_size(iterator->buffer)
                };
            }
            else if (iterator->sampler) {
                wgpu_entries[entries_i] = {
                    .binding = i,
                    .sampler = iterator->sampler
                };
            }
            else if (iterator->texture_view) {
                wgpu_entries[entries_i] = {
                    .binding = i,
                    .textureView = iterator->texture_view
                };
            }
            else {
                entries_i--;
            }

            entries_i++;
            iterator++;
        }

        return make_bind_group(device, {
            .layout     = layout,
            .entryCount = (uint)entries_i,
            .entries    = wgpu_entries
        });
    }

    inline WGPUBindGroup make_bind_group(WGPUDevice device, WGPURenderPipeline pipeline, std::initializer_list<BindGroupEntry> entries) {
        return make_bind_group(device, get_bind_group_layout(pipeline, 0), entries);
    }

    inline WGPUBindGroup make_bind_group(WGPUDevice device, WGPUComputePipeline pipeline, std::initializer_list<BindGroupEntry> entries) {
        return make_bind_group(device, get_bind_group_layout(pipeline, 0), entries);
    }

    inline WGPUBindGroupLayout make_bind_group_layout(WGPUDevice device, const WGPUBindGroupLayoutEntry& entry) {
        return make_bind_group_layout(device, {
            .entryCount = 1,
            .entries    = &entry,
        });
    }

    inline WGPUShaderModule make_spirv_shader(WGPUDevice device, const uint32_t* code, uint32_t size, const char* label = nullptr) {
        let desc = WGPUShaderModuleSPIRVDescriptor {
            .chain    = { .sType = WGPUSType_ShaderModuleSPIRVDescriptor },
            .codeSize = size / sizeof(uint32_t),
            .code     = code,
        };

        return make_shader_module(device, {
            .nextInChain = &desc.chain,
            .label = label,
        });
    }

    inline WGPURenderPassEncoder begin_render_pass(WGPUCommandEncoder command_encoder, const WGPURenderPassColorAttachment& attachment) {
        return begin_render_pass(command_encoder, {
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
        });
    }

    inline WGPUShaderModule make_wgsl_shader_module(WGPUDevice device, const char* const code, const char* label = nullptr) {
        let wgsl = WGPUShaderModuleWGSLDescriptor {
            .chain = { .sType = WGPUSType_ShaderModuleWGSLDescriptor },
            .code = code,
        };
        return make_shader_module(device, {
            .nextInChain = &wgsl.chain,
            .label = label,
        });
    }

    inline WGPUShaderModule make_wgsl_shader_module_from_file(WGPUDevice device, const char* const path, const char* label = nullptr, arenas::arena& arena = arenas::gta) {
        let code = read_file_as_string(path, arena);
        let wgsl = WGPUShaderModuleWGSLDescriptor {
            .chain = { .sType = WGPUSType_ShaderModuleWGSLDescriptor },
            .code = code.data,
        };
        return make_shader_module(device, {
            .nextInChain = &wgsl.chain,
            .label = label,
        });
    }

    inline WGPUBuffer make_buffer(WGPUDevice device, WGPUBufferUsage usage, size_t size) {
        return make_buffer(device, {
            .usage = usage,
            .size  = size,
        });
    }

    tt WGPUBuffer make_buffer(WGPUDevice device, WGPUBufferUsage usage) {
        return make_buffer(device, {
            .usage = usage,
            .size  = sizeof(t),
        });
    }

    tt WGPUBuffer make_buffer(WGPUDevice device) {
        return make_buffer<t> (device, (WGPUBufferUsage) (WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform));
    }

    inline WGPUBuffer make_buffer(WGPUDevice device, WGPUQueue queue, WGPUBufferUsage usage, const void* data, size_t size) {
        let buffer = make_buffer(device, {
            .usage = (WGPUBufferUsageFlags) WGPUBufferUsage_CopyDst | usage,
            .size  = size,
        });
        write_buffer(queue, buffer, 0, data, size);
        return buffer;
    }

    tt WGPUBuffer make_buffer(WGPUDevice device, WGPUQueue queue, WGPUBufferUsage usage, const t& data) { return make_buffer(device, queue, usage, &data, sizeof(t)); }
    tt void write_buffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t buffer_offset, const t& data) { return write_buffer(queue, buffer, buffer_offset, &data, sizeof(t)); }
    tt void write_buffer(WGPUQueue queue, WGPUBuffer buffer, const t& data) { return write_buffer(queue, buffer, 0, &data, sizeof(t)); }

    inline void submit(WGPUQueue queue, WGPUCommandEncoder command_encoder) {
        tmp(commands, finish(command_encoder));
        submit(queue, commands);
    }

    inline void set_bind_group(WGPURenderPassEncoder   render_pass_encoder, WGPUBindGroup group) { set_bind_group( render_pass_encoder, 0, group); }
    inline void set_bind_group(WGPUComputePassEncoder compute_pass_encoder, WGPUBindGroup group) { set_bind_group(compute_pass_encoder, 0, group); }

    struct BoundRenderPipeline {
        WGPURenderPipeline pipeline;
        WGPUBindGroup    bind_group;
    };

    inline void set_pipeline(WGPURenderPassEncoder pass, const BoundRenderPipeline& pipeline) {
        set_pipeline  (pass, pipeline.pipeline  );
        set_bind_group(pass, pipeline.bind_group);
    }

    struct BoundComputePipeline {
        WGPUComputePipeline pipeline;
        WGPUBindGroup    bind_group;
    };

    inline void set_pipeline(WGPUComputePassEncoder pass, const BoundComputePipeline& pipeline) {
        set_pipeline  (pass, pipeline.pipeline  );
        set_bind_group(pass, pipeline.bind_group);
    }

    inline WGPUComputePassEncoder begin_compute_pass(WGPUCommandEncoder encoder, BoundComputePipeline pipeline) {
        let pass = begin_compute_pass(encoder);
        set_pipeline(pass, pipeline);
        return pass;
    }

    inline void dispatch_compute_pass(WGPUCommandEncoder encoder, BoundComputePipeline pipeline, uint32_t workgroup_count_x, uint32_t workgroup_count_y = 1, uint32_t workgroup_count_z = 1) {
        { tmp(pass, begin_compute_pass(encoder, pipeline));
            dispatch_workgroups(pass, workgroup_count_x, workgroup_count_y, workgroup_count_z);
        }
    }
    inline WGPURenderPassEncoder begin_render_pass(WGPUCommandEncoder command_encoder, BoundRenderPipeline pipeline, const WGPURenderPassColorAttachment& attachment) {
        let pass = begin_render_pass(command_encoder, {
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
        });

        set_pipeline(pass, pipeline);
        return pass;
    }


    inline void draw_fullscreen_pass(WGPUCommandEncoder encoder, WGPUSwapChain swapchain, BoundRenderPipeline pipeline, WGPUColor clear_color) {
        tmp(back_buf_view, get_current_texture_view(swapchain));
        tmp(pass, begin_render_pass(encoder, pipeline, {
            .view       = back_buf_view,
            . loadOp    = WGPULoadOp_Clear,
            .storeOp    = WGPUStoreOp_Store,
            .clearValue = clear_color,
        }));

        draw(pass, 3);
    }

    struct SinglePassCommandEncoder {
        WGPUCommandEncoder    encoder;
        WGPURenderPassEncoder pass;
        WGPUTextureView       view;

        WGPUQueue queue;
    };

    inline SinglePassCommandEncoder run_single_pass_encoder(WGPUDevice device, WGPUSwapChain swap_chain, WGPUQueue queue, BoundRenderPipeline pipeline, const WGPURenderPassColorAttachment& attachment, const WGPUCommandEncoderDescriptor& desc = {}) {
        let back_buf_view = get_current_texture_view(swap_chain);

        var att_copy = attachment;
        att_copy.view = back_buf_view;
        if (!att_copy. loadOp) att_copy. loadOp = WGPULoadOp_Clear;
        if (!att_copy.storeOp) att_copy.storeOp = WGPUStoreOp_Store;

        let encoder = make_command_encoder(device, desc);
        let pass    = begin_render_pass(encoder, att_copy);

        set_pipeline(pass, pipeline);

        return {encoder, pass, back_buf_view, queue};
    }

    inline void release(SinglePassCommandEncoder& encoder) {
        end   (encoder.pass);
        submit(encoder.queue, encoder.encoder);

        release(encoder.pass   );
        release(encoder.encoder);
        release(encoder.view   );
    }

    inline WGPUSampler make_wrap_sampler(WGPUDevice device) { return make_sampler(device, {
        .magFilter     = WGPUFilterMode_Linear,
        .minFilter     = WGPUFilterMode_Linear,
        .mipmapFilter  = WGPUMipmapFilterMode_Nearest,
        .lodMaxClamp   = 1.0f,
        .maxAnisotropy = 1,
    });}

    inline int get_bytes_per_pixel(WGPUTextureFormat format) {
        switch (format) {
            case WGPUTextureFormat_R8Unorm:
            case WGPUTextureFormat_R8Uint : return 1;
            case WGPUTextureFormat_R32Uint: return 4;
            default: return 0; // unsupported
        }
    }

    inline void write_texture(WGPUQueue queue, WGPUTexture texture, void const * data) {
        let f = get_format(texture);
        let bytes_per_pixel = get_bytes_per_pixel(f);
        assert(bytes_per_pixel);

        let w = get_width (texture);
        let h = get_height(texture);
        write_texture(queue, {.texture = texture}, data, w * h * bytes_per_pixel, {.bytesPerRow = w * bytes_per_pixel, .rowsPerImage = h,}, { w, h, 1});
    }

    inline ret2<WGPUTexture, WGPUTextureView> make_texture_2d(WGPUDevice device, const usize2& size, WGPUTextureFormat format, const void* data = nullptr) {
        let extent = WGPUExtent3D {size.w, size.h, 1 };
        var texture = make_texture(device, {
            .usage         = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
            .dimension     = WGPUTextureDimension_2D,
            .size          = extent,
            .format        = format,
            .mipLevelCount = 1,
            .sampleCount   = 1,
        });

        if (data) {
            tmp(queue, get_queue(device));
            write_texture(queue, texture, data);
        }

        let view = make_view(texture, {
            .format          = format,
            .dimension       = WGPUTextureViewDimension_2D,
            .mipLevelCount   = 1,
            .arrayLayerCount = 1,
        });

        return ret2_ok(texture, view);
    }

    inline WGPUBlendState alpha_blend_state() { return {
        .color = {
            .srcFactor = WGPUBlendFactor_SrcAlpha,
            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
        },
        .alpha = {
            .srcFactor = WGPUBlendFactor_Zero,
            .dstFactor = WGPUBlendFactor_One
        }
    };}

    inline WGPUBlendState premul_alpha_blend_state() { return {
        .color = {
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
        },
        .alpha = {
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha
        }
    };}
}

#endif //FRANCA2_WEB_WGPU_EX_H
