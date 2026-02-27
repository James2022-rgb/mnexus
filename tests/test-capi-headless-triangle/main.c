/* c headers -------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>

/* public project headers ------------------------------- */
#include "mnexus/public/mnexus.h"

/* project headers -------------------------------------- */
#include "triangle_test_vs_spv.h"
#include "triangle_test_fs_spv.h"

/* test harness ----------------------------------------- */
#include "mnexus_test_harness.h"

int MnTestMain(int argc, char** argv) {
  (void)argc;
  (void)argv;

  uint32_t const kWidth = 256;
  uint32_t const kHeight = 256;
  uint32_t const kBytesPerPixel = 4;
  uint32_t const kBytesPerRow = kWidth * kBytesPerPixel; /* 1024, already 256-aligned */
  uint32_t const kBufferSize = kBytesPerRow * kHeight;

  /* Create headless nexus and device. */
  MnNexusDesc nexus_desc = { 0 };
  nexus_desc.headless = MnBoolTrue;
  MnNexus nexus = MnNexusCreate(&nexus_desc);
  MnDevice device = MnNexusGetDevice(nexus);

  /* Create render target texture. */
  MnTextureDesc tex_desc = { 0 };
  tex_desc.usage = MnTextureUsageFlagBitAttachment | MnTextureUsageFlagBitTransferSrc;
  tex_desc.format = MnFormatR8G8B8A8_UNORM;
  tex_desc.dimension = MnTextureDimension2D;
  tex_desc.width = kWidth;
  tex_desc.height = kHeight;
  tex_desc.depth = 1;
  tex_desc.mip_level_count = 1;
  tex_desc.array_layer_count = 1;
  MnResourceHandle render_target = MnDeviceCreateTexture(device, &tex_desc);

  /* Create readback buffer. */
  MnBufferDesc readback_buf_desc = { 0 };
  readback_buf_desc.usage = MnBufferUsageFlagBitTransferSrc | MnBufferUsageFlagBitTransferDst;
  readback_buf_desc.size_in_bytes = kBufferSize;
  MnResourceHandle readback_buffer = MnDeviceCreateBuffer(device, &readback_buf_desc);

  /* Create vertex buffer with interleaved float2 position + float3 color. */
  typedef struct Vertex {
    float x, y;
    float r, g, b;
  } Vertex;
  static const Vertex kTriangleVertices[] = {
    {  0.0f,  0.5f,   1.0f, 0.0f, 0.0f }, /* top, red */
    { -0.5f, -0.5f,   0.0f, 1.0f, 0.0f }, /* bottom-left, green */
    {  0.5f, -0.5f,   0.0f, 0.0f, 1.0f }, /* bottom-right, blue */
  };

  MnBufferDesc vb_desc = { 0 };
  vb_desc.usage = MnBufferUsageFlagBitVertex;
  vb_desc.size_in_bytes = sizeof(kTriangleVertices);
  MnResourceHandle vertex_buffer = MnDeviceCreateBuffer(device, &vb_desc);

  MnQueueId default_queue = { 0 };
  MnDeviceQueueWriteBuffer(device, &default_queue,
    vertex_buffer, 0, kTriangleVertices, sizeof(kTriangleVertices));

  /* Create shader modules from embedded SPIR-V. */
  MnShaderModuleDesc vs_desc = { 0 };
  vs_desc.source_language = MnShaderSourceLanguageSpirV;
  vs_desc.code_ptr = (uint64_t)(uintptr_t)kTriangleTestVsSpv;
  vs_desc.code_size_in_bytes = (uint32_t)sizeof(kTriangleTestVsSpv);
  MnResourceHandle vs_handle = MnDeviceCreateShaderModule(device, &vs_desc);

  MnShaderModuleDesc fs_desc = { 0 };
  fs_desc.source_language = MnShaderSourceLanguageSpirV;
  fs_desc.code_ptr = (uint64_t)(uintptr_t)kTriangleTestFsSpv;
  fs_desc.code_size_in_bytes = (uint32_t)sizeof(kTriangleTestFsSpv);
  MnResourceHandle fs_handle = MnDeviceCreateShaderModule(device, &fs_desc);

  /* Create program (vertex + fragment). */
  MnResourceHandle shader_modules[2] = { vs_handle, fs_handle };
  MnProgramDesc prog_desc = { 0 };
  prog_desc.shader_modules = shader_modules;
  prog_desc.shader_module_count = 2;
  MnResourceHandle program = MnDeviceCreateProgram(device, &prog_desc);

  /* Render triangle to offscreen texture. */
  MnCommandListDesc cl_desc = { 0 };
  MnCommandList command_list = MnDeviceCreateCommandList(device, &cl_desc);

  MnClearValue clear_value = { 0 };
  clear_value.color.r = 0.392f;
  clear_value.color.g = 0.584f;
  clear_value.color.b = 0.929f;
  clear_value.color.a = 1.0f;

  MnTextureSubresourceRange single_color = { 0 };
  single_color.aspect_mask = MnTextureAspectFlagBitColor;
  single_color.base_mip_level = 0;
  single_color.mip_level_count = 1;
  single_color.base_array_layer = 0;
  single_color.array_layer_count = 1;

  MnColorAttachmentDesc color_attachment = { 0 };
  color_attachment.texture = render_target;
  color_attachment.subresource_range = single_color;
  color_attachment.load_op = MnLoadOpClear;
  color_attachment.store_op = MnStoreOpStore;
  color_attachment.clear_value = clear_value;

  MnRenderPassDesc rp_desc = { 0 };
  rp_desc.color_attachment_count = 1;
  rp_desc.color_attachments = &color_attachment;

  MnCommandListBeginRenderPass(command_list, &rp_desc);

  MnVertexInputBindingDesc binding = { 0 };
  binding.binding = 0;
  binding.stride = sizeof(Vertex);
  binding.step_mode = MnVertexStepModeVertex;

  MnVertexInputAttributeDesc attributes[2] = { { 0 }, { 0 } };
  attributes[0].location = 0;
  attributes[0].binding = 0;
  attributes[0].format = MnFormatR32G32_SFLOAT;
  attributes[0].offset = 0;
  attributes[1].location = 1;
  attributes[1].binding = 0;
  attributes[1].format = MnFormatR32G32B32_SFLOAT;
  attributes[1].offset = sizeof(float) * 2;

  MnCommandListBindRenderProgram(command_list, program);
  MnCommandListSetVertexInputLayout(command_list, &binding, 1, attributes, 2);
  MnCommandListBindVertexBuffer(command_list, 0, vertex_buffer, 0);
  MnCommandListDraw(command_list, 3, 1, 0, 0);

  MnCommandListEndRenderPass(command_list);

  /* Copy render target to readback buffer. */
  MnExtent3d copy_extent = { 0 };
  copy_extent.width = kWidth;
  copy_extent.height = kHeight;
  copy_extent.depth = 1;
  MnCommandListCopyTextureToBuffer(command_list,
    render_target, &single_color,
    readback_buffer, 0, &copy_extent);

  MnCommandListEnd(command_list);

  MnDeviceQueueSubmitCommandList(device, &default_queue, command_list);

  /* Read back pixels. */
  uint8_t* pixels = (uint8_t*)malloc(kBufferSize);
  MnIntraQueueSubmissionId read_id = MnDeviceQueueReadBuffer(
    device, &default_queue, readback_buffer, 0, pixels, kBufferSize);
  MnDeviceQueueWaitIdle(device, &default_queue, read_id);

  /* Write PNG. */
  char const* const output_path = "triangle.png";
  int const result = MnTestWritePng(
    output_path,
    (int)kWidth,
    (int)kHeight,
    (int)kBytesPerPixel,
    pixels,
    (int)kBytesPerRow);

  if (result) {
    printf("Wrote %s (%ux%u)\n", output_path, kWidth, kHeight);
  } else {
    printf("Failed to write %s\n", output_path);
  }

  free(pixels);

  /* Cleanup. */
  MnDeviceDestroyBuffer(device, vertex_buffer);
  MnDeviceDestroyBuffer(device, readback_buffer);
  MnDeviceDestroyTexture(device, render_target);
  MnDeviceDestroyProgram(device, program);
  MnDeviceDestroyShaderModule(device, vs_handle);
  MnDeviceDestroyShaderModule(device, fs_handle);

  MnNexusDestroy(nexus);

  return result ? 0 : 1;
}
