#pragma once

// external headers -------------------------------------
#include <spdlog/fmt/ostr.h>

// public project headers -------------------------------
#include "mbase/public/platform.h"
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-webgpu/webgpu_cpp_print.h"

template <>
struct spdlog::fmt_lib::formatter<wgpu::StringView>
  : spdlog::fmt_lib::ostream_formatter {};
template <>
struct spdlog::fmt_lib::formatter<wgpu::ErrorType>
  : spdlog::fmt_lib::ostream_formatter {};
template <>
struct spdlog::fmt_lib::formatter<wgpu::TextureFormat>
  : spdlog::fmt_lib::ostream_formatter {};
template <>
struct spdlog::fmt_lib::formatter<wgpu::PresentMode>
  : spdlog::fmt_lib::ostream_formatter {};
#if !defined(MBASE_PLATFORM_WEB)
template <>
struct spdlog::fmt_lib::formatter<wgpu::AlphaMode>
  : spdlog::fmt_lib::ostream_formatter {};
#endif
template <>
struct spdlog::fmt_lib::formatter<wgpu::CompositeAlphaMode>
  : spdlog::fmt_lib::ostream_formatter {};
