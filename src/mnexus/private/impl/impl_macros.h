#pragma once

// public project headers -------------------------------
#include "mnexus/public/mnexus.h"

/// Shorthand for implementing a virtual API method with mnexus calling convention.
#define IMPL_VAPI(ret, func, ...) \
  MNEXUS_NO_THROW ret MNEXUS_CALL func(__VA_ARGS__) override
