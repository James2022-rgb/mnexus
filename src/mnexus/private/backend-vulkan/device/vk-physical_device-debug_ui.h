#pragma once

#if MNEXUS_HAVE_DEAR_IMGUI

// project headers --------------------------------------
#include "backend-vulkan/device/vk-physical_device.h"

// Dear ImGui rendering helpers for the descriptor types in
// `vk-physical_device.h`. Each helper renders an inline section --
// the caller wraps the call in a `CollapsingHeader` / `TreeNode` if it
// wants the section to be foldable.

namespace mnexus_backend::vulkan::debug_ui {

void ShowVideoCodingCapabilitiesUi(VideoCodingCapabilities const& caps);
void ShowVideoDecodeH265CapabilitiesUi(VideoDecodeH265Capabilities const& caps);
void ShowVideoDecodeH265PropertiesUi(VideoDecodeH265Properties const& props);
void ShowVideoEncodeH265CapabilitiesUi(VideoEncodeH265Capabilities const& caps);
void ShowVideoEncodeH265PropertiesUi(VideoEncodeH265Properties const& props);

} // namespace mnexus_backend::vulkan::debug_ui

#endif // MNEXUS_HAVE_DEAR_IMGUI
