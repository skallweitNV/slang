#include "validation.h"

namespace slang::rhi::validation {

Result DeviceValidator::queryInterface(SlangUUID const& uuid, void** outObject)
{
    return inner->queryInterface(uuid, outObject);
}

DeviceInfo& DeviceValidator::getInfo()
{
    return inner->getInfo();
}

} // namespace slang::rhi::validation
