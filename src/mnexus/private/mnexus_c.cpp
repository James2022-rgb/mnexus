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

MnNexus MnNexusCreate(MnNexusDesc const* desc) {
  mnexus::NexusDesc cpp_desc {};
  if (desc) { cpp_desc.headless = desc->headless != 0; }
  return reinterpret_cast<MnNexus>(mnexus::INexus::Create(cpp_desc));
}

void MnNexusDestroy(MnNexus nexus) {
  ToNexus(nexus)->Destroy();
}

MnDevice MnNexusGetDevice(MnNexus nexus) {
  return reinterpret_cast<MnDevice>(ToNexus(nexus)->GetDevice());
}

// ----------------------------------------------------------------------------------------------------
// IDevice: Info
//

void MnDeviceGetAdapterInfo(MnDevice device, MnAdapterInfo* out_info) {
  ToDevice(device)->GetAdapterInfo(
    *reinterpret_cast<mnexus::AdapterInfo*>(out_info));
}

// ----------------------------------------------------------------------------------------------------
// IDevice: Resource creation / destruction
//

MnResourceHandle MnDeviceCreateTexture(MnDevice device, MnTextureDesc const* desc) {
  return ToDevice(device)->CreateTexture(
    *reinterpret_cast<mnexus::TextureDesc const*>(desc)).Get();
}

void MnDeviceDestroyTexture(MnDevice device, MnResourceHandle handle) {
  ToDevice(device)->DestroyTexture(mnexus::TextureHandle(handle));
}

MnResourceHandle MnDeviceCreateBuffer(MnDevice device, MnBufferDesc const* desc) {
  return ToDevice(device)->CreateBuffer(
    *reinterpret_cast<mnexus::BufferDesc const*>(desc)).Get();
}

void MnDeviceDestroyBuffer(MnDevice device, MnResourceHandle handle) {
  ToDevice(device)->DestroyBuffer(mnexus::BufferHandle(handle));
}

MnResourceHandle MnDeviceCreateShaderModule(MnDevice device, MnShaderModuleDesc const* desc) {
  return ToDevice(device)->CreateShaderModule(
    *reinterpret_cast<mnexus::ShaderModuleDesc const*>(desc)).Get();
}

void MnDeviceDestroyShaderModule(MnDevice device, MnResourceHandle handle) {
  ToDevice(device)->DestroyShaderModule(mnexus::ShaderModuleHandle(handle));
}

MnResourceHandle MnDeviceCreateProgram(MnDevice device, MnProgramDesc const* desc) {
  mnexus::ProgramDesc cpp_desc {};
  cpp_desc.struct_type = desc->struct_type;
  cpp_desc.next_ptr = desc->next_ptr;
  cpp_desc.shader_modules = mnexus::container::ArrayProxy<mnexus::ShaderModuleHandle const>(
    reinterpret_cast<mnexus::ShaderModuleHandle const*>(desc->shader_modules),
    desc->shader_module_count);
  return ToDevice(device)->CreateProgram(cpp_desc).Get();
}

void MnDeviceDestroyProgram(MnDevice device, MnResourceHandle handle) {
  ToDevice(device)->DestroyProgram(mnexus::ProgramHandle(handle));
}

MnCommandList MnDeviceCreateCommandList(MnDevice device, MnCommandListDesc const* desc) {
  return reinterpret_cast<MnCommandList>(
    ToDevice(device)->CreateCommandList(
      *reinterpret_cast<mnexus::CommandListDesc const*>(desc)));
}

// ----------------------------------------------------------------------------------------------------
// IDevice: Queue operations
//

MnIntraQueueSubmissionId MnDeviceQueueWriteBuffer(
    MnDevice device, MnQueueId const* queue_id,
    MnResourceHandle buffer, uint32_t offset,
    void const* data, uint32_t size) {
  return ToDevice(device)->QueueWriteBuffer(
    *reinterpret_cast<mnexus::QueueId const*>(queue_id),
    mnexus::BufferHandle(buffer), offset, data, size).Get();
}

MnIntraQueueSubmissionId MnDeviceQueueSubmitCommandList(
    MnDevice device, MnQueueId const* queue_id, MnCommandList command_list) {
  return ToDevice(device)->QueueSubmitCommandList(
    *reinterpret_cast<mnexus::QueueId const*>(queue_id),
    ToCommandList(command_list)).Get();
}

MnIntraQueueSubmissionId MnDeviceQueueReadBuffer(
    MnDevice device, MnQueueId const* queue_id,
    MnResourceHandle buffer, uint32_t offset,
    void* dst, uint32_t size) {
  return ToDevice(device)->QueueReadBuffer(
    *reinterpret_cast<mnexus::QueueId const*>(queue_id),
    mnexus::BufferHandle(buffer), offset, dst, size).Get();
}

void MnDeviceQueueWait(
    MnDevice device, MnQueueId const* queue_id,
    MnIntraQueueSubmissionId value) {
  ToDevice(device)->QueueWait(
    *reinterpret_cast<mnexus::QueueId const*>(queue_id),
    mnexus::IntraQueueSubmissionId(value));
}

// ----------------------------------------------------------------------------------------------------
// ICommandList
//

void MnCommandListBeginRenderPass(MnCommandList cl, MnRenderPassDesc const* desc) {
  mnexus::RenderPassDesc cpp_desc {};
  cpp_desc.color_attachments = mnexus::container::ArrayProxy<mnexus::ColorAttachmentDesc const>(
    reinterpret_cast<mnexus::ColorAttachmentDesc const*>(desc->color_attachments),
    desc->color_attachment_count);
  cpp_desc.depth_stencil_attachment =
    reinterpret_cast<mnexus::DepthStencilAttachmentDesc const*>(desc->depth_stencil_attachment);
  ToCommandList(cl)->BeginRenderPass(cpp_desc);
}

void MnCommandListEndRenderPass(MnCommandList cl) {
  ToCommandList(cl)->EndRenderPass();
}

void MnCommandListBindRenderProgram(MnCommandList cl, MnResourceHandle program) {
  ToCommandList(cl)->BindRenderProgram(mnexus::ProgramHandle(program));
}

void MnCommandListSetVertexInputLayout(
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

void MnCommandListBindVertexBuffer(
    MnCommandList cl, uint32_t binding, MnResourceHandle buffer, uint64_t offset) {
  ToCommandList(cl)->BindVertexBuffer(binding, mnexus::BufferHandle(buffer), offset);
}

void MnCommandListDraw(
    MnCommandList cl, uint32_t vertex_count, uint32_t instance_count,
    uint32_t first_vertex, uint32_t first_instance) {
  ToCommandList(cl)->Draw(vertex_count, instance_count, first_vertex, first_instance);
}

void MnCommandListCopyTextureToBuffer(
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

void MnCommandListEnd(MnCommandList cl) {
  ToCommandList(cl)->End();
}

} // extern "C"
