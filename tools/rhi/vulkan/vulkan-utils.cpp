#include "vulkan-utils.h"

namespace slang::rhi::vulkan::utils {

Result handleError(VkResult result)
{
    // assert(!"Vulkan error");
    return (result == VK_SUCCESS) ? SLANG_OK : SLANG_FAIL;
}

VkFilter translateFilter(TextureFilteringMode mode)
{
    switch (mode)
    {
    case TextureFilteringMode::Linear:
        return VK_FILTER_LINEAR;
    case TextureFilteringMode::Point:
        return VK_FILTER_NEAREST;
    default:
        return VkFilter(0);
    }
}

VkSamplerMipmapMode translateSamplerMipmapMode(TextureFilteringMode mode)
{
    switch (mode)
    {
    case TextureFilteringMode::Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    case TextureFilteringMode::Point:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    default:
        return VkSamplerMipmapMode(0);
    }
}

VkSamplerAddressMode translateSamplerAddressMode(TextureAddressingMode mode)
{
    switch (mode)
    {
    case TextureAddressingMode::Wrap:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case TextureAddressingMode::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case TextureAddressingMode::ClampToBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case TextureAddressingMode::MirrorRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case TextureAddressingMode::MirrorOnce:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    default:
        return VkSamplerAddressMode(0);
    }
}

VkCompareOp translateCompareOp(ComparisonFunc func)
{
    switch (func)
    {
    case ComparisonFunc::Never:
        return VK_COMPARE_OP_NEVER;
    case ComparisonFunc::Less:
        return VK_COMPARE_OP_LESS;
    case ComparisonFunc::Equal:
        return VK_COMPARE_OP_EQUAL;
    case ComparisonFunc::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case ComparisonFunc::Greater:
        return VK_COMPARE_OP_GREATER;
    case ComparisonFunc::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case ComparisonFunc::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case ComparisonFunc::Always:
        return VK_COMPARE_OP_ALWAYS;
    default:
        return  VkCompareOp(0);
    }
}

VkSamplerReductionMode translateSamplerReductionMode(TextureReductionOp op)
{
    switch (op)
    {
    case TextureReductionOp::Minimum:
        return VK_SAMPLER_REDUCTION_MODE_MIN;
    case TextureReductionOp::Maximum:
        return VK_SAMPLER_REDUCTION_MODE_MAX;
    default:
        return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
    }
}


} // namespace slang::rhi::vulkan::utils
