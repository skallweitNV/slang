#include "vulkan-backend.h"
#include "vulkan-utils.h"

namespace slang::rhi::vulkan {

Result Sampler::init(Device* device, const SamplerDesc& desc)
{
    this->device = device;
    this->desc = desc;

    VkSamplerCreateInfo createInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    createInfo.magFilter = utils::translateFilter(desc.magFilter);
    createInfo.minFilter = utils::translateFilter(desc.minFilter);
    createInfo.mipmapMode = utils::translateSamplerMipmapMode(desc.mipFilter);
    createInfo.addressModeU = utils::translateSamplerAddressMode(desc.addressU);
    createInfo.addressModeV = utils::translateSamplerAddressMode(desc.addressV);
    createInfo.addressModeW = utils::translateSamplerAddressMode(desc.addressW);
    createInfo.mipLodBias = desc.mipBias;
    createInfo.anisotropyEnable = desc.maxAnisotropy > 1;
    createInfo.maxAnisotropy = (float)desc.maxAnisotropy;
    createInfo.compareEnable = desc.comparisonFunc != ComparisonFunc::Always;
    createInfo.compareOp = utils::translateCompareOp(desc.comparisonFunc);
    createInfo.minLod = desc.mipMin;
    createInfo.maxLod = desc.mipMax;
    // createInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;
    SLANG_VK_RETURN_ON_FAIL(device->api.vkCreateSampler(device->vkDevice, &createInfo, device->vkAllocator, &vkSampler));
    return SLANG_OK;
}

void Sampler::destroy()
{
    device->api.vkDestroySampler(device->vkDevice, vkSampler, device->vkAllocator);
}

Result Sampler::getNativeHandle(NativeHandleType type, NativeHandle* outHandle) const
{
    if (type == NativeHandleType::VK_Sampler)
    {
        *outHandle = NativeHandle((uint64_t)vkSampler);
        return SLANG_OK;
    }
    return SLANG_E_NOT_AVAILABLE;
}

Result Device::createSampler(const SamplerDesc& desc, ISampler** outSampler)
{
    RefPtr<Sampler> sampler = new Sampler();
    SLANG_RETURN_ON_FAIL(sampler->init(this, desc));
    returnComPtr(outSampler, sampler);
    return SLANG_OK;
}

} // namespace slang::rhi::vulkan
