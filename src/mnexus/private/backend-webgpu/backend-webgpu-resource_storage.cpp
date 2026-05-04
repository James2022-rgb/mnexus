// TU header --------------------------------------------
#include "backend-webgpu/backend-webgpu-command_list.h" // ResourceStorage lives next to the command list.

// project headers --------------------------------------
#if MNEXUS_HAVE_DEAR_IMGUI
# include "debug_ui/resource_pool_debug_ui.h"
#endif

// external headers -------------------------------------
#if MNEXUS_HAVE_DEAR_IMGUI
# include "imgui.h"
#endif

namespace mnexus_backend::webgpu {

void ResourceStorage::ShowDebugUi() const {
#if MNEXUS_HAVE_DEAR_IMGUI
  uint32_t const buffer_live          = buffers.GetLiveCountSharedLocked();
  uint32_t const buffer_slots         = buffers.GetSlotCountSharedLocked();
  uint32_t const texture_live         = textures.GetLiveCountSharedLocked();
  uint32_t const texture_slots        = textures.GetSlotCountSharedLocked();
  uint32_t const sampler_live         = samplers.GetLiveCountSharedLocked();
  uint32_t const sampler_slots        = samplers.GetSlotCountSharedLocked();
  uint32_t const shader_module_live   = shader_modules.GetLiveCountSharedLocked();
  uint32_t const shader_module_slots  = shader_modules.GetSlotCountSharedLocked();
  uint32_t const program_live         = programs.GetLiveCountSharedLocked();
  uint32_t const program_slots        = programs.GetSlotCountSharedLocked();
  uint32_t const compute_pipe_live    = compute_pipelines.GetLiveCountSharedLocked();
  uint32_t const compute_pipe_slots   = compute_pipelines.GetSlotCountSharedLocked();
  uint32_t const render_pipe_live     = render_pipelines.GetLiveCountSharedLocked();
  uint32_t const render_pipe_slots    = render_pipelines.GetSlotCountSharedLocked();

  // Summary table.
  if (ImGui::BeginTable("ResourceStorageSummary", 3,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Pool");
    ImGui::TableSetupColumn("Live");
    ImGui::TableSetupColumn("Slots");
    ImGui::TableHeadersRow();
    debug_ui::RenderPoolSummaryRow("Buffers",          buffer_live,        buffer_slots);
    debug_ui::RenderPoolSummaryRow("Textures",         texture_live,       texture_slots);
    debug_ui::RenderPoolSummaryRow("Samplers",         sampler_live,       sampler_slots);
    debug_ui::RenderPoolSummaryRow("ShaderModules",    shader_module_live, shader_module_slots);
    debug_ui::RenderPoolSummaryRow("Programs",         program_live,       program_slots);
    debug_ui::RenderPoolSummaryRow("ComputePipelines", compute_pipe_live,  compute_pipe_slots);
    debug_ui::RenderPoolSummaryRow("RenderPipelines", render_pipe_live,    render_pipe_slots);
    ImGui::EndTable();
  }

  if (!swapchain_texture_handle.IsNull()) {
    ImGui::Text(
      "Swapchain texture: handle=%u/g%u",
      swapchain_texture_handle.index(),
      swapchain_texture_handle.generation()
    );
  } else {
    ImGui::TextUnformatted("Swapchain texture: (none)");
  }

  ImGui::Spacing();

  // Buffers — WebGPU has no per-Hot mapped flag, so we only render the desc-driven 3 columns.
  if (ImGui::CollapsingHeader("Buffers")) {
    debug_ui::RenderPoolTable(
      "Buffers", {"Handle", "Size (bytes)", "Usage"},
      buffer_live, buffers,
      [](resource_pool::ResourceHandle handle, BufferHot const& /*hot*/, BufferCold const& cold) {
        ImGui::TableNextColumn(); debug_ui::RenderHandleCell(handle);
        debug_ui::WriteBufferDescCells(cold.desc);
      }
    );
  }

  // Textures — WebGPU TextureCold exposes the desc directly.
  if (ImGui::CollapsingHeader("Textures")) {
    debug_ui::RenderTexturesSection(
      textures, texture_live, swapchain_texture_handle,
      [](TextureCold const& cold) -> mnexus::TextureDesc const& { return cold.desc; }
    );
  }

  if (ImGui::CollapsingHeader("Samplers")) {
    debug_ui::RenderSamplersSection(samplers, sampler_live);
  }

  if (ImGui::CollapsingHeader("Shader Modules")) {
    debug_ui::RenderShaderModulesSection(shader_modules, shader_module_live);
  }

  if (ImGui::CollapsingHeader("Programs")) {
    debug_ui::RenderProgramsSection(programs, program_live);
  }

  // Compute pipelines — WebGPU ComputePipelineCold is empty, so we only show the handle.
  if (ImGui::CollapsingHeader("Compute Pipelines")) {
    debug_ui::RenderPoolTable(
      "ComputePipelines", {"Handle"},
      compute_pipe_live, compute_pipelines,
      [](resource_pool::ResourceHandle handle, ComputePipelineHot const& /*hot*/, ComputePipelineCold const& /*cold*/) {
        ImGui::TableNextColumn(); debug_ui::RenderHandleCell(handle);
      }
    );
  }

  // Render pipelines (WebGPU-only resource pool — Vulkan keeps these solely in the cache).
  if (ImGui::CollapsingHeader("Render Pipelines")) {
    debug_ui::RenderPoolTable(
      "RenderPipelines", {"Handle"},
      render_pipe_live, render_pipelines,
      [](resource_pool::ResourceHandle handle, RenderPipelineHot const& /*hot*/, RenderPipelineCold const& /*cold*/) {
        ImGui::TableNextColumn(); debug_ui::RenderHandleCell(handle);
      }
    );
  }

  ImGui::Spacing();
  if (ImGui::CollapsingHeader("Caches", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BulletText("Pipeline Layout Cache: %zu entries", pipeline_layout_cache.size());
    debug_ui::RenderRenderPipelineCacheStats(render_pipeline_cache.GetDiagnostics());
  }
#endif // MNEXUS_HAVE_DEAR_IMGUI
}

} // namespace mnexus_backend::webgpu
