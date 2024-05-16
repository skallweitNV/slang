#include "validation.h"

namespace slang::rhi::validation {

Result Device::queryInterface(SlangUUID const& uuid, void** outObject)
{
    return inner->queryInterface(uuid, outObject);
}

const DeviceInfo& Device::getInfo() const
{
    return inner->getInfo();
}

Result Device::createBuffer(const BufferDesc& desc, IBuffer** outBuffer)
{
    return inner->createBuffer(desc, outBuffer);
}

Result Device::createTexture(const TextureDesc& desc, ITexture** outTexture)
{
    return inner->createTexture(desc, outTexture);
}

Result Device::createSampler(const SamplerDesc& desc, ISampler** outSampler)
{
    return inner->createSampler(desc, outSampler);
}

Result Device::createFence(const FenceDesc& desc, IFence** outFence)
{
    return inner->createFence(desc, outFence);
}


} // namespace slang::rhi::validation
