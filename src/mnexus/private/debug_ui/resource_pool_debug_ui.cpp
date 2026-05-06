// TU header --------------------------------------------
#include "debug_ui/resource_pool_debug_ui.h"

#if MNEXUS_HAVE_DEAR_IMGUI

namespace debug_ui {

ImVec2 PoolTableSize(uint32_t row_count) {
  // Cap visible rows so very populated pools don't overflow the panel.
  uint32_t const visible_rows = row_count < 12u ? row_count : 12u;
  float const height = ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(visible_rows + 1u) + 8.0f;
  return ImVec2 { 0.0f, height };
}

void RenderHandleCell(resource_pool::ResourceHandle handle) {
  ImGui::Text("%u/g%u", handle.index(), handle.generation());
}

void RenderPoolSummaryRow(char const* label, uint32_t live, uint32_t slots) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
  ImGui::TableNextColumn(); ImGui::Text("%u", live);
  ImGui::TableNextColumn(); ImGui::Text("%u", slots);
}

std::string BufferUsageString(mnexus::BufferUsageFlags usage) {
  std::string out;
  auto const append = [&](char const* token) {
    if (!out.empty()) out += '|';
    out += token;
  };
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kUniform))        append("Uniform");
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kStorage))        append("Storage");
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kIndex))          append("Index");
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kVertex))         append("Vertex");
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kIndirect))       append("Indirect");
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kTransferSrc))    append("TransferSrc");
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kTransferDst))    append("TransferDst");
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kVideoDecodeSrc)) append("VideoDecodeSrc");
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kVideoEncodeDst)) append("VideoEncodeDst");
  if (out.empty()) out = "-";
  return out;
}

char const* DimensionToCStr(mnexus::TextureDimension d) {
  switch (d) {
  case mnexus::TextureDimension::k1D:   return "1D";
  case mnexus::TextureDimension::k2D:   return "2D";
  case mnexus::TextureDimension::k3D:   return "3D";
  case mnexus::TextureDimension::kCube: return "Cube";
  }
  return "?";
}

char const* FilterToCStr(mnexus::Filter f) {
  switch (f) {
  case mnexus::Filter::kNearest: return "Nearest";
  case mnexus::Filter::kLinear:  return "Linear";
  }
  return "?";
}

char const* AddressModeToCStr(mnexus::AddressMode m) {
  switch (m) {
  case mnexus::AddressMode::kRepeat:       return "Repeat";
  case mnexus::AddressMode::kMirrorRepeat: return "MirrorRepeat";
  case mnexus::AddressMode::kClampToEdge:  return "ClampToEdge";
  }
  return "?";
}

void WriteBufferDescCells(mnexus::BufferDesc const& desc) {
  ImGui::TableNextColumn(); ImGui::Text("%u", desc.size_in_bytes);
  ImGui::TableNextColumn(); ImGui::TextUnformatted(BufferUsageString(desc.usage).c_str());
}

void WriteTextureDescCells(mnexus::TextureDesc const& desc) {
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(mnexus::ToString(static_cast<MnFormat>(desc.format)).data());
  ImGui::TableNextColumn(); ImGui::TextUnformatted(DimensionToCStr(desc.dimension));
  ImGui::TableNextColumn();
  if (desc.array_layer_count > 1u || desc.mip_level_count > 1u) {
    ImGui::Text("%ux%ux%u (mips=%u, layers=%u)",
      desc.width, desc.height, desc.depth, desc.mip_level_count, desc.array_layer_count);
  } else {
    ImGui::Text("%ux%ux%u", desc.width, desc.height, desc.depth);
  }
  ImGui::TableNextColumn(); ImGui::TextUnformatted(mnexus::ToString(desc.usage).c_str());
}

void WriteSamplerDescCells(mnexus::SamplerDesc const& desc) {
  ImGui::TableNextColumn();
  ImGui::Text("%s/%s", FilterToCStr(desc.min_filter), FilterToCStr(desc.mag_filter));
  ImGui::TableNextColumn(); ImGui::TextUnformatted(FilterToCStr(desc.mipmap_filter));
  ImGui::TableNextColumn();
  ImGui::Text("%s/%s/%s",
    AddressModeToCStr(desc.address_mode_u),
    AddressModeToCStr(desc.address_mode_v),
    AddressModeToCStr(desc.address_mode_w)
  );
}

void WriteShaderReflectionCell(shader::ShaderModuleReflection const& reflection) {
  auto const& groups = reflection.GetBindGroupLayouts();
  if (groups.empty()) {
    ImGui::TextUnformatted("(none)");
    return;
  }
  std::string summary;
  for (auto const& group : groups) {
    for (auto const& entry : group.entries) {
      if (!summary.empty()) summary += ", ";
      summary += '(';
      summary += std::to_string(group.set);
      summary += ',';
      summary += std::to_string(entry.binding);
      summary += ")->";
      summary += mnexus::ToString(entry.type);
    }
  }
  ImGui::TextUnformatted(summary.c_str());
}

void RenderRenderPipelineCacheStats(pipeline::RenderPipelineCacheDiagnostics const& diag) {
  ImGui::BulletText(
    "Render Pipeline Cache: %llu cached, %llu lookups (%llu hits / %llu misses)",
    static_cast<unsigned long long>(diag.cached_pipeline_count),
    static_cast<unsigned long long>(diag.total_lookups),
    static_cast<unsigned long long>(diag.cache_hits),
    static_cast<unsigned long long>(diag.cache_misses)
  );
}

} // namespace debug_ui

#endif // MNEXUS_HAVE_DEAR_IMGUI
