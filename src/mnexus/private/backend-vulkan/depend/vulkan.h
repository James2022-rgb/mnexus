#pragma once

#include "mbase/public/platform.h"

#if MBASE_PLATFORM_WINDOWS
# if !defined(VK_USE_PLATFORM_WIN32_KHR)
#  define VK_USE_PLATFORM_WIN32_KHR 1
# endif

# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
# endif
# if !defined(NOMINMAX)
#  define NOMINMAX
# endif

#elif MBASE_PLATFORM_LINUX
# if !defined(VK_USE_PLATFORM_XLIB_KHR)
#  define VK_USE_PLATFORM_XLIB_KHR 1
# endif
# if !defined(VK_USE_PLATFORM_WAYLAND_KHR)
#  define VK_USE_PLATFORM_WAYLAND_KHR 1
# endif
#elif MBASE_PLATFORM_ANDROID
# if !defined(VK_USE_PLATFORM_ANDROID_KHR)
#  define VK_USE_PLATFORM_ANDROID_KHR 1
# endif
#endif
#include "volk/volk.h"
#include "vulkan/vk_enum_string_helper.h"
#include "vulkan/utility/vk_format_utils.h"
