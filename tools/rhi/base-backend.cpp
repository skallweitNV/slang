#include "base-backend.h"

namespace slang::rhi {

// ----------------------------------------------------------------------------
// SamplerBase
// ----------------------------------------------------------------------------

ISlangUnknown* SamplerBase::getInterface(const Slang::Guid& guid)
{
    if (guid == ISlangUnknown::getTypeGuid() || guid == IResource::getTypeGuid() || guid == ISampler::getTypeGuid())
    {
        return this;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// DeviceBase
// ----------------------------------------------------------------------------

ISlangUnknown* DeviceBase::getInterface(const Slang::Guid& guid)
{
    if (guid == ISlangUnknown::getTypeGuid() || guid == IAdapter::getTypeGuid())
    {
        return this;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// AdapterBase
// ----------------------------------------------------------------------------

ISlangUnknown* AdapterBase::getInterface(const Slang::Guid& guid)
{
    if (guid == ISlangUnknown::getTypeGuid() || guid == IAdapter::getTypeGuid())
    {
        return this;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// FactoryBase
// ----------------------------------------------------------------------------

ISlangUnknown* FactoryBase::getInterface(const Slang::Guid& guid)
{
    if (guid == ISlangUnknown::getTypeGuid() || guid == IFactory::getTypeGuid())
    {
        return this;
    }
    return nullptr;
}

Result FactoryBase::enumAdapter(Index index, IAdapter** outAdapter)
{
    if (index >= 0 && index < adapters.getCount())
    {
        returnComPtr(outAdapter, adapters[index]);
        return SLANG_OK;
    }
    return SLANG_E_INVALID_ARG;
}

} // namespace slang::rhi
