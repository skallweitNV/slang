#include "slang-rhi.h"
#include "common.h"

#include "vulkan/vulkan-backend.h"

namespace slang::rhi {

inline Result createFactory(GraphicsAPI api, IFactory** outFactory)
{
    switch (api)
    {
#if SLANG_WINDOWS_FAMILY
    case GraphicsAPI::D3D12:
        return d3d12::createFactory(outFactory);
#endif
#if SLANG_WINDOWS_FAMILY || SLANG_LINUX_FAMILY || SLANG_APPLE_FAMILY
    case GraphicsAPI::Vulkan:
        return vulkan::createFactory(outFactory);
#endif
    default:
        break;
    }

    return SLANG_E_NOT_AVAILABLE;
}

} // namespace slang::rhi

extern "C"
{

slang::rhi::Result slangRhiCreateFactory(slang::rhi::GraphicsAPI api, slang::rhi::IFactory** outFactory)
{
    return createFactory(api, outFactory);
}
    
} // extern "C"
