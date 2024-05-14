#include "vulkan-api.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#if SLANG_WINDOWS_FAMILY
#   include <windows.h>
#else
#   include <dlfcn.h>
#endif

namespace slang::rhi::vulkan {

// ----------------------------------------------------------------------------
// VulkanModule
// ----------------------------------------------------------------------------

Result VulkanModule::load(bool useSoftwareImpl)
{
    if (m_module)
    {
        unload();
    }

    m_isSoftwareImpl = useSoftwareImpl;

#if SLANG_WINDOWS_FAMILY
    const char* libraryName = useSoftwareImpl ? "vk_swiftshader.dll" : "vulkan-1.dll";
    m_module = (void*)::LoadLibraryA(libraryName);
#elif SLANG_APPLE_FAMILY
    const char* libraryName = useSoftwareImpl ? "libvk_swiftshader.dylib" : "libvulkan.1.dylib";
    m_module = dlopen(libraryName, RTLD_NOW | RTLD_GLOBAL);
#else
    const char* libraryName = useSoftwareImpl ? "libvk_swiftshader.so" : "libvulkan.so.1";
    if (useSoftwareImpl)
    {
        dlopen("libpthread.so.0", RTLD_NOW | RTLD_GLOBAL);
    }
    m_module = dlopen(libraryName, RTLD_NOW);
#endif

    return m_module ? SLANG_OK : SLANG_FAIL;
}

void VulkanModule::unload()
{
    if (!m_module)
    {
        return;
    }

#if SLANG_WINDOWS_FAMILY
    ::FreeLibrary((HMODULE)m_module);
#else
    dlclose(m_module);
#endif
    m_module = nullptr;
}

PFN_vkVoidFunction VulkanModule::getFunction(const char* name) const
{
    if (!m_module)
    {
        return nullptr;
    }
#if SLANG_WINDOWS_FAMILY
    return (PFN_vkVoidFunction)::GetProcAddress((HMODULE)m_module, name);
#else
    return (PFN_vkVoidFunction)dlsym(m_module, name);
#endif
}

// ----------------------------------------------------------------------------
// VulkanApi
// ----------------------------------------------------------------------------

#define VULKAN_API_CHECK_FUNCTION(x) && (x != nullptr)
#define VULKAN_API_CHECK_FUNCTIONS(FUNCTION_LIST) true FUNCTION_LIST(VULKAN_API_CHECK_FUNCTION)

bool VulkanApi::isDefined(ProcType type) const
{
    switch (type)
    {
    case ProcType::Global:
        return VULKAN_API_CHECK_FUNCTIONS(VULKAN_API_ALL_GLOBAL_PROCS);
    case ProcType::Instance:
        return VULKAN_API_CHECK_FUNCTIONS(VULKAN_API_ALL_INSTANCE_PROCS);
    case ProcType::Device:
        return VULKAN_API_CHECK_FUNCTIONS(VULKAN_API_DEVICE_PROCS);
    }
    return false;
}

Result VulkanApi::initGlobalProcs(const VulkanModule& module)
{
#define VULKAN_API_GET_GLOBAL_PROC(x) x = (PFN_##x)module.getFunction(#x);

    // Initialize all the global functions
    VULKAN_API_ALL_GLOBAL_PROCS(VULKAN_API_GET_GLOBAL_PROC)

    if (!isDefined(ProcType::Global))
    {
        return SLANG_FAIL;
    }
    m_module = &module;
    return SLANG_OK;
}

Result VulkanApi::initInstanceProcs(VkInstance instance)
{
    assert(instance && vkGetInstanceProcAddr != nullptr);

#define VULKAN_API_GET_INSTANCE_PROC(x) x = (PFN_##x)vkGetInstanceProcAddr(instance, #x);

    VULKAN_API_ALL_INSTANCE_PROCS(VULKAN_API_GET_INSTANCE_PROC)

    // Get optional 
    VULKAN_API_INSTANCE_PROCS_OPT(VULKAN_API_GET_INSTANCE_PROC)

    if (!isDefined(ProcType::Instance))
    {
        return SLANG_FAIL;
    }

    m_instance = instance;
    return SLANG_OK;
}

Result VulkanApi::initPhysicalDevice(VkPhysicalDevice physicalDevice)
{
    assert(m_physicalDevice == VK_NULL_HANDLE);
    m_physicalDevice = physicalDevice;

    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_deviceMemoryProperties);

    return SLANG_OK;
}

Result VulkanApi::initDeviceProcs(VkDevice device)
{
    assert(m_instance && device && vkGetDeviceProcAddr != nullptr);

#define VULKAN_API_GET_DEVICE_PROC(x) x = (PFN_##x)vkGetDeviceProcAddr(device, #x);

    VULKAN_API_ALL_DEVICE_PROCS(VULKAN_API_GET_DEVICE_PROC)

    if (!isDefined(ProcType::Device))
    {
        return SLANG_FAIL;
    }

    if (!vkGetBufferDeviceAddressKHR && vkGetBufferDeviceAddressEXT)
        vkGetBufferDeviceAddressKHR = vkGetBufferDeviceAddressEXT;
    if (!vkGetBufferDeviceAddress && vkGetBufferDeviceAddressKHR)
        vkGetBufferDeviceAddress = vkGetBufferDeviceAddressKHR;
    if (!vkGetSemaphoreCounterValue && vkGetSemaphoreCounterValueKHR)
        vkGetSemaphoreCounterValue = vkGetSemaphoreCounterValueKHR;
    if (!vkSignalSemaphore && vkSignalSemaphoreKHR)
        vkSignalSemaphore = vkSignalSemaphoreKHR;
    m_device = device;
    
    return SLANG_OK;
}


} // slang::rhi::vulkan
