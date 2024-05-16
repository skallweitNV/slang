#pragma once

#include "core/slang-com-object.h"
#include "core/slang-list.h"
#include "core/slang-math.h"

namespace slang::rhi {

using Slang::RefPtr;
using Slang::ComObject;
using Slang::List;
using Slang::Math;

template<typename To, typename From>
To checked_cast(From from)
{
    static_assert(!std::is_same_v<To, From>, "redundant checked_cast");
    assert(from == nullptr || dynamic_cast<To>(from) != nullptr);
    return static_cast<To>(from);
}

/// All descriptor structs start with a StructHeader, containing the struct type
/// and a pointer to the next struct, forming a linked list of structs.
/// This function iterates through the list and returns the first struct of the given type, or nullptr.
template<typename T>
const T* findStructInChain(void* next)
{
    StructType structType = T().structType;
    while (next)
    {
        StructHeader* header = (StructHeader*)next;
        if (header->structType == structType)
        {
            return (const T*)next;
        }
        next = header->next;
    }
    return nullptr;
}

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
