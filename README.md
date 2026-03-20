
<div align="center">

# `mnexus`

**Thin abstraction layer providing a unified API over WebGPU (native and Emscipten) and Vulkan (planned)**

</div>

## Supported Platforms

| Backend (API) | Windows 🖥         | Linux 🖥 | Android 📱         | Web 🌐             |
| ------------- | ------------------ | -------- | ------------------ | ------------------ |
| Vulkan        | 📋                 | 📋       | 📋                 | ➖                 |
| WebGPU        | ✅ Dawn            | ⚠️ Dawn  | 🚧 Dawn            | ✅ Emscripten      |

> ✅ Supported &ensp; ⚠️ Implemented, not yet tested &ensp; 📋 Planned &ensp; 🚧 Impractical &ensp; ➖ N/A

## Vulkan Backend Requirements

- Vulkan 1.1
- `VK_KHR_timeline_semaphore`
- `VK_KHR_synchronization2`
