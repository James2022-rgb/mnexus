// c++ headers ------------------------------------------
#include <array>
#include <cstdio>
#include <vector>

// external headers -------------------------------------
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

#include "mnexus/public/mnexus.h"

// project headers --------------------------------------
#include "triangle_test_vs_spv.h"
#include "triangle_test_fs_spv.h"

int main() {
  mbase::Logger::Initialize();

  constexpr uint32_t kWidth = 256;
  constexpr uint32_t kHeight = 256;
  constexpr uint32_t kBytesPerPixel = 4;
  constexpr uint32_t kBytesPerRow = kWidth * kBytesPerPixel; // 1024, already 256-aligned
  constexpr uint32_t kBufferSize = kBytesPerRow * kHeight;

  // Create headless nexus and device.
  mnexus::INexus* nexus = mnexus::INexus::Create({.headless = true});
  mnexus::IDevice* device = nexus->GetDevice();

  // Create render target texture.
  mnexus::TextureHandle render_target = device->CreateTexture(
    mnexus::TextureDesc {
      .usage = mnexus::TextureUsageFlagBits::kAttachment | mnexus::TextureUsageFlagBits::kTransferSrc,
      .format = MnFormat::kR8G8B8A8_UNORM,
      .dimension = mnexus::TextureDimension::k2D,
      .width = kWidth,
      .height = kHeight,
      .depth = 1,
      .mip_level_count = 1,
      .array_layer_count = 1,
    }
  );

  // Create readback buffer.
  mnexus::BufferHandle readback_buffer = device->CreateBuffer(
    mnexus::BufferDesc {
      .usage = mnexus::BufferUsageFlagBits::kTransferSrc | mnexus::BufferUsageFlagBits::kTransferDst,
      .size_in_bytes = kBufferSize,
    }
  );

  // Create vertex buffer with interleaved float2 position + float3 color.
  struct Vertex {
    float x, y;
    float r, g, b;
  };
  static constexpr Vertex kTriangleVertices[] = {
    {  0.0f,  0.5f,   1.0f, 0.0f, 0.0f }, // top, red
    { -0.5f, -0.5f,   0.0f, 1.0f, 0.0f }, // bottom-left, green
    {  0.5f, -0.5f,   0.0f, 0.0f, 1.0f }, // bottom-right, blue
  };

  mnexus::BufferHandle vertex_buffer = device->CreateBuffer(
    mnexus::BufferDesc {
      .usage = mnexus::BufferUsageFlagBits::kVertex,
      .size_in_bytes = sizeof(kTriangleVertices),
    }
  );
  device->QueueWriteBuffer({}, vertex_buffer, 0, kTriangleVertices, sizeof(kTriangleVertices));

  // Create shader modules from embedded SPIR-V.
  mnexus::ShaderModuleHandle vs_handle = device->CreateShaderModule(
    mnexus::ShaderModuleDesc {
      .source_language = mnexus::ShaderSourceLanguage::kSpirV,
      .code_ptr = reinterpret_cast<uint64_t>(test_shader::kTriangleTestVsSpv),
      .code_size_in_bytes = static_cast<uint32_t>(sizeof(test_shader::kTriangleTestVsSpv)),
    }
  );

  mnexus::ShaderModuleHandle fs_handle = device->CreateShaderModule(
    mnexus::ShaderModuleDesc {
      .source_language = mnexus::ShaderSourceLanguage::kSpirV,
      .code_ptr = reinterpret_cast<uint64_t>(test_shader::kTriangleTestFsSpv),
      .code_size_in_bytes = static_cast<uint32_t>(sizeof(test_shader::kTriangleTestFsSpv)),
    }
  );

  // Create program (vertex + fragment).
  std::array<mnexus::ShaderModuleHandle, 2> shader_modules = { vs_handle, fs_handle };
  mnexus::ProgramHandle program = device->CreateProgram(
    mnexus::ProgramDesc {
      .shader_modules = shader_modules,
    }
  );

  // Render triangle to offscreen texture.
  mnexus::ICommandList* command_list = device->CreateCommandList({});

  mnexus::ClearValue clear_value {};
  clear_value.color.r = 0.392f;
  clear_value.color.g = 0.584f;
  clear_value.color.b = 0.929f;
  clear_value.color.a = 1.0f;

  mnexus::ColorAttachmentDesc color_attachment {
    .texture = render_target,
    .subresource_range = mnexus::TextureSubresourceRange::SingleSubresourceColor(0, 0),
    .load_op = mnexus::LoadOp::kClear,
    .store_op = mnexus::StoreOp::kStore,
    .clear_value = clear_value,
  };

  command_list->BeginRenderPass(
    mnexus::RenderPassDesc {
      .color_attachments = color_attachment,
    }
  );

  mnexus::VertexInputBindingDesc binding {
    .binding = 0,
    .stride = sizeof(Vertex),
    .step_mode = mnexus::VertexStepMode::kVertex,
  };
  std::array<mnexus::VertexInputAttributeDesc, 2> attributes = {{
    { .location = 0, .binding = 0, .format = MnFormat::kR32G32_SFLOAT,    .offset = 0 },
    { .location = 1, .binding = 0, .format = MnFormat::kR32G32B32_SFLOAT, .offset = sizeof(float) * 2 },
  }};

  command_list->BindRenderProgram(program);
  command_list->SetVertexInputLayout(binding, attributes);
  command_list->BindVertexBuffer(0, vertex_buffer, 0);
  command_list->Draw(3, 1, 0, 0);

  command_list->EndRenderPass();

  // Copy render target to readback buffer.
  command_list->CopyTextureToBuffer(
    render_target,
    mnexus::TextureSubresourceRange::SingleSubresourceColor(0, 0),
    readback_buffer,
    0,
    mnexus::Extent3d { kWidth, kHeight, 1 }
  );

  command_list->End();

  device->QueueSubmitCommandList({}, command_list);

  // Read back pixels.
  std::vector<uint8_t> pixels(kBufferSize);
  mnexus::IntraQueueSubmissionId read_id = device->QueueReadBuffer(
    {}, readback_buffer, 0, pixels.data(), kBufferSize
  );
  device->QueueWait({}, read_id);

  // Write PNG.
  char const* const output_path = "triangle.png";
  int const result = stbi_write_png(
    output_path,
    static_cast<int>(kWidth),
    static_cast<int>(kHeight),
    static_cast<int>(kBytesPerPixel),
    pixels.data(),
    static_cast<int>(kBytesPerRow)
  );

  if (result) {
    std::printf("Wrote %s (%ux%u)\n", output_path, kWidth, kHeight);
  } else {
    std::printf("Failed to write %s\n", output_path);
  }

  // Cleanup.
  device->DestroyBuffer(vertex_buffer);
  device->DestroyBuffer(readback_buffer);
  device->DestroyTexture(render_target);
  device->DestroyProgram(program);
  device->DestroyShaderModule(vs_handle);
  device->DestroyShaderModule(fs_handle);

  nexus->Destroy();

  mbase::Logger::Shutdown();
  return result ? 0 : 1;
}
