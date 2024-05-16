#pragma once

// This needs to be included first, as it includes the Vulkan headers
// and sets the appropriate defines for each platform.
#include "vulkan-api.h"

#define SLANG_RHI_VULKAN
#include "slang-rhi.h"
#include "../base-backend.h"

#include "core/slang-com-object.h"

namespace slang::rhi::vulkan {

class Device;

struct VulkanContext
{
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
    VkDevice vkDevice = VK_NULL_HANDLE;

    VkAllocationCallbacks* vkAllocator = nullptr;

    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
};

// ----------------------------------------------------------------------------
// Sampler
// ----------------------------------------------------------------------------

class Sampler : public SamplerBase
{
public:
    virtual ~Sampler() { destroy(); }

    Result init(Device* device, const SamplerDesc& desc);
    void destroy();

    virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(NativeHandleType type, NativeHandle* outHandle) const override;

    RefPtr<Device> device;
    VkSampler vkSampler = VK_NULL_HANDLE;
};

// ----------------------------------------------------------------------------
// Device
// ----------------------------------------------------------------------------

class Device : public DeviceBase
{
public:
    using Base = DeviceBase;

    virtual ~Device() { destroy(); }

    Result init(const DeviceDesc& desc, IAdapter* adapter);
    void destroy();

    virtual SLANG_NO_THROW Result SLANG_MCALL createSampler(const SamplerDesc& desc, ISampler** outSampler) override;

    VulkanApi api;
    VulkanContext ctx;
    // VkDevice vkDevice = VK_NULL_HANDLE;
    VkAllocationCallbacks* vkAllocator = nullptr;
};

// ----------------------------------------------------------------------------
// Adapter
// ----------------------------------------------------------------------------

class Adapter : public AdapterBase {};

// ----------------------------------------------------------------------------
// Factory
// ----------------------------------------------------------------------------

class Factory : public FactoryBase
{
public:
    Result init();

    virtual Result SLANG_MCALL createDevice(const DeviceDesc& desc, IAdapter* adapter, IDevice** outDevice) override;
};

inline Result createFactory(IFactory** outFactory)
{
    RefPtr<Factory> factory = new Factory();
    SLANG_RETURN_ON_FAIL(factory->init());
    returnComPtr(outFactory, factory);
    return SLANG_OK;
}

} // namespace slang::rhi::vulkan
