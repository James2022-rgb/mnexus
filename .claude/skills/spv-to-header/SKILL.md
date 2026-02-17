---
name: spv-to-header
description: Convert a SPIR-V binary (.spv) file to a C/C++ header with an inline constexpr byte array. Use when the user wants to embed a .spv shader as a C array.
disable-model-invocation: true
argument-hint: <path-to-spv-file>
allowed-tools: Bash(xxd *), Read, Write, Glob
---

Convert the SPIR-V binary file at `$ARGUMENTS` to a C++ header containing an `inline constexpr uint8_t` array.

## Steps

1. Verify the `.spv` file exists using Glob. If `$ARGUMENTS` is empty, search for `.spv` files under `src/mnexus/private/builtin_shader/` and ask the user which one to convert.
2. Run `xxd -i "<path>"` via Bash to produce the raw C array bytes.
3. Write a header file (same directory, name = `<basename>_spv.h`) with this structure:

```cpp
#pragma once

// Auto-generated from <basename>.spv by xxd -i.
// Source: <basename>.slang (if present)

#include <cstdint>

namespace builtin_shader {

inline constexpr uint8_t k<PascalCaseName>Spv[] = {
  // ... hex bytes from xxd, 12 bytes per line ...
};

inline constexpr uint32_t k<PascalCaseName>SpvSize = sizeof(k<PascalCaseName>Spv);

} // namespace builtin_shader
```

## Naming convention

- File: `blit_2d_color.spv` -> `blit_2d_color_spv.h`
- Array: `blit_2d_color` -> `kBlit2dColorSpv` (snake_case to PascalCase, append `Spv`)
- Size: `kBlit2dColorSpvSize`

## Notes

- Use `inline constexpr` so the array lives in the header without ODR issues.
- Use `uint8_t` for the array element type, not `unsigned char`.
- If a `.slang` file with the same basename exists in the same directory, reference it in the comment.
- Output the header in the same directory as the input `.spv` file.
