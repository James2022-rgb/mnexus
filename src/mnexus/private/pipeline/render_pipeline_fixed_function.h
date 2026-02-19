#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <type_traits>

// public project headers -------------------------------
#include "mnexus/public/types.h"

namespace pipeline {

/// Packed per-draw fixed-function state for PSO cache key hashing.
/// All fields are stored as uint8_t to enable fast memcmp/memhash.
struct PerDrawFixedFunctionStaticState final {
  uint8_t ia_primitive_topology   = static_cast<uint8_t>(mnexus::PrimitiveTopology::kTriangleList);
  uint8_t raster_polygon_mode     = static_cast<uint8_t>(mnexus::PolygonMode::kFill);
  uint8_t raster_cull_mode        = static_cast<uint8_t>(mnexus::CullMode::kNone);
  uint8_t raster_front_face       = static_cast<uint8_t>(mnexus::FrontFace::kCounterClockwise);
  uint8_t depth_test_enabled      = 0;
  uint8_t depth_write_enabled     = 0;
  uint8_t depth_compare_op        = static_cast<uint8_t>(mnexus::CompareOp::kAlways);
  uint8_t stencil_test_enabled    = 0;
  uint8_t stencil_front_fail_op   = static_cast<uint8_t>(mnexus::StencilOp::kKeep);
  uint8_t stencil_front_pass_op   = static_cast<uint8_t>(mnexus::StencilOp::kKeep);
  uint8_t stencil_front_depth_fail_op = static_cast<uint8_t>(mnexus::StencilOp::kKeep);
  uint8_t stencil_front_compare_op    = static_cast<uint8_t>(mnexus::CompareOp::kAlways);
  uint8_t stencil_back_fail_op   = static_cast<uint8_t>(mnexus::StencilOp::kKeep);
  uint8_t stencil_back_pass_op   = static_cast<uint8_t>(mnexus::StencilOp::kKeep);
  uint8_t stencil_back_depth_fail_op = static_cast<uint8_t>(mnexus::StencilOp::kKeep);
  uint8_t stencil_back_compare_op    = static_cast<uint8_t>(mnexus::CompareOp::kAlways);
};
static_assert(std::is_trivially_copyable_v<PerDrawFixedFunctionStaticState>);
static_assert(sizeof(PerDrawFixedFunctionStaticState) == 16);

/// Packed per-attachment fixed-function state for PSO cache key hashing.
struct PerAttachmentFixedFunctionStaticState final {
  uint8_t blend_enabled          = 0;
  uint8_t blend_src_color_factor = static_cast<uint8_t>(mnexus::BlendFactor::kOne);
  uint8_t blend_dst_color_factor = static_cast<uint8_t>(mnexus::BlendFactor::kZero);
  uint8_t blend_color_blend_op   = static_cast<uint8_t>(mnexus::BlendOp::kAdd);
  uint8_t blend_src_alpha_factor = static_cast<uint8_t>(mnexus::BlendFactor::kOne);
  uint8_t blend_dst_alpha_factor = static_cast<uint8_t>(mnexus::BlendFactor::kZero);
  uint8_t blend_alpha_blend_op   = static_cast<uint8_t>(mnexus::BlendOp::kAdd);
  uint8_t color_write_mask       = static_cast<uint8_t>(mnexus::ColorWriteMask::kAll);
};
static_assert(std::is_trivially_copyable_v<PerAttachmentFixedFunctionStaticState>);
static_assert(sizeof(PerAttachmentFixedFunctionStaticState) == 8);

} // namespace pipeline
