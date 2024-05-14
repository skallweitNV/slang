#pragma once

#include "core/slang-com-object.h"
#include "core/slang-list.h"

namespace slang::rhi {

using Slang::RefPtr;
using Slang::ComObject;
using Slang::List;

template<size_t N>
void copyString(char(&dst)[N], const char* src)
{
    ::strncpy((char*)&dst[0], src, N);
    dst[N - 1] = 0;
}

// Helpers for returning an object implementation as COM pointer.
template<typename TInterface, typename TImpl>
void returnComPtr(TInterface** outInterface, TImpl* rawPtr)
{
    static_assert(
        !std::is_base_of<Slang::RefObject, TInterface>::value,
        "TInterface must be an interface type.");
    rawPtr->addRef();
    *outInterface = rawPtr;
}

template <typename TInterface, typename TImpl>
void returnComPtr(TInterface** outInterface, const Slang::RefPtr<TImpl>& refPtr)
{
    static_assert(
        !std::is_base_of<Slang::RefObject, TInterface>::value,
        "TInterface must be an interface type.");
    refPtr->addRef();
    *outInterface = refPtr.Ptr();
}

template <typename TInterface, typename TImpl>
void returnComPtr(TInterface** outInterface, Slang::ComPtr<TImpl>& comPtr)
{
    static_assert(
        !std::is_base_of<Slang::RefObject, TInterface>::value,
        "TInterface must be an interface type.");
    *outInterface = comPtr.detach();
}

} // namespace slang::rhi
