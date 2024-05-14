#pragma once

#include "slang-rhi.h"
#include "vulkan-api.h"
#include "../base-backend.h"

#include "core/slang-com-object.h"

namespace slang::rhi::vulkan {

class Device;

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

class Device : public IDevice, public Slang::ComObject
{
public:
    SLANG_COM_OBJECT_IUNKNOWN_ALL

    ISlangUnknown* getInterface(const Slang::Guid& guid) { return nullptr; }

    Result init(const DeviceDesc& desc);

    virtual SLANG_NO_THROW Result SLANG_MCALL createSampler(const SamplerDesc& desc, ISampler** outSampler) override;

    VulkanApi api;
    VkDevice vkDevice = VK_NULL_HANDLE;
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

    virtual Result SLANG_MCALL createDevice(const DeviceDesc* desc, IAdapter* adapter, IDevice** outDevice) override;
};

inline Result createFactory(IFactory** outFactory)
{
    RefPtr<Factory> factory = new Factory();
    SLANG_RETURN_ON_FAIL(factory->init());
    returnComPtr(outFactory, factory);
    return SLANG_OK;
}

} // namespace slang::rhi::vulkan
