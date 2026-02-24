/* c headers -------------------------------------------- */
#include <stdio.h>

/* public project headers ------------------------------- */
#include "mnexus/public/mnexus.h"

/* test harness ----------------------------------------- */
#include "mnexus_test_harness.h"

int MnTestMain(int argc, char** argv) {
  (void)argc;
  (void)argv;

  MnNexusDesc desc = { 0 };
  desc.headless = MnBoolTrue;
  MnNexus nexus = MnNexusCreate(&desc);
  MnDevice device = MnNexusGetDevice(nexus);

  MnAdapterInfo info = { 0 };
  MnDeviceGetAdapterInfo(device, &info);

  printf("Adapter Info:\n");
  printf("  Device Name:  %s\n", info.device_name);
  printf("  Vendor:       %s\n", info.vendor);
  printf("  Architecture: %s\n", info.architecture);
  printf("  Description:  %s\n", info.description);
  printf("  Vendor ID:    0x%04X\n", info.vendor_id);
  printf("  Device ID:    0x%04X\n", info.device_id);

  MnNexusDestroy(nexus);

  return 0;
}
