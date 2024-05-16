#pragma once

#include "slang-rhi.h"
#include "../common.h"

namespace slang::rhi::validation {

class Device : public IDevice, public Slang::ComObject {
public:
    SLANG_COM_OBJECT_IUNKNOWN_ADD_REF
    SLANG_COM_OBJECT_IUNKNOWN_RELEASE

    virtual SLANG_NO_THROW Result SLANG_MCALL queryInterface(SlangUUID const& uuid, void** outObject) override;

    virtual SLANG_NO_THROW const DeviceInfo& SLANG_MCALL getInfo() const override;

    virtual SLANG_NO_THROW Result SLANG_MCALL createBuffer(const BufferDesc& desc, IBuffer** outBuffer) override;

    virtual SLANG_NO_THROW Result SLANG_MCALL createTexture(const TextureDesc& desc, ITexture** outTexture) override;

    virtual SLANG_NO_THROW Result SLANG_MCALL createSampler(const SamplerDesc& desc, ISampler** outSampler) override;

    virtual SLANG_NO_THROW Result SLANG_MCALL createFence(const FenceDesc& desc, IFence** outFence) override;

    ComPtr<IDevice> inner;
};

} // namespace slang::rhi::validation
