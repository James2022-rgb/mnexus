#pragma once

// public project headers -------------------------------
#include "mbase/public/log.h"
#include "mbase/public/trap.h"

#include "mnexus/public/mnexus.h"

/// Shorthand for implementing a virtual API method with mnexus calling convention.
#define IMPL_VAPI(ret, func, ...) \
  MNEXUS_NO_THROW ret MNEXUS_CALL func(__VA_ARGS__) override

/// Log an error and trap for functions that are declared but not yet implemented.
#define STUB_NOT_IMPLEMENTED() \
  do { MBASE_LOG_ERROR("Stub: {}() not implemented", __func__); mbase::Trap(); } while (0)
