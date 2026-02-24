#pragma once

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

// Each test implements this function.
// Called by the harness after Logger initialization.
// Return 0 on success, non-zero on failure.
int MnTestMain(int argc, char** argv);

// Write pixels as PNG.
// Native: writes to file at `filename`.
// Emscripten: triggers a browser download of the PNG.
// Returns non-zero on success.
int MnTestWritePng(
    char const* filename,
    int width, int height, int channels,
    void const* pixels, int stride_bytes);

#if defined(__cplusplus)
}
#endif
