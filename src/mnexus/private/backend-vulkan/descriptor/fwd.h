#pragma once

namespace mnexus_backend::vulkan {

//
// descriptor_set_allocator.h
//

class IDescriptorSetAllocator;

//
// descriptor_set_binder.h
//

class DescriptorSetBinder;

//
// descriptor_set_write.h
//

struct DescriptorBufferValue;
struct DescriptorWriteDesc;
struct DescriptorHashedState;
class DescriptorSetWriteDesc;

} // namespace mnexus_backend::vulkan
