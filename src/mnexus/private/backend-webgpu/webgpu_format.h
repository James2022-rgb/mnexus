#pragma once

// external headers -------------------------------------
#include <fmt/ostream.h>

// public project headers -------------------------------
#include "mbase/public/platform.h"
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-webgpu/webgpu_cpp_print.h"

template <>
struct fmt::formatter<wgpu::StringView>
  : fmt::ostream_formatter {};
template <>
struct fmt::formatter<wgpu::ErrorType>
  : fmt::ostream_formatter {};
template <>
struct fmt::formatter<wgpu::TextureFormat>
  : fmt::ostream_formatter {};
template <>
struct fmt::formatter<wgpu::PresentMode>
  : fmt::ostream_formatter {};
#if !defined(MBASE_PLATFORM_WEB)
template <>
struct fmt::formatter<wgpu::AlphaMode>
  : fmt::ostream_formatter {};
#endif
template <>
struct fmt::formatter<wgpu::CompositeAlphaMode>
  : fmt::ostream_formatter {};
