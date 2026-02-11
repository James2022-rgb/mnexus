#pragma once

// c++ headers ------------------------------------------
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace shader {

// Tint runtime initialization / shutdown.
void InitializeWgslConverter();
void ShutdownWgslConverter();

// SPIR-V to WGSL conversion via Tint.
#if MNEXUS_INTERNAL_USE_TINT
std::optional<std::string> ConvertSpirvToWgsl(std::vector<uint32_t> const& spirv);
std::optional<std::string> ConvertSpirvToWgsl(uint32_t const* spirv, uint32_t spirv_word_count);
#endif

} // namespace shader
