/*
 * SVGA3=VLKN - Vulkan Dispatch & Dynamic Loader Implementation
 */

#include "vlkn_dispatch.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include "vlkn_mock_dispatch.h"

bool vlkn_dispatch_init(VlknDispatchTable *table, bool forceMock) {
    memset(table, 0, sizeof(*table));
    if (forceMock) {
        vlkn_mock_dispatch_init(table);
        return true;
    }

    /* Auto-configure layer path if present in project */
    if (!getenv("VK_LAYER_PATH")) {
        if (access("/home/sull/Projects/SVGA3=VLKN/layers", F_OK) == 0) {
            setenv("VK_LAYER_PATH", "/home/sull/Projects/SVGA3=VLKN/layers:/home/sull/Projects/SVGA3=VLKN/layers_ext/usr/share/vulkan/explicit_layer.d", 0);
        }
    }

    void *handle = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        handle = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    }

    if (!handle) {
        /* Real Vulkan requested but library not found - FAIL */
        return false;
    }

    table->libHandle = handle;
    table->vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(handle, "vkGetInstanceProcAddr");
    if (!table->vkGetInstanceProcAddr) {
        dlclose(handle);
        table->libHandle = NULL;
        return false;
    }

    /* Load initial global functions (instance == VK_NULL_HANDLE) */
    table->vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)table->vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties");
    table->vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)table->vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    table->vkCreateInstance = (PFN_vkCreateInstance)table->vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");

    table->isMock = false;
    return true;
}

bool vlkn_dispatch_init_instance(VlknDispatchTable *table, VkInstance instance) {
    if (table->isMock) return true;
    if (!instance || !table->vkGetInstanceProcAddr) return false;

    table->vkDestroyInstance = (PFN_vkDestroyInstance)table->vkGetInstanceProcAddr(instance, "vkDestroyInstance");
    table->vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)table->vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
    table->vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)table->vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
    table->vkGetPhysicalDeviceFormatProperties = (PFN_vkGetPhysicalDeviceFormatProperties)table->vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties");
    table->vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)table->vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures");
    table->vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)table->vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties");
    table->vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)table->vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    table->vkCreateDevice = (PFN_vkCreateDevice)table->vkGetInstanceProcAddr(instance, "vkCreateDevice");

    if (!table->vkDestroyInstance || !table->vkEnumeratePhysicalDevices ||
        !table->vkGetPhysicalDeviceProperties || !table->vkGetPhysicalDeviceFormatProperties ||
        !table->vkGetPhysicalDeviceFeatures ||
        !table->vkGetPhysicalDeviceMemoryProperties ||
        !table->vkGetPhysicalDeviceQueueFamilyProperties || !table->vkCreateDevice) {
        return false;
    }

    table->vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)table->vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    table->vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)table->vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    return true;
}

