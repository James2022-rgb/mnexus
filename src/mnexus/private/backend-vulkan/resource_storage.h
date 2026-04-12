#pragma once

// public project headers -------------------------------
#include "mbase/public/assert.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/backend-vulkan-buffer.h"
#include "backend-vulkan/backend-vulkan-texture.h"
#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/backend-vulkan-compute_pipeline.h"

namespace mnexus_backend::vulkan {

struct ResourceStorage final {
  BufferResourcePool buffers;
  TextureResourcePool textures;
  ShaderModuleResourcePool shader_modules;
  ProgramResourcePool programs;
  ComputePipelineResourcePool compute_pipelines;
  SamplerResourcePool samplers;

  pipeline::TPipelineLayoutCache<VulkanPipelineLayoutPtr> pipeline_layout_cache;

  /// Stamp a resource's sync stamp to record that it was used in a GPU submission.
  void StampResourceUse(resource_pool::ResourceHandle handle, uint32_t queue_compact_index, uint64_t serial) {
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
    default:
      MBASE_ASSERT_MSG(false, "StampResourceUse: unhandled resource type {}", handle.resource_type());
      break;
    }
  }
};

} // namespace mnexus_backend::vulkan
