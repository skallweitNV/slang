#include "vulkan-backend.h"
#include "vulkan-utils.h"

namespace slang::rhi::vulkan {

inline AdapterLUID getAdapterLUID(const VulkanApi& api, VkPhysicalDevice physicalDevice)
{
    AdapterLUID luid = {};

    VkPhysicalDeviceIDPropertiesKHR idProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR };
    VkPhysicalDeviceProperties2 props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    props.pNext = &idProps;
    SLANG_ASSERT(api.vkGetPhysicalDeviceFeatures2);
    api.vkGetPhysicalDeviceProperties2(physicalDevice, &props);
    if (idProps.deviceLUIDValid)
    {
        SLANG_ASSERT(sizeof(AdapterLUID) >= VK_LUID_SIZE);
        ::memcpy(&luid, idProps.deviceLUID, VK_LUID_SIZE);
    }
    else
    {
        SLANG_ASSERT(sizeof(AdapterLUID) >= VK_UUID_SIZE);
        ::memcpy(&luid, idProps.deviceUUID, VK_UUID_SIZE);
    }

    return luid;
}

inline Result enumerateAdapters(List<RefPtr<AdapterBase>>& outAdapters)
{
    for (int useSoftware = 0; useSoftware <= 1; useSoftware++)
    {
        VulkanModule module;
        if (module.load(useSoftware != 0) != SLANG_OK)
            continue;
        VulkanApi api;
        if (api.initGlobalProcs(module) != SLANG_OK)
            continue;

        VkInstance instance;
        {
            VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
            const char* enabledExtensionNames[] = {
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            };
            createInfo.enabledExtensionCount = SLANG_COUNT_OF(enabledExtensionNames);
            createInfo.ppEnabledExtensionNames = &enabledExtensionNames[0];
            SLANG_VK_RETURN_ON_FAIL(api.vkCreateInstance(&createInfo, nullptr, &instance));
        }

        // This will fail due to not loading any extensions.
        api.initInstanceProcs(instance);

        // Make sure required functions for enumerating physical devices were loaded.
        if (api.vkEnumeratePhysicalDevices || api.vkGetPhysicalDeviceProperties)
        {
            uint32_t physicalDeviceCount = 0;
            SLANG_VK_RETURN_ON_FAIL(api.vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));

            List<VkPhysicalDevice> physicalDevices;
            physicalDevices.setCount(physicalDeviceCount);
            SLANG_VK_RETURN_ON_FAIL(api.vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.getBuffer()));

            for (const auto& physicalDevice : physicalDevices)
            {
                RefPtr<Adapter> adapter = new Adapter();
                AdapterInfo& info = adapter->info;

                VkPhysicalDeviceProperties props;
                api.vkGetPhysicalDeviceProperties(physicalDevice, &props);
                copyString(info.name, props.deviceName);
                info.vendorID = props.vendorID;
                info.deviceID = props.deviceID;
                info.luid = getAdapterLUID(api, physicalDevice);

                outAdapters.add(adapter);
            }
        }

        api.vkDestroyInstance(instance, nullptr);
        module.unload();
    }

    return SLANG_OK;
}

Result Factory::init()
{
    SLANG_RETURN_ON_FAIL(enumerateAdapters(adapters));
    return SLANG_OK;
}

Result Factory::createDevice(const DeviceDesc* desc, IAdapter* adapter, IDevice** outDevice)
{
    return SLANG_E_NOT_IMPLEMENTED;
}

} // slang::rhi::vulkan
