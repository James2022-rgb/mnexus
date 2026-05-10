#pragma once

// public project headers -------------------------------
#include "mbase/public/assert.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "pipeline/render_pipeline_cache.h"

#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/backend-vulkan-buffer.h"
#include "backend-vulkan/backend-vulkan-texture.h"
#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/backend-vulkan-compute_pipeline.h"
#include "backend-vulkan/object/vk-object-render_pipeline.h"
#include "backend-vulkan/resource/image_view_cache.h"
#include "backend-vulkan/resource/vk-query_pool.h"

#if MNEXUS_ENABLE_VIDEO_CODING
#  include "backend-vulkan/video/vk-video_session.h"
#  include "backend-vulkan/video/vk-video_session_parameters.h"
#endif

namespace mnexus_backend::vulkan {

struct ResourceStorage final {
  BufferResourcePool buffers;
  TextureResourcePool textures;
  ShaderModuleResourcePool shader_modules;
  ProgramResourcePool programs;
  ComputePipelineResourcePool compute_pipelines;
  SamplerResourcePool samplers;
  QueryPoolResourcePool query_pools;
#if MNEXUS_ENABLE_VIDEO_CODING
  VideoSessionResourcePool video_sessions;
  VideoSessionParametersResourcePool video_session_parameters;
#endif

  pipeline::TPipelineLayoutCache<VulkanPipelineLayoutPtr> pipeline_layout_cache;
  pipeline::TRenderPipelineCache<VulkanRenderPipelinePtr> render_pipeline_cache;
  ImageViewCache image_view_cache;

  resource_pool::ResourceHandle swapchain_texture_handle = resource_pool::ResourceHandle::Null(); // Not protected; set only during initialization.

  /// Stamp a resource's sync stamp to record that it was used in a GPU submission.
  void StampResourceUse(resource_pool::ResourceHandle handle, uint32_t queue_compact_index, uint64_t serial);

  void ShowDebugUi() const;
};

} // namespace mnexus_backend::vulkan
