// TU header --------------------------------------------
#include "backend-vulkan/resource/resource_storage.h"

// project headers --------------------------------------
#if MNEXUS_HAVE_DEAR_IMGUI
# include "debug_ui/resource_pool_debug_ui.h"
#endif

// external headers -------------------------------------
#if MNEXUS_HAVE_DEAR_IMGUI
# include "imgui.h"
#endif

namespace mnexus_backend::vulkan {

void ResourceStorage::StampResourceUse(resource_pool::ResourceHandle handle, uint32_t queue_compact_index, uint64_t serial) {
  switch (handle.resource_type()) {
  case mnexus::kResourceTypeBuffer: {
    auto& hot = buffers.LockSharedAndGetRefHot(handle);
    hot.Stamp(queue_compact_index, serial);
    buffers.UnlockShared();
    break;
  }
  case mnexus::kResourceTypeTexture: {
    auto& hot = textures.LockSharedAndGetRefHot(handle);
    hot.Stamp(queue_compact_index, serial);
    textures.UnlockShared();
    break;
  }
  case mnexus::kResourceTypeShaderModule: {
    auto& hot = shader_modules.LockSharedAndGetRefHot(handle);
    hot.Stamp(queue_compact_index, serial);
    shader_modules.UnlockShared();
    break;
  }
  case mnexus::kResourceTypeProgram: {
    auto& hot = programs.LockSharedAndGetRefHot(handle);
    hot.Stamp(queue_compact_index, serial);
    programs.UnlockShared();
    break;
  }
  case mnexus::kResourceTypeComputePipeline: {
    auto& hot = compute_pipelines.LockSharedAndGetRefHot(handle);
    hot.Stamp(queue_compact_index, serial);
    compute_pipelines.UnlockShared();
    break;
  }
  case mnexus::kResourceTypeSampler: {
    auto& hot = samplers.LockSharedAndGetRefHot(handle);
    hot.Stamp(queue_compact_index, serial);
    samplers.UnlockShared();
    break;
  }
  case mnexus::kResourceTypeQueryPool: {
    // Query pools have no timeline tracking (the public read API is
    // non-blocking), so this is a deliberate no-op.
    break;
  }
#if MNEXUS_ENABLE_VIDEO_CODING
  case mnexus::kResourceTypeVideoSession: {
    auto& hot = video_sessions.LockSharedAndGetRefHot(handle);
    hot.Stamp(queue_compact_index, serial);
    video_sessions.UnlockShared();
    break;
  }
  case mnexus::kResourceTypeVideoSessionParameters: {
    auto& hot = video_session_parameters.LockSharedAndGetRefHot(handle);
    hot.Stamp(queue_compact_index, serial);
    video_session_parameters.UnlockShared();
    break;
  }
#endif
  default:
    MBASE_ASSERT_MSG(false, "StampResourceUse: unhandled resource type {}", handle.resource_type());
    break;
  }
}

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

  // Buffers — Vulkan-specific 4th column shows VMA-mapped status.
  if (ImGui::CollapsingHeader("Buffers")) {
    debug_ui::RenderPoolTable(
      "Buffers", {"Handle", "Size (bytes)", "Usage", "Mapped"},
      buffer_live, buffers,
      [](resource_pool::ResourceHandle handle, BufferHot const& hot, BufferCold const& cold) {
        ImGui::TableNextColumn(); debug_ui::RenderHandleCell(handle);
        debug_ui::WriteBufferDescCells(cold.desc);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(hot.mapped_data ? "yes" : "no");
      }
    );
  }

  // Textures — Vulkan TextureCold wraps the desc behind GetTextureDesc().
  if (ImGui::CollapsingHeader("Textures")) {
    debug_ui::RenderTexturesSection(
      textures, texture_live, swapchain_texture_handle,
      [](TextureCold const& cold) -> mnexus::TextureDesc const& {
        return cold.GetTextureDesc();
      }
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

  // Compute pipelines — Vulkan stores program/shader-module handles in cold.
  if (ImGui::CollapsingHeader("Compute Pipelines")) {
    debug_ui::RenderPoolTable(
      "ComputePipelines", {"Handle", "Program"},
      compute_pipe_live, compute_pipelines,
      [](resource_pool::ResourceHandle handle, ComputePipelineHot const& /*hot*/, ComputePipelineCold const& cold) {
        ImGui::TableNextColumn(); debug_ui::RenderHandleCell(handle);
        ImGui::TableNextColumn();
        auto const program_handle = resource_pool::ResourceHandle::FromU64(cold.program_handle().Get());
        ImGui::Text("%u/g%u", program_handle.index(), program_handle.generation());
      }
    );
  }

  ImGui::Spacing();
  if (ImGui::CollapsingHeader("Caches", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BulletText("Pipeline Layout Cache: %zu entries", pipeline_layout_cache.size());
    debug_ui::RenderRenderPipelineCacheStats(render_pipeline_cache.GetDiagnostics());
    ImGui::BulletText("Image View Cache: %zu entries", image_view_cache.size());
  }
#endif // MNEXUS_HAVE_DEAR_IMGUI
}

} // namespace mnexus_backend::vulkan
