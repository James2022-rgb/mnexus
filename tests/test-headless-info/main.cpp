// c++ headers ------------------------------------------
#include <cstdio>

// public project headers -------------------------------
#include "mnexus/public/mnexus.h"

// test harness -----------------------------------------
#include "mnexus_test_harness.h"

extern "C" int MnTestMain(int, char**) {
  mnexus::INexus* nexus = mnexus::INexus::Create({.headless = true});
  mnexus::IDevice* device = nexus->GetDevice();

  mnexus::AdapterInfo info {};
  device->GetAdapterInfo(info);

  std::printf("Adapter Info:\n");
  std::printf("  Device Name:  %s\n", info.device_name);
  std::printf("  Vendor:       %s\n", info.vendor);
  std::printf("  Architecture: %s\n", info.architecture);
  std::printf("  Description:  %s\n", info.description);
  std::printf("  Vendor ID:    0x%04X\n", info.vendor_id);
  std::printf("  Device ID:    0x%04X\n", info.device_id);

  nexus->Destroy();

  return 0;
}
