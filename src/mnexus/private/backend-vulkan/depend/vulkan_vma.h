#pragma once

// Include volk-based Vulkan headers first so that vk_mem_alloc.h sees
// VULKAN_H_ already defined and skips its own #include <vulkan/vulkan.h>.
#include "backend-vulkan/depend/vulkan.h"

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wnullability-completeness"
# pragma clang diagnostic ignored "-Wunused-parameter"
# pragma clang diagnostic ignored "-Wunused-variable"
# pragma clang diagnostic ignored "-Wmissing-field-initializers"
# pragma clang diagnostic ignored "-Wunused-private-field"
#endif

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#if defined(__clang__)
# pragma clang diagnostic pop
#endif
