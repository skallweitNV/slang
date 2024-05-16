#define SLANG_RHI_VULKAN
#include "slang-rhi.h"

using namespace slang::rhi;

int main()
{
    ComPtr<IFactory> factory = createFactory(GraphicsAPI::Vulkan);
    if (!factory)
    {
        printf("Failed to create factory\n");
        return 1;
    }

    IAdapter* adapter;
    Index adapterIndex = 0;
    while (factory->enumAdapter(adapterIndex++, &adapter) == SLANG_OK)
    {
        const AdapterInfo& info = adapter->getInfo();
        printf("name = %s\n", info.name);
        printf("vendorID = %08x\n", info.vendorID);
        printf("deviceID = %08x\n", info.deviceID);
        auto l = info.luid.luid;
        printf(
            "luid = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
            l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7], l[8], l[9], l[10], l[11], l[12], l[13], l[14], l[15]
        );
        printf("isSoftware = %d\n", info.isSoftware);
    }

    DeviceDesc deviceDesc = {};
    DeviceDescVulkan deviceDescVulkan = {};
    // deviceDesc.next = &deviceDescVulkan;
    ComPtr<IDevice> device = factory->createDevice(deviceDesc, nullptr);
    if (!device)
    {
        printf("Failed to create device\n");
        return 1;
    }
    ComPtr<IDeviceVulkan> deviceVulkan;
    device->queryInterface(IDeviceVulkan::getTypeGuid(), (void**)deviceVulkan.writeRef());

    return 0;
}
