#pragma once

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "binding/state_tracker.h"

#include "container/generational_pool.h"

#include "backend-webgpu/include_dawn.h"
#include "backend-webgpu/backend-webgpu-buffer.h"
#include "backend-webgpu/backend-webgpu-texture.h"
#include "backend-webgpu/backend-webgpu-sampler.h"
#include "backend-webgpu/types_bridge.h"

namespace mnexus_backend::webgpu {

/// Resolves dirty bind groups from the state tracker and sets them on the given pass encoder.
/// Works with both `wgpu::ComputePassEncoder` and `wgpu::RenderPassEncoder`.
///
/// - `TPassEncoder`: `wgpu::ComputePassEncoder` or `wgpu::RenderPassEncoder`
/// - `TPipeline`: `wgpu::ComputePipeline` or `wgpu::RenderPipeline`
template<typename TPassEncoder, typename TPipeline>
void ResolveAndSetBindGroups(
  wgpu::Device const& wgpu_device,
  TPassEncoder& pass,
  TPipeline const& pipeline,
  binding::BindGroupStateTracker& state_tracker,
  BufferResourcePool const& buffer_pool,
  TextureResourcePool const& texture_pool,
  SamplerResourcePool const& sampler_pool
) {
  for (uint32_t group = 0; group < 4; ++group) {
    if (!state_tracker.IsGroupDirty(group)) {
      continue;
    }

    mbase::ArrayProxy<binding::BoundEntry const> entries = state_tracker.GetGroupEntries(group);
    if (entries.empty()) {
      state_tracker.MarkGroupClean(group);
      continue;
    }

    wgpu::BindGroupLayout layout = pipeline.GetBindGroupLayout(group);

    mbase::SmallVector<wgpu::BindGroupEntry, 4> wgpu_entries;
    wgpu_entries.reserve(entries.size());

    for (auto const& entry : entries) {
      wgpu::BindGroupEntry wgpu_entry {};
      wgpu_entry.binding = entry.binding;

      switch (entry.type) {
      case mnexus::BindGroupLayoutEntryType::kUniformBuffer:
      case mnexus::BindGroupLayoutEntryType::kStorageBuffer: {
        auto pool_handle = container::ResourceHandle::FromU64(entry.buffer.buffer.Get());
        auto [hot, lock] = buffer_pool.GetHotConstRefWithSharedLockGuard(pool_handle);
        wgpu_entry.buffer = hot.wgpu_buffer;
        wgpu_entry.offset = entry.buffer.offset;
        wgpu_entry.size = entry.buffer.size;
        break;
      }
      case mnexus::BindGroupLayoutEntryType::kSampledTexture: {
        auto pool_handle = container::ResourceHandle::FromU64(entry.texture.texture.Get());
        auto [hot, cold, lock] = texture_pool.GetConstRefWithSharedLockGuard(pool_handle);
        wgpu::TextureFormat wgpu_format = ToWgpuTextureFormat(cold.desc.format);
        wgpu::TextureViewDescriptor view_desc = MakeWgpuTextureViewDesc(
          wgpu_format,
          wgpu::TextureViewDimension::e2D,
          entry.texture.subresource_range,
          wgpu::TextureAspect::All
        );
        wgpu_entry.textureView = hot.wgpu_texture.CreateView(&view_desc);
        break;
      }
      case mnexus::BindGroupLayoutEntryType::kSampler: {
        auto pool_handle = container::ResourceHandle::FromU64(entry.sampler.sampler.Get());
        auto [hot, lock] = sampler_pool.GetHotConstRefWithSharedLockGuard(pool_handle);
        wgpu_entry.sampler = hot.wgpu_sampler;
        break;
      }
      default:
        MBASE_ASSERT_MSG(false, "Unsupported BindGroupLayoutEntryType in ResolveAndSetBindGroups");
        break;
      }

      wgpu_entries.emplace_back(wgpu_entry);
    }

    wgpu::BindGroupDescriptor bind_group_desc {};
    bind_group_desc.layout = layout;
    bind_group_desc.entryCount = static_cast<uint32_t>(wgpu_entries.size());
    bind_group_desc.entries = wgpu_entries.data();

    wgpu::BindGroup bind_group = wgpu_device.CreateBindGroup(&bind_group_desc);
    pass.SetBindGroup(group, bind_group);

    state_tracker.MarkGroupClean(group);
  }
}

} // namespace mnexus_backend::webgpu
