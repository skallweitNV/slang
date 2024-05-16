#include "vulkan-backend.h"
#include "vulkan-utils.h"

#include "core/slang-math.h"

namespace slang::rhi::vulkan {

Result Sampler::init(Device* device, const SamplerDesc& desc)
{
    this->device = device;
    this->desc = desc;

    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = utils::translateFilter(desc.magFilter);
    samplerInfo.minFilter = utils::translateFilter(desc.minFilter);
    samplerInfo.mipmapMode = utils::translateSamplerMipmapMode(desc.mipFilter);
    samplerInfo.addressModeU = utils::translateSamplerAddressMode(desc.addressU);
    samplerInfo.addressModeV = utils::translateSamplerAddressMode(desc.addressV);
    samplerInfo.addressModeW = utils::translateSamplerAddressMode(desc.addressW);
    samplerInfo.mipLodBias = desc.mipBias;
    samplerInfo.anisotropyEnable = desc.maxAnisotropy > 1;
    samplerInfo.maxAnisotropy = (float)desc.maxAnisotropy;
    samplerInfo.compareEnable = desc.reductionOp != TextureReductionOp::Comparison;
    samplerInfo.compareOp = utils::translateCompareOp(desc.comparisonFunc);
    samplerInfo.minLod = Math::Max(0.f, desc.mipMin);
    samplerInfo.maxLod = Math::Clamp(desc.mipMax, samplerInfo.minLod, VK_LOD_CLAMP_NONE);
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    VkSamplerReductionModeCreateInfo reductionInfo = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO };
    reductionInfo.reductionMode = utils::translateSamplerReductionMode(desc.reductionOp);
    utils::appendStruct(samplerInfo, reductionInfo);

    VkSamplerCustomBorderColorCreateInfoEXT borderColorInfo = { VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT };
    if (device->api.m_extendedFeatures.customBorderColorFeatures.customBorderColors)
    {
        borderColorInfo.customBorderColor.float32[0] = desc.borderColor.r;
        borderColorInfo.customBorderColor.float32[1] = desc.borderColor.g;
        borderColorInfo.customBorderColor.float32[2] = desc.borderColor.b;
        borderColorInfo.customBorderColor.float32[3] = desc.borderColor.a;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
        utils::appendStruct(samplerInfo, borderColorInfo);
    }

    SLANG_VK_RETURN_ON_FAIL(device->api.vkCreateSampler(device->ctx.vkDevice, &samplerInfo, device->ctx.vkAllocator, &vkSampler));
    return SLANG_OK;
}

void Sampler::destroy()
{
    device->api.vkDestroySampler(device->ctx.vkDevice, vkSampler, device->ctx.vkAllocator);
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

// Result Device::createSampler(const SamplerDesc& desc, ISampler** outSampler)
// {
//     RefPtr<Sampler> sampler = new Sampler();
//     SLANG_RETURN_ON_FAIL(sampler->init(this, desc));
//     returnComPtr(outSampler, sampler);
//     return SLANG_OK;
// }

} // namespace slang::rhi::vulkan
