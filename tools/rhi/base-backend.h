#pragma once

#include "slang-rhi.h"
#include "common.h"

namespace slang::rhi {

template<typename Interface>
class ResourceBase : public Interface, public Slang::ComObject
{
public:
    SLANG_COM_OBJECT_IUNKNOWN_ALL

    virtual ISlangUnknown* getInterface(const Slang::Guid& guid)
    {
        if (guid == ISlangUnknown::getTypeGuid() || guid == IResource::getTypeGuid())
        {
            return this;
        }
        return nullptr;
    }

    virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(NativeHandleType type, NativeHandle* outHandle) const override
    {
        return SLANG_E_NOT_AVAILABLE;
    }
};

class SamplerBase : public ResourceBase<ISampler>
{
public:
    SLANG_COM_OBJECT_IUNKNOWN_ALL

    virtual ISlangUnknown* getInterface(const Slang::Guid& guid) override;

    virtual SLANG_NO_THROW const SamplerDesc& SLANG_MCALL getDesc() const override { return desc; }

    SamplerDesc desc;
};

class DeviceBase : public IDevice, public Slang::ComObject
{
public:
    SLANG_COM_OBJECT_IUNKNOWN_ALL

    virtual ISlangUnknown* getInterface(const Slang::Guid& guid);

    Result init(const DeviceDesc& desc)
    {
        return SLANG_OK;
    }

    virtual SLANG_NO_THROW const DeviceInfo& SLANG_MCALL getInfo() const override { return info; }

    virtual SLANG_NO_THROW Result SLANG_MCALL createBuffer(const BufferDesc& desc, IBuffer** outBuffer) override { return SLANG_E_NOT_AVAILABLE; }

    virtual SLANG_NO_THROW Result SLANG_MCALL createTexture(const TextureDesc& desc, ITexture** outTexture) override { return SLANG_E_NOT_AVAILABLE; }

    virtual SLANG_NO_THROW Result SLANG_MCALL createSampler(const SamplerDesc& desc, ISampler** outSampler) override { return SLANG_E_NOT_AVAILABLE; }

    virtual SLANG_NO_THROW Result SLANG_MCALL createFence(const FenceDesc& desc, IFence** outFence) override { return SLANG_E_NOT_AVAILABLE; }

    DeviceInfo info;
};

class AdapterBase : public IAdapter, public Slang::ComObject
{
public:
    SLANG_COM_OBJECT_IUNKNOWN_ALL

    virtual ISlangUnknown* getInterface(const Slang::Guid& guid);

    virtual SLANG_NO_THROW AdapterInfo& SLANG_MCALL getInfo() override { return info; }

    AdapterInfo info;
};

class FactoryBase : public IFactory, public Slang::ComObject
{
public:
    SLANG_COM_OBJECT_IUNKNOWN_ALL

    virtual ISlangUnknown* getInterface(const Slang::Guid& guid);

    virtual Result SLANG_MCALL enumAdapter(Index index, IAdapter** outAdapter) override;

    List<RefPtr<AdapterBase>> adapters;
};

} // namespace slang::rhi
