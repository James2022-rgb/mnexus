// public project headers -------------------------------
#include "mnexus/public/mnexus.h"

extern "C" {

MnNexus MnNexusCreate(MnNexusDesc const* desc) {
  mnexus::NexusDesc cpp_desc {};
  if (desc) { cpp_desc.headless = desc->headless != 0; }
  return reinterpret_cast<MnNexus>(mnexus::INexus::Create(cpp_desc));
}

void MnNexusDestroy(MnNexus nexus) {
  reinterpret_cast<mnexus::INexus*>(nexus)->Destroy();
}

MnDevice MnNexusGetDevice(MnNexus nexus) {
  return reinterpret_cast<MnDevice>(
    reinterpret_cast<mnexus::INexus*>(nexus)->GetDevice());
}

void MnDeviceGetAdapterInfo(MnDevice device, MnAdapterInfo* out_info) {
  reinterpret_cast<mnexus::IDevice*>(device)->GetAdapterInfo(
    *reinterpret_cast<mnexus::AdapterInfo*>(out_info));
}

} // extern "C"
