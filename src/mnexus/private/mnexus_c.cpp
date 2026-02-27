// public project headers -------------------------------
#include "mnexus/public/mnexus.h"

namespace {

mnexus::INexus* ToNexus(MnNexus h) { return reinterpret_cast<mnexus::INexus*>(h); }
mnexus::IDevice* ToDevice(MnDevice h) { return reinterpret_cast<mnexus::IDevice*>(h); }
mnexus::ICommandList* ToCommandList(MnCommandList h) { return reinterpret_cast<mnexus::ICommandList*>(h); }

} // namespace

extern "C" {

// ----------------------------------------------------------------------------------------------------
// INexus
//

MNEXUS_NO_THROW MnNexus MNEXUS_CALL MnNexusCreate(MnNexusDesc const* desc) {
  mnexus::NexusDesc cpp_desc {};
  if (desc) { cpp_desc.headless = desc->headless != 0; }
  return reinterpret_cast<MnNexus>(mnexus::INexus::Create(cpp_desc));
}

MNEXUS_NO_THROW void MNEXUS_CALL MnNexusDestroy(MnNexus nexus) {
  ToNexus(nexus)->Destroy();
}

MNEXUS_NO_THROW MnDevice MNEXUS_CALL MnNexusGetDevice(MnNexus nexus) {
  return reinterpret_cast<MnDevice>(ToNexus(nexus)->GetDevice());
}

// ----------------------------------------------------------------------------------------------------
// IDevice: Info
//

MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceGetAdapterInfo(MnDevice device, MnAdapterInfo* out_info) {
  ToDevice(device)->GetAdapterInfo(
    *reinterpret_cast<mnexus::AdapterInfo*>(out_info));
}

// ----------------------------------------------------------------------------------------------------
// IDevice: Resource creation / destruction
//

MNEXUS_NO_THROW MnResourceHandle MNEXUS_CALL MnDeviceCreateTexture(MnDevice device, MnTextureDesc const* desc) {
  return ToDevice(device)->CreateTexture(
    *reinterpret_cast<mnexus::TextureDesc const*>(desc)).Get();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceDestroyTexture(MnDevice device, MnResourceHandle handle) {
  ToDevice(device)->DestroyTexture(mnexus::TextureHandle(handle));
}

MNEXUS_NO_THROW MnResourceHandle MNEXUS_CALL MnDeviceCreateBuffer(MnDevice device, MnBufferDesc const* desc) {
  return ToDevice(device)->CreateBuffer(
    *reinterpret_cast<mnexus::BufferDesc const*>(desc)).Get();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceDestroyBuffer(MnDevice device, MnResourceHandle handle) {
  ToDevice(device)->DestroyBuffer(mnexus::BufferHandle(handle));
}

MNEXUS_NO_THROW MnResourceHandle MNEXUS_CALL MnDeviceCreateShaderModule(MnDevice device, MnShaderModuleDesc const* desc) {
  return ToDevice(device)->CreateShaderModule(
    *reinterpret_cast<mnexus::ShaderModuleDesc const*>(desc)).Get();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceDestroyShaderModule(MnDevice device, MnResourceHandle handle) {
  ToDevice(device)->DestroyShaderModule(mnexus::ShaderModuleHandle(handle));
}

MNEXUS_NO_THROW MnResourceHandle MNEXUS_CALL MnDeviceCreateProgram(MnDevice device, MnProgramDesc const* desc) {
  return ToDevice(device)->CreateProgram(
    *reinterpret_cast<mnexus::ProgramDesc const*>(desc)).Get();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceDestroyProgram(MnDevice device, MnResourceHandle handle) {
  ToDevice(device)->DestroyProgram(mnexus::ProgramHandle(handle));
}

MNEXUS_NO_THROW MnCommandList MNEXUS_CALL MnDeviceCreateCommandList(MnDevice device, MnCommandListDesc const* desc) {
  return reinterpret_cast<MnCommandList>(
    ToDevice(device)->CreateCommandList(
      *reinterpret_cast<mnexus::CommandListDesc const*>(desc)));
}

// ----------------------------------------------------------------------------------------------------
// IDevice: Queue operations
//

MNEXUS_NO_THROW MnIntraQueueSubmissionId MNEXUS_CALL MnDeviceQueueWriteBuffer(
    MnDevice device, MnQueueId const* queue_id,
    MnResourceHandle buffer, uint32_t offset,
    void const* data, uint32_t size) {
  return ToDevice(device)->QueueWriteBuffer(
    *reinterpret_cast<mnexus::QueueId const*>(queue_id),
    mnexus::BufferHandle(buffer), offset, data, size).Get();
}

MNEXUS_NO_THROW MnIntraQueueSubmissionId MNEXUS_CALL MnDeviceQueueSubmitCommandList(
    MnDevice device, MnQueueId const* queue_id, MnCommandList command_list) {
  return ToDevice(device)->QueueSubmitCommandList(
    *reinterpret_cast<mnexus::QueueId const*>(queue_id),
    ToCommandList(command_list)).Get();
}

MNEXUS_NO_THROW MnIntraQueueSubmissionId MNEXUS_CALL MnDeviceQueueReadBuffer(
    MnDevice device, MnQueueId const* queue_id,
    MnResourceHandle buffer, uint32_t offset,
    void* dst, uint32_t size) {
  return ToDevice(device)->QueueReadBuffer(
    *reinterpret_cast<mnexus::QueueId const*>(queue_id),
    mnexus::BufferHandle(buffer), offset, dst, size).Get();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceQueueWaitIdle(
    MnDevice device, MnQueueId const* queue_id,
    MnIntraQueueSubmissionId value) {
  ToDevice(device)->QueueWaitIdle(
    *reinterpret_cast<mnexus::QueueId const*>(queue_id),
    mnexus::IntraQueueSubmissionId(value));
}

// ----------------------------------------------------------------------------------------------------
// ICommandList
//

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListBeginRenderPass(MnCommandList cl, MnRenderPassDesc const* desc) {
  ToCommandList(cl)->BeginRenderPass(
    *reinterpret_cast<mnexus::RenderPassDesc const*>(desc));
}

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListEndRenderPass(MnCommandList cl) {
  ToCommandList(cl)->EndRenderPass();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListBindRenderProgram(MnCommandList cl, MnResourceHandle program) {
  ToCommandList(cl)->BindRenderProgram(mnexus::ProgramHandle(program));
}

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListSetVertexInputLayout(
    MnCommandList cl,
    MnVertexInputBindingDesc const* bindings, uint32_t binding_count,
    MnVertexInputAttributeDesc const* attributes, uint32_t attribute_count) {
  ToCommandList(cl)->SetVertexInputLayout(
    mnexus::container::ArrayProxy<mnexus::VertexInputBindingDesc const>(
      reinterpret_cast<mnexus::VertexInputBindingDesc const*>(bindings),
      binding_count),
    mnexus::container::ArrayProxy<mnexus::VertexInputAttributeDesc const>(
      reinterpret_cast<mnexus::VertexInputAttributeDesc const*>(attributes),
      attribute_count));
}

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListBindVertexBuffer(
    MnCommandList cl, uint32_t binding, MnResourceHandle buffer, uint64_t offset) {
  ToCommandList(cl)->BindVertexBuffer(binding, mnexus::BufferHandle(buffer), offset);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListDraw(
    MnCommandList cl, uint32_t vertex_count, uint32_t instance_count,
    uint32_t first_vertex, uint32_t first_instance) {
  ToCommandList(cl)->Draw(vertex_count, instance_count, first_vertex, first_instance);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListCopyTextureToBuffer(
    MnCommandList cl,
    MnResourceHandle src_texture, MnTextureSubresourceRange const* src_range,
    MnResourceHandle dst_buffer, uint32_t dst_offset,
    MnExtent3d const* extent) {
  ToCommandList(cl)->CopyTextureToBuffer(
    mnexus::TextureHandle(src_texture),
    *reinterpret_cast<mnexus::TextureSubresourceRange const*>(src_range),
    mnexus::BufferHandle(dst_buffer),
    dst_offset,
    *reinterpret_cast<mnexus::Extent3d const*>(extent));
}

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListEnd(MnCommandList cl) {
  ToCommandList(cl)->End();
}

} // extern "C"
