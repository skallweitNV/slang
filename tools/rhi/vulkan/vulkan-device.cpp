#include "vulkan-backend.h"

namespace slang::rhi::vulkan {

Result Device::init(const DeviceDesc& desc, IAdapter* adapter_)
{
    SLANG_RETURN_ON_FAIL(Base::init(desc));

    // Adapter* adapter = checked_cast<Adapter*>(adapter_);

    // if (!adapter) createVulkan
    
    const DeviceDescVulkan* descVulkan = findStructInChain<DeviceDescVulkan>(desc.next);
    if (descVulkan) {
        printf("Found vulkan desc!\n");
    }
    

    return SLANG_E_NOT_IMPLEMENTED;
}

void Device::destroy()
{
}

Result Device::createSampler(const SamplerDesc& desc, ISampler** outSampler)
{
    RefPtr<Sampler> sampler = new Sampler();
    SLANG_RETURN_ON_FAIL(sampler->init(this, desc));
    returnComPtr(outSampler, sampler);
    return SLANG_OK;
}

} // namespace slang::rhi::vulkan
