#include "backend-vulkan/depend/vulkan_vma.h"

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wnullability-completeness"
# pragma clang diagnostic ignored "-Wunused-parameter"
# pragma clang diagnostic ignored "-Wunused-variable"
# pragma clang diagnostic ignored "-Wmissing-field-initializers"
# pragma clang diagnostic ignored "-Wunused-private-field"
#endif

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#if defined(__clang__)
# pragma clang diagnostic pop
#endif
