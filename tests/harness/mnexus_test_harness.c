// c headers --------------------------------------------
#include <stdlib.h>
#include <string.h>

// TU header --------------------------------------------
#include "mnexus_test_harness.h"

// external headers -------------------------------------
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

// public project headers -------------------------------
#include "mbase/public/log/log_c.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// ----------------------------------------------------------------------------------------------------
// Emscripten: PNG download via JS Blob + <a> click.
//

EM_JS(void, mn_test_js_download_file, (const char* filename, const void* data, int size), {
    var copy = new Uint8Array(size);
    copy.set(new Uint8Array(wasmMemory.buffer, data, size));
    var blob = new Blob([copy], { type: "application/octet-stream" });
    var url = URL.createObjectURL(blob);
    var a = document.createElement("a");
    a.href = url;
    a.download = UTF8ToString(filename);
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
});

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} MnTestMemoryBuffer;

static void mn_test_write_to_memory(void* context, void* data, int size) {
    MnTestMemoryBuffer* buf = (MnTestMemoryBuffer*)context;
    while (buf->size + (size_t)size > buf->capacity) {
        buf->capacity = buf->capacity == 0 ? 1024 : buf->capacity * 2;
        buf->data = (uint8_t*)realloc(buf->data, buf->capacity);
    }
    memcpy(buf->data + buf->size, data, (size_t)size);
    buf->size += (size_t)size;
}

#endif // __EMSCRIPTEN__

// ----------------------------------------------------------------------------------------------------
// MnTestWritePng
//

int MnTestWritePng(
    char const* filename,
    int width, int height, int channels,
    void const* pixels, int stride_bytes)
{
#ifdef __EMSCRIPTEN__
    MnTestMemoryBuffer buffer = { NULL, 0, 0 };
    int result = stbi_write_png_to_func(
        mn_test_write_to_memory, &buffer,
        width, height, channels, pixels, stride_bytes);
    if (result) {
        mn_test_js_download_file(filename, buffer.data, (int)buffer.size);
    }
    free(buffer.data);
    return result;
#else
    return stbi_write_png(filename, width, height, channels, pixels, stride_bytes);
#endif
}

// ----------------------------------------------------------------------------------------------------
// main
//
// Provided by the harness. Each test implements MnTestMain().
//

int main(int argc, char** argv) {
    MbLoggerInitialize();
    int result = MnTestMain(argc, argv);
#ifndef __EMSCRIPTEN__
    // On Emscripten, skip Logger shutdown. EXIT_RUNTIME=0 (default) keeps the runtime alive,
    // and a main-loop test's callback may still be firing after MnTestMain() returns.
    // Page/tab close handles cleanup.
    MbLoggerShutdown();
#endif
    return result;
}
