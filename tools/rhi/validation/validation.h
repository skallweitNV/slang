#pragma once

#include "slang-rhi.h"
#include "../common.h"

namespace slang::rhi::validation {

class DeviceValidator : public IDevice, public Slang::ComObject {
public:
    SLANG_COM_OBJECT_IUNKNOWN_ADD_REF
    SLANG_COM_OBJECT_IUNKNOWN_RELEASE

    virtual SLANG_NO_THROW Result SLANG_MCALL queryInterface(SlangUUID const& uuid, void** outObject) override;

    virtual SLANG_NO_THROW DeviceInfo& SLANG_MCALL getInfo() override;

    IDevice* inner;
};

} // namespace slang::rhi::validation
