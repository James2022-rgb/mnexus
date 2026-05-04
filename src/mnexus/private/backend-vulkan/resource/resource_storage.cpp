// TU header --------------------------------------------
#include "backend-vulkan/resource/resource_storage.h"

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
  default:
    MBASE_ASSERT_MSG(false, "StampResourceUse: unhandled resource type {}", handle.resource_type());
    break;
  }
}

void ResourceStorage::ShowDebugUi() const {
#if MNEXUS_HAVE_DEAR_IMGUI
  ImGui::Text("PLACEHOLDER: ResourceStorage debug UI");
#endif
}

} // namespace mnexus_backend::vulkan
