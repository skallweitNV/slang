#pragma once

#include "slang-rhi.h"

#include <vulkan/vulkan.h>

namespace slang::rhi::vulkan::utils {

Result handleError(VkResult result);

VkFilter translateFilter(TextureFilteringMode mode);
VkSamplerMipmapMode translateSamplerMipmapMode(TextureFilteringMode mode);
VkSamplerAddressMode translateSamplerAddressMode(TextureAddressingMode mode);
VkCompareOp translateCompareOp(ComparisonFunc func);
VkSamplerReductionMode translateSamplerReductionMode(TextureReductionOp op);

template<typename A, typename B>
void appendStruct(A& head, B& item)
{
    item.pNext = head.pNext;
    head.pNext = &item;
}

} // namespace slang::rhi::vulkan::utils

#define SLANG_VK_RETURN_ON_FAIL(x) { VkResult result_ = x; if (result_ != VK_SUCCESS) { return utils::handleError(result_); }  }

#define SLANG_VK_RETURN_NULL_ON_FAIL(x) { VkResult result_ = x; if (result_ != VK_SUCCESS) { utils::handleError(result_); return nullptr; }  }

/// Is similar to SLANG_VK_RETURN_ON_FAIL, but does not return. Will call checkFail on failure - which asserts on debug builds.
#define SLANG_VK_CHECK(x) {  VkResult _res = x; if (_res != VK_SUCCESS) { VulkanUtil::checkFail(_res); }  }
