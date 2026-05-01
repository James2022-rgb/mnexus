#pragma once

namespace mnexus_backend::vulkan {

//
// command_encoder.h
//

union RenderTargetClearValue;
struct RenderTargetDesc;
struct DynamicRenderPassDesc;
struct CommandEncoderDesc;
class ICommandEncoder;

//
// image_layout_tracker.h
//

class ImageLayoutTracker;

//
// pending_pipeline_barrier.h
//

struct SyncScope;
class PendingPipelineBarrier;

} // namespace mnexus_backend::vulkan
