#pragma once

#if MNEXUS_HAVE_DEAR_IMGUI

// c++ headers ------------------------------------------
#include <cstdint>
#include <initializer_list>
#include <string>

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "pipeline/render_pipeline_diagnostics.h"
#include "resource_pool/resource_generational_pool.h"
#include "shader/reflection.h"

// external headers -------------------------------------
#include "imgui.h"

// Backend-agnostic ImGui rendering helpers for the per-backend
// `ResourceStorage::ShowDebugUi()`. The atom helpers and `WriteXDescCells`
// helpers are non-templated and live in `resource_pool_debug_ui.cpp`.
// The section-renderer templates are header-inline; they assume the
// per-backend `Hot` / `Cold` types follow the conventional duck-typed
// shape (e.g. `Cold::desc` for buffers / samplers, `Cold::reflection`
// for shader modules, `Cold::shader_module_handles` for programs).

namespace debug_ui {

inline constexpr ImGuiTableFlags kPoolTableFlags =
    ImGuiTableFlags_RowBg
  | ImGuiTableFlags_BordersInnerV
  | ImGuiTableFlags_SizingStretchProp
  | ImGuiTableFlags_ScrollY;

ImVec2 PoolTableSize(uint32_t row_count);

void RenderHandleCell(resource_pool::ResourceHandle handle);
void RenderPoolSummaryRow(char const* label, uint32_t live, uint32_t slots);

std::string BufferUsageString(mnexus::BufferUsageFlags usage);
char const* DimensionToCStr(mnexus::TextureDimension d);
char const* FilterToCStr(mnexus::Filter f);
char const* AddressModeToCStr(mnexus::AddressMode m);

/// Writes 2 cells: size (uint32 bytes), usage (Uniform|Storage|...).
void WriteBufferDescCells(mnexus::BufferDesc const& desc);

/// Writes 4 cells: format, dimension, extent (with mip/layer suffix when > 1), usage flags.
void WriteTextureDescCells(mnexus::TextureDesc const& desc);

/// Writes 3 cells: min/mag filter pair, mip filter, addressUVW triple.
void WriteSamplerDescCells(mnexus::SamplerDesc const& desc);

/// Writes a single row of cells listing the bind groups / bindings reflected
/// from a SPIRV-Reflect module.
void WriteShaderReflectionCell(shader::ShaderModuleReflection const& reflection);

/// Renders the standard "Render Pipeline Cache" diagnostics line.
void RenderRenderPipelineCacheStats(pipeline::RenderPipelineCacheDiagnostics const& diag);

// ----- Generic pool table -------------------------------------------------

/// Renders a per-pool table. The caller's `row_fn(handle, hot, cold)` writes
/// one cell per column via `ImGui::TableNextColumn()` (TableNextRow is
/// invoked by this helper before each row).
template <class Pool, class RowFn>
void RenderPoolTable(
  char const* table_id,
  std::initializer_list<char const*> column_headers,
  uint32_t live_count,
  Pool const& pool,
  RowFn&& row_fn
) {
  if (!ImGui::BeginTable(table_id, static_cast<int>(column_headers.size()),
                         kPoolTableFlags, PoolTableSize(live_count))) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  for (char const* header : column_headers) {
    ImGui::TableSetupColumn(header);
  }
  ImGui::TableHeadersRow();
  pool.ForEachAliveSharedLocked(
    [&row_fn](resource_pool::ResourceHandle handle, auto const& hot, auto const& cold) {
      ImGui::TableNextRow();
      row_fn(handle, hot, cold);
    }
  );
  ImGui::EndTable();
}

// ----- Section renderers shared across backends ---------------------------

/// 4-column samplers section. `Cold::desc` must be `mnexus::SamplerDesc`.
template <class SamplerPool>
void RenderSamplersSection(SamplerPool const& pool, uint32_t live_count) {
  RenderPoolTable(
    "Samplers", {"Handle", "Min/Mag", "Mip", "AddressUVW"},
    live_count, pool,
    [](resource_pool::ResourceHandle handle, auto const& /*hot*/, auto const& cold) {
      ImGui::TableNextColumn(); RenderHandleCell(handle);
      WriteSamplerDescCells(cold.desc);
    }
  );
}

/// 2-column shader-modules section. `Cold::reflection` must be `shader::ShaderModuleReflection`.
template <class ShaderModulePool>
void RenderShaderModulesSection(ShaderModulePool const& pool, uint32_t live_count) {
  RenderPoolTable(
    "ShaderModules", {"Handle", "Bindings (set,binding) -> kind"},
    live_count, pool,
    [](resource_pool::ResourceHandle handle, auto const& /*hot*/, auto const& cold) {
      ImGui::TableNextColumn(); RenderHandleCell(handle);
      ImGui::TableNextColumn(); WriteShaderReflectionCell(cold.reflection);
    }
  );
}

/// 2-column programs section. `Cold::shader_module_handles` must be a range of `mnexus::ShaderModuleHandle`.
template <class ProgramPool>
void RenderProgramsSection(ProgramPool const& pool, uint32_t live_count) {
  RenderPoolTable(
    "Programs", {"Handle", "Shader Modules"},
    live_count, pool,
    [](resource_pool::ResourceHandle handle, auto const& /*hot*/, auto const& cold) {
      ImGui::TableNextColumn(); RenderHandleCell(handle);
      ImGui::TableNextColumn();
      if (cold.shader_module_handles.empty()) {
        ImGui::TextUnformatted("(none)");
      } else {
        std::string summary;
        for (auto const& sm : cold.shader_module_handles) {
          if (!summary.empty()) summary += ", ";
          auto const sm_handle = resource_pool::ResourceHandle::FromU64(sm.Get());
          summary += std::to_string(sm_handle.index());
          summary += "/g";
          summary += std::to_string(sm_handle.generation());
        }
        ImGui::TextUnformatted(summary.c_str());
      }
    }
  );
}

/// Texture section. `get_desc(cold)` must return `mnexus::TextureDesc const&`
/// (Vulkan wraps the desc behind a variant accessor; WebGPU exposes it as
/// a plain `desc` member).
template <class TexturePool, class GetDesc>
void RenderTexturesSection(
  TexturePool const& pool,
  uint32_t live_count,
  resource_pool::ResourceHandle swapchain_handle,
  GetDesc&& get_desc
) {
  RenderPoolTable(
    "Textures", {"Handle", "Kind", "Format", "Dim", "Extent", "Usage"},
    live_count, pool,
    [swapchain_handle, &get_desc](resource_pool::ResourceHandle handle, auto const& /*hot*/, auto const& cold) {
      ImGui::TableNextColumn(); RenderHandleCell(handle);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(handle == swapchain_handle ? "Swapchain" : "Regular");
      WriteTextureDescCells(get_desc(cold));
    }
  );
}

} // namespace debug_ui

#endif // MNEXUS_HAVE_DEAR_IMGUI
