// TU header --------------------------------------------
#include "shader/wgsl.h"

// external headers -------------------------------------
#if MNEXUS_INTERNAL_USE_TINT
# include <tint/tint.h>
#endif

namespace shader {

void InitializeWgslConverter() {
#if MNEXUS_INTERNAL_USE_TINT
  tint::Initialize();
#endif
}

void ShutdownWgslConverter() {
#if MNEXUS_INTERNAL_USE_TINT
  tint::Shutdown();
#endif
}

#if MNEXUS_INTERNAL_USE_TINT

std::optional<std::string> ConvertSpirvToWgsl(std::vector<uint32_t> const& spirv) {
  tint::wgsl::writer::Options wgsl_options;
  tint::Result<std::string> result = tint::SpirvToWgsl(spirv, wgsl_options);
  if (result != tint::Success) {
    return std::nullopt;
  }
  return std::move(result.Move());
}

std::optional<std::string> ConvertSpirvToWgsl(uint32_t const* spirv, uint32_t spirv_word_count) {
  std::vector<uint32_t> spirv_vector(spirv, spirv + spirv_word_count);
  return ConvertSpirvToWgsl(spirv_vector);
}

#endif // MNEXUS_INTERNAL_USE_TINT

} // namespace shader
