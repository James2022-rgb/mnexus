# Emscripten ASYNCIFY and Main Loop

The WebGPU backend of mnexus uses `wgpuInstanceWaitAny`
(via `wgpu::Instance::WaitAny`) to implement synchronous `QueueWait`. On
Emscripten, synchronous blocking is impossible in the browser event loop, so the
build enables **ASYNCIFY**, which transforms blocking calls into a suspend/resume
(unwind/rewind) sequence backed by JavaScript promises.

```cmake
set(CMAKE_EXE_LINKER_FLAGS "... -s ASYNCIFY -s ASYNCIFY_STACK_SIZE=65536 ...")
```

This is required because the WebGPU backend's `QueueWait` implementation polls
`WaitAny(future, UINT64_MAX)` in a loop until the target timeline value is
reached.

## Problem: `emscripten_set_main_loop` + ASYNCIFY conflict

### Symptom

An application using the mnexus WebGPU backend on Emscripten renders one frame
(or zero frames) and then freezes. The browser console shows:

```
Uncaught (in promise) unwind
```

### Root cause

If `WaitAny` (or any ASYNCIFY-wrapped function) is called before
`emscripten_set_main_loop` — for example, during resource initialization where
`QueueWait` is used to synchronously wait for a texture upload — the subsequent
code runs inside a **resumed ASYNCIFY context**, i.e. inside a JavaScript promise
chain.

Two common patterns conflict with this:

1. **`emscripten_set_main_loop_arg(cb, arg, fps, true)`** — the
   `simulate_infinite_loop=true` parameter works by *throwing* an exception to
   unwind the C stack and prevent `main()` from continuing. When executed inside
   an ASYNCIFY promise context, this throw becomes an uncaught promise rejection
   (`"Uncaught (in promise) unwind"`), which aborts the application.

2. **`emscripten_exit_with_live_runtime()`** — also throws internally. Same
   conflict when called inside an ASYNCIFY-resumed context.

### Solution

Use `simulate_infinite_loop=false` and let `main()` return normally:

```cpp
emscripten_set_main_loop_arg(callback, arg, 0, false);
return 0;  // main() returns; runtime stays alive (EXIT_RUNTIME=0 by default)
```

Key points:

- `EXIT_RUNTIME=0` (Emscripten default) keeps the runtime alive after `main()`
  returns. The main loop callback continues to fire via `requestAnimationFrame`.
- Do **not** call `emscripten_exit_with_live_runtime()` — it throws just like
  `simulate_infinite_loop=true`.
- Any objects referenced by the main loop callback must **outlive `main()`**.
  Use `.release()` on `std::unique_ptr` or allocate on the heap:

```cpp
MyApp* app = MyApp::Create(...).release();  // intentionally not freed

emscripten_set_main_loop_arg(
    [](void* arg) {
        auto* a = reinterpret_cast<MyApp*>(arg);
        a->Tick();
    },
    app, 0, false);

return 0;
```

## Summary of incompatible patterns

| Pattern | Why it fails with ASYNCIFY |
|---------|---------------------------|
| `emscripten_set_main_loop(..., true)` | Throws to simulate infinite loop; becomes uncaught in ASYNCIFY promise |
| `emscripten_exit_with_live_runtime()` | Also throws internally; same problem |
| Stack-allocated objects used after `main()` returns | Dangling pointers if `simulate_infinite_loop=false` and main returns |

## References

- [emscripten#24154](https://github.com/emscripten-core/emscripten/issues/24154) — `wgpuInstanceWaitAny` + "Uncaught (in promise) unwind"
- [emscripten#16071](https://github.com/emscripten-core/emscripten/issues/16071) — unwind error with main loop
- [emscripten#18442](https://github.com/emscripten-core/emscripten/issues/18442) — `EM_ASYNC_JS` + `emscripten_exit_with_live_runtime()`
- [Emscripten ASYNCIFY docs](https://emscripten.org/docs/porting/asyncify.html)