bool vlkn_dispatch_init_device(VlknDispatchTable *table, VkInstance instance, VkDevice device) {
    if (table->isMock) return true;
    if (!device || !table->vkGetInstanceProcAddr) return false;

#define LOAD_DEV(fn) table->fn = (PFN_##fn)table->vkGetDeviceProcAddr(device, #fn); \
    if (!table->fn) table->fn = (PFN_##fn)table->vkGetInstanceProcAddr(instance, #fn); \
    if (!table->fn) return false;

    table->vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)table->vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
    table->vkDestroyDevice = (PFN_vkDestroyDevice)table->vkGetInstanceProcAddr(instance, "vkDestroyDevice");
    table->vkGetDeviceQueue = (PFN_vkGetDeviceQueue)table->vkGetInstanceProcAddr(instance, "vkGetDeviceQueue");
    if (!table->vkGetDeviceProcAddr || !table->vkDestroyDevice || !table->vkGetDeviceQueue) {
        return false;
    }

    /* Debug utils extensions (may be null if not enabled) */
    table->vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)table->vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    table->vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)table->vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    LOAD_DEV(vkCreateCommandPool);
    LOAD_DEV(vkDestroyCommandPool);
    LOAD_DEV(vkAllocateCommandBuffers);
    LOAD_DEV(vkFreeCommandBuffers);
    LOAD_DEV(vkBeginCommandBuffer);
    LOAD_DEV(vkEndCommandBuffer);
    LOAD_DEV(vkResetCommandBuffer);

    LOAD_DEV(vkCreateImage);
    LOAD_DEV(vkDestroyImage);
    LOAD_DEV(vkGetImageMemoryRequirements);
    LOAD_DEV(vkCreateImageView);
    LOAD_DEV(vkDestroyImageView);

    LOAD_DEV(vkCreateBuffer);
    LOAD_DEV(vkDestroyBuffer);
    LOAD_DEV(vkGetBufferMemoryRequirements);

    LOAD_DEV(vkAllocateMemory);
    LOAD_DEV(vkFreeMemory);
    LOAD_DEV(vkBindBufferMemory);
    LOAD_DEV(vkBindImageMemory);
    LOAD_DEV(vkMapMemory);
    LOAD_DEV(vkUnmapMemory);

    LOAD_DEV(vkCreateSampler);
    LOAD_DEV(vkDestroySampler);

    LOAD_DEV(vkCreateShaderModule);
    LOAD_DEV(vkDestroyShaderModule);
    LOAD_DEV(vkCreatePipelineLayout);
    LOAD_DEV(vkDestroyPipelineLayout);
    LOAD_DEV(vkCreateRenderPass);
    LOAD_DEV(vkDestroyRenderPass);
    LOAD_DEV(vkCreateFramebuffer);
    LOAD_DEV(vkDestroyFramebuffer);
    LOAD_DEV(vkCreateGraphicsPipelines);
    LOAD_DEV(vkDestroyPipeline);

    LOAD_DEV(vkCreateDescriptorSetLayout);
    LOAD_DEV(vkDestroyDescriptorSetLayout);
    LOAD_DEV(vkCreateDescriptorPool);
    LOAD_DEV(vkDestroyDescriptorPool);
    LOAD_DEV(vkAllocateDescriptorSets);
    LOAD_DEV(vkFreeDescriptorSets);
    LOAD_DEV(vkUpdateDescriptorSets);
    LOAD_DEV(vkCmdBindDescriptorSets);
    LOAD_DEV(vkCmdPushConstants);

    LOAD_DEV(vkCmdBeginRenderPass);
    LOAD_DEV(vkCmdEndRenderPass);
    LOAD_DEV(vkCmdBindPipeline);
    LOAD_DEV(vkCmdSetViewport);
    LOAD_DEV(vkCmdSetScissor);
    LOAD_DEV(vkCmdSetBlendConstants);
    LOAD_DEV(vkCmdSetDepthBias);
    LOAD_DEV(vkCmdSetStencilReference);
    LOAD_DEV(vkCmdSetStencilCompareMask);
    LOAD_DEV(vkCmdSetStencilWriteMask);
    LOAD_DEV(vkCmdBindVertexBuffers);
    LOAD_DEV(vkCmdBindIndexBuffer);
    LOAD_DEV(vkCmdDraw);
    LOAD_DEV(vkCmdDrawIndexed);
    LOAD_DEV(vkCmdClearColorImage);
    LOAD_DEV(vkCmdClearAttachments);
    LOAD_DEV(vkCmdPipelineBarrier);
    LOAD_DEV(vkCmdCopyBuffer);
    LOAD_DEV(vkCmdCopyBufferToImage);
    LOAD_DEV(vkCmdCopyImageToBuffer);
    LOAD_DEV(vkCmdCopyImage);
    LOAD_DEV(vkCmdBlitImage);

    LOAD_DEV(vkCreateQueryPool);
    LOAD_DEV(vkDestroyQueryPool);
    LOAD_DEV(vkCmdBeginQuery);
    LOAD_DEV(vkCmdEndQuery);
    LOAD_DEV(vkCmdResetQueryPool);
    LOAD_DEV(vkGetQueryPoolResults);

    LOAD_DEV(vkCreateFence);
    LOAD_DEV(vkDestroyFence);
    LOAD_DEV(vkWaitForFences);
    LOAD_DEV(vkResetFences);
    LOAD_DEV(vkQueueSubmit);
    LOAD_DEV(vkQueueWaitIdle);
    LOAD_DEV(vkDeviceWaitIdle);

#undef LOAD_DEV
    return true;
}

void vlkn_dispatch_cleanup(VlknDispatchTable *table) {
    if (table->libHandle) {
        dlclose(table->libHandle);
        table->libHandle = NULL;
    }
}
