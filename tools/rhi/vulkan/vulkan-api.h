#pragma once

#include "slang-rhi.h"

#if SLANG_WINDOWS_FAMILY
#   define VK_USE_PLATFORM_WIN32_KHR 1
#elif SLANG_LINUX_FAMILY
#   define VK_USE_PLATFORM_XLIB_KHR 1
#elif SLANG_APPLE_FAMILY
#   define VK_USE_PLATFORM_METAL_EXT 1
#else
#   error "Platform not supported"
#endif

#define VK_NO_PROTOTYPES

#include <vulkan/vulkan.h>

// Undef xlib macros
#ifdef Always
#    undef Always
#endif
#ifdef None
#    undef None
#endif

namespace slang::rhi::vulkan {

// ----------------------------------------------------------------------------
// VulkanModule
// ----------------------------------------------------------------------------

class VulkanModule
{
public:
    ~VulkanModule() { unload(); }

    Result load(bool useSoftwareImpl);
    void unload();

    bool isAvailable() const { return m_module != nullptr; }
    bool isSoftwareImpl() const { return m_isSoftwareImpl; }

    PFN_vkVoidFunction getFunction(const char* name) const;

private:
    void* m_module = nullptr;
    bool m_isSoftwareImpl = false;
};

#define VULKAN_API_GLOBAL_PROCS(x) \
    x(vkGetInstanceProcAddr) \
    x(vkCreateInstance) \
    x(vkEnumerateInstanceLayerProperties) \
    x(vkEnumerateDeviceExtensionProperties) \
    x(vkDestroyInstance) \
    /* */

#define VULKAN_API_INSTANCE_PROCS_OPT(x) \
    x(vkGetPhysicalDeviceFeatures2) \
    x(vkGetPhysicalDeviceProperties2) \
    x(vkCreateDebugReportCallbackEXT) \
    x(vkDestroyDebugReportCallbackEXT) \
    x(vkDebugReportMessageEXT) \
    /* */

#define VULKAN_API_INSTANCE_PROCS(x) \
    x(vkCreateDevice) \
    x(vkDestroyDevice) \
    x(vkEnumeratePhysicalDevices) \
    x(vkGetPhysicalDeviceProperties) \
    x(vkGetPhysicalDeviceFeatures) \
    x(vkGetPhysicalDeviceMemoryProperties) \
    x(vkGetPhysicalDeviceQueueFamilyProperties) \
    x(vkGetPhysicalDeviceFormatProperties) \
    x(vkGetDeviceProcAddr) \
    /* */

#define VULKAN_API_DEVICE_PROCS(x) \
    x(vkCreateDescriptorPool) \
    x(vkDestroyDescriptorPool) \
    x(vkResetDescriptorPool) \
    x(vkGetDeviceQueue) \
    x(vkQueueSubmit) \
    x(vkQueueWaitIdle) \
    x(vkCreateBuffer) \
    x(vkAllocateMemory) \
    x(vkMapMemory) \
    x(vkUnmapMemory) \
    x(vkCmdCopyBuffer) \
    x(vkDestroyBuffer) \
    x(vkFreeMemory) \
    x(vkCreateDescriptorSetLayout) \
    x(vkDestroyDescriptorSetLayout) \
    x(vkAllocateDescriptorSets) \
    x(vkFreeDescriptorSets) \
    x(vkUpdateDescriptorSets) \
    x(vkCreatePipelineLayout) \
    x(vkDestroyPipelineLayout) \
    x(vkCreateComputePipelines) \
    x(vkCreateGraphicsPipelines) \
    x(vkDestroyPipeline) \
    x(vkCreateShaderModule) \
    x(vkDestroyShaderModule) \
    x(vkCreateFramebuffer) \
    x(vkDestroyFramebuffer) \
    x(vkCreateImage) \
    x(vkDestroyImage) \
    x(vkCreateImageView) \
    x(vkDestroyImageView) \
    x(vkCreateRenderPass) \
    x(vkDestroyRenderPass) \
    x(vkCreateCommandPool) \
    x(vkDestroyCommandPool) \
    x(vkCreateSampler) \
    x(vkDestroySampler) \
    x(vkCreateBufferView) \
    x(vkDestroyBufferView) \
    \
    x(vkGetBufferMemoryRequirements) \
    x(vkGetImageMemoryRequirements) \
    \
    x(vkCmdBindPipeline) \
    x(vkCmdClearAttachments) \
    x(vkCmdClearColorImage) \
    x(vkCmdClearDepthStencilImage) \
    x(vkCmdFillBuffer) \
    x(vkCmdBindDescriptorSets) \
    x(vkCmdDispatch) \
    x(vkCmdDraw) \
    x(vkCmdDrawIndexed) \
    x(vkCmdDrawIndirect) \
    x(vkCmdDrawIndexedIndirect) \
    x(vkCmdSetScissor) \
    x(vkCmdSetViewport) \
    x(vkCmdBindVertexBuffers) \
    x(vkCmdBindIndexBuffer) \
    x(vkCmdBeginRenderPass) \
    x(vkCmdEndRenderPass) \
    x(vkCmdPipelineBarrier) \
    x(vkCmdCopyBufferToImage)\
    x(vkCmdCopyImage) \
    x(vkCmdCopyImageToBuffer) \
    x(vkCmdResolveImage) \
    x(vkCmdPushConstants) \
    x(vkCmdSetStencilReference) \
    x(vkCmdWriteTimestamp) \
    x(vkCmdBeginQuery) \
    x(vkCmdEndQuery) \
    x(vkCmdResetQueryPool) \
    x(vkCmdCopyQueryPoolResults) \
    \
    x(vkCreateFence) \
    x(vkDestroyFence) \
    x(vkResetFences) \
    x(vkGetFenceStatus) \
    x(vkWaitForFences) \
    \
    x(vkCreateSemaphore) \
    x(vkDestroySemaphore) \
    \
    x(vkCreateEvent) \
    x(vkDestroyEvent) \
    x(vkGetEventStatus) \
    x(vkSetEvent) \
    x(vkResetEvent) \
    \
    x(vkFreeCommandBuffers) \
    x(vkAllocateCommandBuffers) \
    x(vkBeginCommandBuffer) \
    x(vkEndCommandBuffer) \
    x(vkResetCommandBuffer) \
    x(vkResetCommandPool) \
    \
    x(vkBindImageMemory) \
    x(vkBindBufferMemory) \
    \
    x(vkCreateQueryPool) \
    x(vkGetQueryPoolResults) \
    x(vkDestroyQueryPool) \
    /* */

#if SLANG_WINDOWS_FAMILY
#   define VULKAN_API_INSTANCE_PLATFORM_KHR_PROCS(x)          \
    x(vkCreateWin32SurfaceKHR) \
    /* */
#elif SLANG_APPLE_FAMILY
#   define VULKAN_API_INSTANCE_PLATFORM_KHR_PROCS(x)          \
    x(vkCreateMetalSurfaceEXT) \
    /* */
#elif SLANG_ENABLE_XLIB
#   define VULKAN_API_INSTANCE_PLATFORM_KHR_PROCS(x)          \
    x(vkCreateXlibSurfaceKHR) \
    /* */
#else
#   define VULKAN_API_INSTANCE_PLATFORM_KHR_PROCS(x)          \
    /* */
#endif

#define VULKAN_API_INSTANCE_KHR_PROCS(x)          \
    VULKAN_API_INSTANCE_PLATFORM_KHR_PROCS(x) \
    x(vkGetPhysicalDeviceSurfaceSupportKHR) \
    x(vkGetPhysicalDeviceSurfaceFormatsKHR) \
    x(vkGetPhysicalDeviceSurfacePresentModesKHR) \
    x(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    x(vkDestroySurfaceKHR) \

    /* */

#define VULKAN_API_DEVICE_KHR_PROCS(x) \
    x(vkQueuePresentKHR) \
    x(vkCreateSwapchainKHR) \
    x(vkGetSwapchainImagesKHR) \
    x(vkDestroySwapchainKHR) \
    x(vkAcquireNextImageKHR) \
    x(vkCreateRayTracingPipelinesKHR) \
    x(vkCmdTraceRaysKHR) \
    x(vkGetRayTracingShaderGroupHandlesKHR) \
    /* */

#if SLANG_WINDOWS_FAMILY
#   define VULKAN_API_DEVICE_PLATFORM_OPT_PROCS(x) \
    x(vkGetMemoryWin32HandleKHR) \
    x(vkGetSemaphoreWin32HandleKHR) \
    /* */
#else
#   define VULKAN_API_DEVICE_PLATFORM_OPT_PROCS(x) \
    x(vkGetMemoryFdKHR) \
    x(vkGetSemaphoreFdKHR) \
    /* */
#endif

#define VULKAN_API_DEVICE_OPT_PROCS(x) \
    VULKAN_API_DEVICE_PLATFORM_OPT_PROCS(x) \
    x(vkCmdSetPrimitiveTopologyEXT) \
    x(vkGetBufferDeviceAddress) \
    x(vkGetBufferDeviceAddressKHR) \
    x(vkGetBufferDeviceAddressEXT) \
    x(vkCmdBuildAccelerationStructuresKHR) \
    x(vkCmdCopyAccelerationStructureKHR) \
    x(vkCmdCopyAccelerationStructureToMemoryKHR) \
    x(vkCmdCopyMemoryToAccelerationStructureKHR) \
    x(vkCmdWriteAccelerationStructuresPropertiesKHR) \
    x(vkCreateAccelerationStructureKHR) \
    x(vkDestroyAccelerationStructureKHR) \
    x(vkGetAccelerationStructureBuildSizesKHR) \
    x(vkGetSemaphoreCounterValue) \
    x(vkGetSemaphoreCounterValueKHR) \
    x(vkSignalSemaphore) \
    x(vkSignalSemaphoreKHR) \
    x(vkWaitSemaphores) \
    x(vkWaitSemaphoresKHR) \
    x(vkCmdSetSampleLocationsEXT) \
    x(vkCmdDebugMarkerBeginEXT) \
    x(vkCmdDebugMarkerEndEXT) \
    x(vkDebugMarkerSetObjectNameEXT) \
    x(vkCmdDrawMeshTasksEXT) \
    /* */

#define VULKAN_API_ALL_GLOBAL_PROCS(x) \
    VULKAN_API_GLOBAL_PROCS(x)

#define VULKAN_API_ALL_INSTANCE_PROCS(x) \
    VULKAN_API_INSTANCE_PROCS(x) \
    VULKAN_API_INSTANCE_KHR_PROCS(x)

#define VULKAN_API_ALL_DEVICE_PROCS(x) \
    VULKAN_API_DEVICE_PROCS(x) \
    VULKAN_API_DEVICE_KHR_PROCS(x) \
    VULKAN_API_DEVICE_OPT_PROCS(x)

#define VULKAN_API_ALL_PROCS(x) \
    VULKAN_API_ALL_GLOBAL_PROCS(x) \
    VULKAN_API_ALL_INSTANCE_PROCS(x) \
    VULKAN_API_ALL_DEVICE_PROCS(x) \
    \
    VULKAN_API_INSTANCE_PROCS_OPT(x) \
    /* */

#define VULKAN_API_DECLARE_PROC(NAME) PFN_##NAME NAME = nullptr;


struct VulkanExtendedFeatureProperties
{
    // 16 bit storage features
    VkPhysicalDevice16BitStorageFeatures storage16BitFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR
    };
    // Atomic Float features
    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT
    };
    VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT atomicFloat2Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT
    };
    // Image int64 atomic features
    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT imageInt64AtomicFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT
    };
    // Extended dynamic state features
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT
    };
    // Acceleration structure features
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
    };
    // Ray tracing pipeline features
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
    };
    // Ray query (inline ray-tracing) features
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR
    };
    // Inline uniform block features
    VkPhysicalDeviceInlineUniformBlockFeaturesEXT inlineUniformBlockFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT
    };
    // Robustness2 features
    VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT
    };

    VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV rayTracingInvocationReorderFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV
    };

    VkPhysicalDeviceVariablePointerFeaturesKHR variablePointersFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES_KHR
    };

    // Clock features
    VkPhysicalDeviceShaderClockFeaturesKHR clockFeatures = { 
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR
    };

    // Mesh shader features
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
    };

    // Multiview features
    VkPhysicalDeviceMultiviewFeaturesKHR multiviewFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR 
    };

    // Fragment shading rate features
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR 
    };

    // Vulkan 1.2 features.
    VkPhysicalDeviceVulkan12Features vulkan12Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
    };
};

// ----------------------------------------------------------------------------
// VulkanApi
// ----------------------------------------------------------------------------

struct VulkanApi
{
    VULKAN_API_ALL_PROCS(VULKAN_API_DECLARE_PROC)

    enum class ProcType
    {
        Global,
        Instance,
        Device,
    };

    /// Returns true if all the functions in the category are defined.
    bool isDefined(ProcType type) const;

    /// Initialize global functions.
    Result initGlobalProcs(const VulkanModule& module);
    /// Initialize instance functions.
    Result initInstanceProcs(VkInstance instance);

    /// Called before initDevice
    Result initPhysicalDevice(VkPhysicalDevice physicalDevice);

    /// Initialize the device functions
    Result initDeviceProcs(VkDevice device);

    const VulkanModule* m_module = nullptr;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties                              m_deviceProperties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR         m_rtProperties;
    VkPhysicalDeviceFeatures                                m_deviceFeatures;
    VkPhysicalDeviceMemoryProperties                        m_deviceMemoryProperties;
    VulkanExtendedFeatureProperties                         m_extendedFeatures;
};


} // slang::rhi::vulkan
