/*
 * SVGA3=VLKN - Vulkan Backend Device & Memory Management Implementation
 */

#include "vlkn_backend.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>

extern "C" void log_msg(const char *fmt, ...);

namespace svga3_vlkn {

static VKAPI_ATTR VkBool32 VKAPI_CALL vlkn_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    (void)type;
    VlknBackend *backend = static_cast<VlknBackend*>(pUserData);
    bool isError = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
    std::string msg = (pCallbackData && pCallbackData->pMessage) ? pCallbackData->pMessage : "Unknown validation message";
    if (backend) {
        backend->addValidationMessage(isError, msg);
    }
    log_msg("[Vulkan Validation %s] %s\n", isError ? "ERROR" : "WARN", msg.c_str());
    return VK_FALSE;
}

VlknBackend::VlknBackend()
    : m_instance(VK_NULL_HANDLE)
    , m_physicalDevice(VK_NULL_HANDLE)
    , m_device(VK_NULL_HANDLE)
    , m_queue(VK_NULL_HANDLE)
    , m_queueFamilyIndex(0)
    , m_cmdPool(VK_NULL_HANDLE)
    , m_cmdBuffer(VK_NULL_HANDLE)
    , m_cmdBufferRecording(false)
    , m_renderPassActive(false)
    , m_completedSubmissionSerial(0)
    , m_recordingSerial(0)
    , m_descriptorPool(VK_NULL_HANDLE)
    , m_debugMessenger(VK_NULL_HANDLE)
    , m_validationErrors(0)
    , m_validationWarnings(0)
    , m_stagingBuffer(VK_NULL_HANDLE)
    , m_stagingMemory(VK_NULL_HANDLE)
    , m_stagingMapped(nullptr)
    , m_stagingSize(0)
    , m_stagingBump(0)
    , m_fallbackBuffer(VK_NULL_HANDLE)
    , m_fallbackMemory(VK_NULL_HANDLE)
    , m_fallbackSize(0)
{
    m_measurePerformance = std::getenv("SVGA3_VLKN_PERF_COUNTERS") != nullptr;
    memset(&m_dispatch, 0, sizeof(m_dispatch));
    memset(&m_props, 0, sizeof(m_props));
    memset(&m_features, 0, sizeof(m_features));
    memset(&m_memProps, 0, sizeof(m_memProps));
}

VlknBackend::~VlknBackend() {
    shutdown();
}

void VlknBackend::addValidationMessage(bool isError, const std::string &msg) {
    std::lock_guard<std::mutex> lock(m_validationMutex);
    if (isError) {
        m_validationErrors++;
    } else {
        m_validationWarnings++;
    }
    m_validationMessages.push_back(msg);
}

Svga3VlknStatus VlknBackend::init(const Svga3VlknConfig *config) {
    bool forceMock = config ? config->forceMockBackend : false;
    m_initializationError = "Vulkan loader or required entry points unavailable";
    if (!vlkn_dispatch_init(&m_dispatch, forceMock)) {
        return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }

    m_initializationError = "Vulkan instance or requested validation layer unavailable";
    Svga3VlknStatus st = initInstance(config);
    if (st != SVGA3_VLKN_SUCCESS) {
        /* NO fallback to mock when real Vulkan is requested! */
        return st;
    }

    m_initializationError = "No suitable Vulkan graphics device available";
    st = selectPhysicalDevice(config);
    if (st != SVGA3_VLKN_SUCCESS) return st;

    m_initializationError = "Vulkan device, command pool or descriptor pool creation failed";
    st = initDevice(config);
    if (st != SVGA3_VLKN_SUCCESS) return st;

    size_t stagingSize = (config && config->stagingBufferSize > 0) ? config->stagingBufferSize : (64 * 1024 * 1024);
    m_initializationError = "Staging buffer allocation or mapping failed";
    st = initStagingBuffer(stagingSize);
    if (st != SVGA3_VLKN_SUCCESS) return st;

    m_initializationError = "Fallback buffer allocation or mapping failed";
    st = initFallbackBuffer(1024 * 1024); /* 1MB static fallback vertex/index buffer */
    if (st != SVGA3_VLKN_SUCCESS) return st;

    return SVGA3_VLKN_SUCCESS;
}

void VlknBackend::shutdown() {
    if (waitIdle() != SVGA3_VLKN_SUCCESS) {
        // Completion is unknown: retain allocations until a later reset or process exit.
        log_msg("[libqemu_svga3d] Retaining Vulkan resources after failed shutdown idle\n");
        return;
    }
    cleanupRetiredImages();
    if (m_device && m_measurePerformance) {
        const auto &c = m_performanceCounters;
        log_msg("[libqemu_svga3d] Performance totals: presents=%llu readback_bytes=%llu copy_bytes=%llu copy_calls=%llu submissions=%llu queue_wait_ns=%llu identical_shaders=%llu\n",
                (unsigned long long)c.presentationCalls, (unsigned long long)c.readbackBytes,
                (unsigned long long)c.framebufferCopyBytes, (unsigned long long)c.framebufferCopyCalls,
                (unsigned long long)c.queueSubmissions, (unsigned long long)c.queueWaitNanoseconds,
                (unsigned long long)c.identicalShaderDefinitions);
    }
    cleanupRetiredBuffers(true);
    releaseFramebufferImport();

    for (auto &rp : m_renderPasses) {
        if (rp.renderPass) {
            m_dispatch.vkDestroyRenderPass(m_device, rp.renderPass, nullptr);
        }
    }
    m_renderPasses.clear();

    if (m_descriptorPool && m_device) {
        m_dispatch.vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_stagingMapped && m_stagingMemory) {
        m_dispatch.vkUnmapMemory(m_device, m_stagingMemory);
        m_stagingMapped = nullptr;
    }
    if (m_stagingBuffer) {
        m_dispatch.vkDestroyBuffer(m_device, m_stagingBuffer, nullptr);
        m_stagingBuffer = VK_NULL_HANDLE;
    }
    if (m_stagingMemory) {
        m_dispatch.vkFreeMemory(m_device, m_stagingMemory, nullptr);
        m_stagingMemory = VK_NULL_HANDLE;
    }

    if (m_fallbackBuffer) {
        m_dispatch.vkDestroyBuffer(m_device, m_fallbackBuffer, nullptr);
        m_fallbackBuffer = VK_NULL_HANDLE;
    }
    if (m_fallbackMemory) {
        m_dispatch.vkFreeMemory(m_device, m_fallbackMemory, nullptr);
        m_fallbackMemory = VK_NULL_HANDLE;
    }
    m_fallbackSize = 0;

    for (auto &slot : m_submissions) {
        if (slot.fence) m_dispatch.vkDestroyFence(m_device, slot.fence, nullptr);
        slot.fence = VK_NULL_HANDLE;
    }
    if (m_cmdPool) {
        m_dispatch.vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
        m_cmdPool = VK_NULL_HANDLE;
        m_cmdBuffer = VK_NULL_HANDLE;
        m_cmdBufferRecording = false;
    }

    if (m_device) {
        m_dispatch.vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_debugMessenger && m_instance && m_dispatch.vkDestroyDebugUtilsMessengerEXT) {
        m_dispatch.vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    if (m_instance) {
        m_dispatch.vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    vlkn_dispatch_cleanup(&m_dispatch);
}

void VlknBackend::observeDeviceResult(VkResult result) {
    if (result == VK_ERROR_DEVICE_LOST && !m_deviceLost) {
        m_deviceLost = true;
        m_cmdBufferRecording = false;
        m_renderPassActive = false;
        m_activeQueryPool = VK_NULL_HANDLE;
        m_activeQuerySlot = 0;
        m_activeQueryAccumulated = 0;
        m_activeQueryFailed = false;
        log_msg("[libqemu_svga3d] Vulkan device lost; disabling 3D until device recreation\n");
    }
}

Svga3VlknStatus VlknBackend::waitIdle() {
    if (m_deviceLost) return SVGA3_VLKN_ERROR_DEVICE_LOST;
    if (!m_device) return SVGA3_VLKN_SUCCESS;
    if (!m_dispatch.vkDeviceWaitIdle) return SVGA3_VLKN_ERROR_DEVICE_LOST;
    Svga3VlknStatus status = submitCommandBuffer();
    if (status != SVGA3_VLKN_SUCCESS) return status;
    VkResult result = m_dispatch.vkDeviceWaitIdle(m_device);
    observeDeviceResult(result);
    if (result != VK_SUCCESS) return SVGA3_VLKN_ERROR_DEVICE_LOST;
    for (auto &slot : m_submissions) slot.pending = false;
    m_completedSubmissionSerial = m_lastSubmittedSerial;
    m_stagingBump = 0;
    cleanupRetiredBuffers(false);
    cleanupRetiredImages();
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknBackend::prepareSubmissionSlot() {
    if (m_deviceLost) return SVGA3_VLKN_ERROR_DEVICE_LOST;
    auto &slot = m_submissions[m_currentSlot];
    if (slot.pending) {
        const auto start = std::chrono::steady_clock::now();
        VkResult result = waitForSubmission(slot.fence);
        if (m_measurePerformance)
            m_performanceCounters.queueWaitNanoseconds += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        observeDeviceResult(result);
        if (result != VK_SUCCESS) return SVGA3_VLKN_ERROR_DEVICE_LOST;
        slot.pending = false;
    }
    m_cmdBuffer = slot.commands;
    if (slot.needsReset) {
        VkResult result = m_dispatch.vkResetCommandBuffer(m_cmdBuffer, 0);
        observeDeviceResult(result);
        if (result != VK_SUCCESS) return SVGA3_VLKN_ERROR_DEVICE_LOST;
        slot.needsReset = false;
    }
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknBackend::drainSubmissions() {
    if (m_deviceLost) return SVGA3_VLKN_ERROR_DEVICE_LOST;
    const auto start = std::chrono::steady_clock::now();
    for (auto &slot : m_submissions) {
        if (!slot.pending) continue;
        VkResult result = waitForSubmission(slot.fence);
        observeDeviceResult(result);
        if (result != VK_SUCCESS) return SVGA3_VLKN_ERROR_DEVICE_LOST;
        slot.pending = false;
    }
    if (m_measurePerformance)
        m_performanceCounters.queueWaitNanoseconds += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    m_completedSubmissionSerial = m_lastSubmittedSerial;
    m_stagingBump = 0;
    cleanupRetiredBuffers(false);
    cleanupRetiredImages();
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknBackend::initInstance(const Svga3VlknConfig *config) {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = (config && config->appName) ? config->appName : "SVGA3=VLKN Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "SVGA3=VLKN";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = (config && config->apiVersion) ? config->apiVersion : VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> layers;
    std::vector<const char*> extensions;
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};

    if (config && config->enableValidationLayers && !m_dispatch.isMock) {
        /* Check layer availability */
        uint32_t layerCount = 0;
        if (m_dispatch.vkEnumerateInstanceLayerProperties) {
            m_dispatch.vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            if (layerCount > 0) {
                std::vector<VkLayerProperties> availableLayers(layerCount);
                m_dispatch.vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
                for (const auto &l : availableLayers) {
                    if (strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                        layers.push_back("VK_LAYER_KHRONOS_validation");
                        break;
                    }
                }
            }
        }

        /* Check extension availability for debug utils */
        uint32_t extCount = 0;
        if (m_dispatch.vkEnumerateInstanceExtensionProperties) {
            m_dispatch.vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
            if (extCount > 0) {
                std::vector<VkExtensionProperties> availableExts(extCount);
                m_dispatch.vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExts.data());
                for (const auto &e : availableExts) {
                    if (strcmp(e.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                        break;
                    }
                }
            }
        }

        if (!extensions.empty()) {
            debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugInfo.pfnUserCallback = vlkn_debug_callback;
            debugInfo.pUserData = this;
            createInfo.pNext = &debugInfo;
        }
    }

    if (!layers.empty()) {
        createInfo.enabledLayerCount = (uint32_t)layers.size();
        createInfo.ppEnabledLayerNames = layers.data();
    }
    if (!extensions.empty()) {
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();
    }

    VkResult res = m_dispatch.vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }

    if (!vlkn_dispatch_init_instance(&m_dispatch, m_instance)) {
        log_msg("[libqemu_svga3d] vlkn_dispatch_init_instance failed\n");
        return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }

    if (config && config->enableValidationLayers && !m_dispatch.isMock && !extensions.empty()) {
        PFN_vkCreateDebugUtilsMessengerEXT pfnCreateDebugUtils =
            (PFN_vkCreateDebugUtilsMessengerEXT)m_dispatch.vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (pfnCreateDebugUtils) {
            pfnCreateDebugUtils(m_instance, &debugInfo, nullptr, &m_debugMessenger);
        }
    }

    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknBackend::selectPhysicalDevice(const Svga3VlknConfig *config) {
    uint32_t deviceCount = 0;
    m_dispatch.vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    m_dispatch.vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    /* Score and select best physical device:
     * Discrete GPU > Integrated GPU > Virtual GPU > Other > CPU */
    int bestScore = -1;
    m_physicalDevice = devices[0];
    for (uint32_t i = 0; i < deviceCount; ++i) {
        VkPhysicalDeviceProperties props;
        m_dispatch.vkGetPhysicalDeviceProperties(devices[i], &props);
        int score = 0;
        if (props.deviceType == 2 /* DISCRETE_GPU */) {
            score = (config && config->preferIntegratedGpu) ? 800 : 1000;
        } else if (props.deviceType == 1 /* INTEGRATED_GPU */) {
            score = (config && config->preferIntegratedGpu) ? 1000 : 900;
        } else if (props.deviceType == 3 /* VIRTUAL_GPU */) {
            score = 500;
        } else if (props.deviceType == 0 /* OTHER */) {
            score = 100;
        } else if (props.deviceType == 4 /* CPU */) {
            score = 10;
        }
        if (score > bestScore) {
            bestScore = score;
            m_physicalDevice = devices[i];
        }
    }

    m_dispatch.vkGetPhysicalDeviceProperties(m_physicalDevice, &m_props);
    m_dispatch.vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_features);
    m_dispatch.vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memProps);

    log_msg("[libqemu_svga3d] Selected Vulkan device: %s (type=%d, API=%u.%u.%u, driver=0x%x)\n",
            m_props.deviceName, m_props.deviceType,
            (m_props.apiVersion >> 22) & 0x7F,
            (m_props.apiVersion >> 12) & 0x3FF,
            m_props.apiVersion & 0xFFF,
            m_props.driverVersion);

    /* Find graphics queue family */
    uint32_t queueFamilyCount = 0;
    m_dispatch.vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    m_dispatch.vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    bool foundGraphicsQueue = false;
    m_queueFamilyIndex = 0;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & 0x00000001 /* GRAPHICS_BIT */) {
            m_queueFamilyIndex = i;
            foundGraphicsQueue = true;
            break;
        }
    }
    if (!foundGraphicsQueue) {
        log_msg("[libqemu_svga3d] No graphics queue family found on selected device\n");
        return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }

    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknBackend::initDevice(const Svga3VlknConfig *config) {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures enabledFeatures = {};
    if (!m_features.robustBufferAccess) {
        log_msg("[libqemu_svga3d] robustBufferAccess is required for untrusted guest vertex/index fetches\n");
        return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }
    enabledFeatures.robustBufferAccess = VK_TRUE;
    enabledFeatures.samplerAnisotropy = m_features.samplerAnisotropy;
    enabledFeatures.textureCompressionBC = m_features.textureCompressionBC;
    enabledFeatures.depthClamp = m_features.depthClamp;
    enabledFeatures.depthBiasClamp = m_features.depthBiasClamp;
    enabledFeatures.fillModeNonSolid = m_features.fillModeNonSolid;
    enabledFeatures.wideLines = m_features.wideLines;
    enabledFeatures.largePoints = m_features.largePoints;
    enabledFeatures.independentBlend = m_features.independentBlend;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

    VkPhysicalDeviceCustomBorderColorFeaturesEXT borderFeatures = {};
    borderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
    const char *borderExtension = VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME;
    const uint32_t requestedVersion = config && config->apiVersion ? config->apiVersion : VK_API_VERSION_1_1;
    if (requestedVersion >= VK_API_VERSION_1_1 && !m_dispatch.isMock && m_dispatch.vkGetInstanceProcAddr) {
        auto enumerate = (PFN_vkEnumerateDeviceExtensionProperties)m_dispatch.vkGetInstanceProcAddr(m_instance, "vkEnumerateDeviceExtensionProperties");
        auto getFeatures = (PFN_vkGetPhysicalDeviceFeatures2)m_dispatch.vkGetInstanceProcAddr(m_instance, "vkGetPhysicalDeviceFeatures2");
        uint32_t count = 0;
        if (enumerate && enumerate(m_physicalDevice, nullptr, &count, nullptr) == VK_SUCCESS) {
            std::vector<VkExtensionProperties> extensions(count);
            if (enumerate(m_physicalDevice, nullptr, &count, extensions.data()) == VK_SUCCESS) {
                for (const auto &extension : extensions) {
                    const uint32_t requestedVersion = (config && config->apiVersion) ? config->apiVersion : VK_API_VERSION_1_1;
                    if (getFeatures && strcmp(extension.extensionName, borderExtension) == 0) {
                        VkPhysicalDeviceFeatures2 features = {};
                        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                        features.pNext = &borderFeatures;
                        getFeatures(m_physicalDevice, &features);
                        m_customBorderColors = borderFeatures.customBorderColors && borderFeatures.customBorderColorWithoutFormat;
                    }
                    // maintenance2, required by image_format_list, is core in 1.1.
                    if (requestedVersion >= VK_API_VERSION_1_1 && m_props.apiVersion >= VK_API_VERSION_1_1 &&
                        strcmp(extension.extensionName, VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME) == 0)
                        m_imageFormatList = true;
                    if (requestedVersion >= VK_API_VERSION_1_1 && m_props.apiVersion >= VK_API_VERSION_1_1 &&
                        strcmp(extension.extensionName, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME) == 0) {
                        auto properties = (PFN_vkGetPhysicalDeviceProperties2)m_dispatch.vkGetInstanceProcAddr(m_instance, "vkGetPhysicalDeviceProperties2");
                        auto external = (PFN_vkGetPhysicalDeviceExternalBufferProperties)m_dispatch.vkGetInstanceProcAddr(m_instance, "vkGetPhysicalDeviceExternalBufferProperties");
                        if (properties && external) {
                            VkPhysicalDeviceExternalMemoryHostPropertiesEXT host{};
                            host.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
                            VkPhysicalDeviceProperties2 props{};
                            props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                            props.pNext = &host;
                            properties(m_physicalDevice, &props);
                            VkPhysicalDeviceExternalBufferInfo info{};
                            info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO;
                            info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                            info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
                            VkExternalBufferProperties result{};
                            result.sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES;
                            external(m_physicalDevice, &info, &result);
                            const auto flags = result.externalMemoryProperties.externalMemoryFeatures;
                            if ((flags & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) &&
                                !(flags & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT))
                                m_hostImportAlignment = host.minImportedHostPointerAlignment;
                        }
                    }
                }
            }
        }
    }
    std::vector<const char *> deviceExtensions;
    if (m_imageFormatList) deviceExtensions.push_back(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
    if (m_hostImportAlignment) deviceExtensions.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
    if (m_customBorderColors) {
        deviceCreateInfo.pNext = &borderFeatures;
        deviceExtensions.push_back(borderExtension);
    }
    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkResult res = m_dispatch.vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }

    if (!vlkn_dispatch_init_device(&m_dispatch, m_instance, m_device)) {
        log_msg("[libqemu_svga3d] vlkn_dispatch_init_device failed: missing device entry points\n");
        return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }

    m_dispatch.vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
    if (m_hostImportAlignment)
        m_getHostPointerProperties = (PFN_vkGetMemoryHostPointerPropertiesEXT)m_dispatch.vkGetDeviceProcAddr(m_device, "vkGetMemoryHostPointerPropertiesEXT");

    /* Create Command Pool */
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_queueFamilyIndex;
    poolInfo.flags = 0x00000002; /* VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT */

    res = m_dispatch.vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_cmdPool);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_cmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    for (auto &slot : m_submissions) {
        res = m_dispatch.vkAllocateCommandBuffers(m_device, &allocInfo, &slot.commands);
        if (res != VK_SUCCESS) return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
        res = m_dispatch.vkCreateFence(m_device, &fenceInfo, nullptr, &slot.fence);
        if (res != VK_SUCCESS) return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }
    m_cmdBuffer = m_submissions[0].commands;

    /* Create Descriptor Pool for shader uniform buffers and texture samplers */
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4096 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 8192 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 * 16 }
    };
    VkDescriptorPoolCreateInfo descPoolInfo = {};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.maxSets = 4096;
    descPoolInfo.poolSizeCount = 3;
    descPoolInfo.pPoolSizes = poolSizes;
    descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    res = m_dispatch.vkCreateDescriptorPool(m_device, &descPoolInfo, nullptr, &m_descriptorPool);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        return SVGA3_VLKN_ERROR_VULKAN_INIT_FAILED;
    }

    return SVGA3_VLKN_SUCCESS;
}

int VlknBackend::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties,
                                VkMemoryPropertyFlags preferredProperties) {
    if (preferredProperties) {
        const int preferred = findMemoryType(typeFilter, properties | preferredProperties);
        if (preferred >= 0) return preferred;
    }
    for (uint32_t i = 0; i < m_memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (m_memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return (int)i;
        }
    }
    /* No fallback: returning a type without the requested properties (e.g.
     * device-local memory for a HOST_VISIBLE|HOST_COHERENT request) would
     * silently produce unmappable or incoherent allocations. Callers must
     * treat -1 as "no suitable memory type" and fail the allocation. */
    return -1;
}

Svga3VlknStatus VlknBackend::allocateMemory(VkDeviceSize size, uint32_t memoryTypeIndex, VkDeviceMemory *outMemory) {
    /* A memory type index is only meaningful below memoryTypeCount. The
     * bug in issue #10 passed findMemoryType()'s -1 failure return here,
     * where it wrapped to 0xFFFFFFFF. Individual call sites guard their
     * findMemoryType() results, and this central check makes the whole
     * class of failure impossible regardless of caller. */
    if (memoryTypeIndex >= m_memProps.memoryTypeCount) {
        log_msg("[libqemu_svga3d] allocateMemory: invalid memory type index %u (type count %u)\n",
                memoryTypeIndex, m_memProps.memoryTypeCount);
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    VkResult res = m_dispatch.vkAllocateMemory(m_device, &allocInfo, nullptr, outMemory);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }
    return SVGA3_VLKN_SUCCESS;
}

void VlknBackend::freeMemory(VkDeviceMemory memory) {
    if (m_deviceLost) return;
    if (memory) {
        m_dispatch.vkFreeMemory(m_device, memory, nullptr);
    }
}

Svga3VlknStatus VlknBackend::createBuffer(VkDeviceSize size,
                                         VkBufferUsageFlags usage,
                                         VkMemoryPropertyFlags properties,
                                         VkBuffer *outBuffer,
                                         VkDeviceMemory *outMemory,
                                         VkMemoryPropertyFlags preferredProperties)
{
    // Queued vertex/index updates snapshot old mapped bytes. Cached coherent
    // memory keeps those CPU reads cheap while preserving the required flags.
    if (!preferredProperties && (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        (usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT)))
        preferredProperties = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = m_dispatch.vkCreateBuffer(m_device, &bufferInfo, nullptr, outBuffer);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }

    VkMemoryRequirements memReqs;
    m_dispatch.vkGetBufferMemoryRequirements(m_device, *outBuffer, &memReqs);

    int memType = findMemoryType(memReqs.memoryTypeBits, properties, preferredProperties);
    if (memType < 0) {
        log_msg("[libqemu_svga3d] createBuffer: no memory type satisfies the requested properties\n");
        m_dispatch.vkDestroyBuffer(m_device, *outBuffer, nullptr);
        *outBuffer = VK_NULL_HANDLE;
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }
    if (preferredProperties)
        log_msg("[libqemu_svga3d] Transfer memory type=%d properties=0x%x preferred=0x%x\n",
                memType, m_memProps.memoryTypes[memType].propertyFlags, preferredProperties);
    Svga3VlknStatus st = allocateMemory(memReqs.size, (uint32_t)memType, outMemory);
    if (st != SVGA3_VLKN_SUCCESS) {
        m_dispatch.vkDestroyBuffer(m_device, *outBuffer, nullptr);
        *outBuffer = VK_NULL_HANDLE;
        return st;
    }

    VkResult bindRes = m_dispatch.vkBindBufferMemory(m_device, *outBuffer, *outMemory, 0);
    if (bindRes != VK_SUCCESS) {
        log_msg("[libqemu_svga3d] createBuffer: vkBindBufferMemory failed (%d)\n", bindRes);
        m_dispatch.vkDestroyBuffer(m_device, *outBuffer, nullptr);
        *outBuffer = VK_NULL_HANDLE;
        m_dispatch.vkFreeMemory(m_device, *outMemory, nullptr);
        *outMemory = VK_NULL_HANDLE;
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }
    return SVGA3_VLKN_SUCCESS;
}

void VlknBackend::destroyBuffer(VkBuffer buffer, VkDeviceMemory memory) {
    if (m_deviceLost) { retireBuffer(buffer, memory, nullptr, m_recordingSerial, 0); return; }
    if (buffer) m_dispatch.vkDestroyBuffer(m_device, buffer, nullptr);
    if (memory) m_dispatch.vkFreeMemory(m_device, memory, nullptr);
}

Svga3VlknStatus VlknBackend::initStagingBuffer(size_t size) {
    m_stagingSize = size;
    // This shared buffer also supplies CPU readbacks for every presented
    // frame. Prefer cached memory, retaining coherent memory as a requirement
    // and the existing allocation choice when no cached coherent type fits.
    Svga3VlknStatus st = createBuffer(size,
                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      &m_stagingBuffer,
                                      &m_stagingMemory,
                                      VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    if (st != SVGA3_VLKN_SUCCESS) return st;

    VkResult res = m_dispatch.vkMapMemory(m_device, m_stagingMemory, 0, size, 0, &m_stagingMapped);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknBackend::initFallbackBuffer(size_t size) {
    m_fallbackSize = size;
    Svga3VlknStatus st = createBuffer(size,
                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      &m_fallbackBuffer,
                                      &m_fallbackMemory);
    if (st != SVGA3_VLKN_SUCCESS) return st;

    void *mapped = nullptr;
    VkResult res = m_dispatch.vkMapMemory(m_device, m_fallbackMemory, 0, size, 0, &mapped);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }

    /* Fill buffer with 1.0f float values (0x3F800000) so any attribute read produces 1.0f */
    uint32_t *words = static_cast<uint32_t*>(mapped);
    size_t numWords = size / sizeof(uint32_t);
    for (size_t i = 0; i < numWords; ++i) {
        words[i] = 0x3F800000;
    }
    m_dispatch.vkUnmapMemory(m_device, m_fallbackMemory);
    return SVGA3_VLKN_SUCCESS;
}

VkDeviceSize VlknBackend::stagingAlloc(VkDeviceSize size) {
    /* Caller holds m_stagingMutex. */
    if (!m_stagingBuffer || !m_stagingMapped || size > m_stagingSize) {
        return kStagingAllocFailed;
    }
    /* optimalBufferCopyOffsetAlignment is a power of two; 16 covers the
     * largest texel block, so the offset is valid for buffer and image
     * copies alike. */
    VkDeviceSize align = m_props.limits.optimalBufferCopyOffsetAlignment;
    if (align < 16) align = 16;
    VkDeviceSize offset = (m_stagingBump + align - 1) & ~(align - 1);
    if (offset > m_stagingSize || m_stagingSize - offset < size) {
        return kStagingAllocFailed;
    }
    m_stagingBump = offset + size;
    return offset;
}

Svga3VlknStatus VlknBackend::uploadToBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, const void *srcData, VkDeviceSize size) {
    std::lock_guard<std::mutex> lock(m_stagingMutex);
    if (size > m_stagingSize) {
        /* Allocate a temporary staging buffer for large transfers */
        VkBuffer tempBuffer;
        VkDeviceMemory tempMemory;
        Svga3VlknStatus st = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          &tempBuffer, &tempMemory);
        if (st != SVGA3_VLKN_SUCCESS) return st;

        void *mapped = nullptr;
        VkResult mapRes = m_dispatch.vkMapMemory(m_device, tempMemory, 0, size, 0, &mapped);
        if (mapRes != VK_SUCCESS || !mapped) {
            log_msg("[libqemu_svga3d] uploadToBuffer: vkMapMemory failed (%d)\n", mapRes);
            destroyBuffer(tempBuffer, tempMemory);
            return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
        }
        memcpy(mapped, srcData, (size_t)size);
        m_dispatch.vkUnmapMemory(m_device, tempMemory);

        VkCommandBuffer cb = getActiveCommandBuffer();
        if (!cb) {
            destroyBuffer(tempBuffer, tempMemory);
            return SVGA3_VLKN_ERROR_DEVICE_LOST;
        }
        VkBufferCopy region = {};
        region.srcOffset = 0;
        region.dstOffset = dstOffset;
        region.size = size;
        VkBufferMemoryBarrier dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dependency.srcQueueFamilyIndex = dependency.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        dependency.buffer = dstBuffer;
        dependency.offset = dstOffset;
        dependency.size = size;
        m_dispatch.vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 1, &dependency, 0, nullptr);
        m_dispatch.vkCmdCopyBuffer(cb, tempBuffer, dstBuffer, 1, &region);
        dependency.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        m_dispatch.vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0, 0, nullptr, 1, &dependency, 0, nullptr);
        const uint64_t serial = recordingSerial();
        Svga3VlknStatus flushSt = submitCommandBuffer();
        retireBuffer(tempBuffer, tempMemory, nullptr, serial, 0);
        return flushSt;
    }

    /* Bump-allocate so a pending unflushed range is never clobbered. */
    VkDeviceSize stagingOffset = stagingAlloc(size);
    if (stagingOffset == kStagingAllocFailed) {
        Svga3VlknStatus fst = flushCommandBuffer();
        if (fst != SVGA3_VLKN_SUCCESS) return fst;
        stagingOffset = stagingAlloc(size);
        if (stagingOffset == kStagingAllocFailed) return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }
    memcpy(stagingMappedAt(stagingOffset), srcData, (size_t)size);

    VkCommandBuffer cb = getActiveCommandBuffer();
    if (!cb) return SVGA3_VLKN_ERROR_DEVICE_LOST;
    VkBufferCopy region = {};
    region.srcOffset = stagingOffset;
    region.dstOffset = dstOffset;
    region.size = size;
    VkBufferMemoryBarrier dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dependency.srcQueueFamilyIndex = dependency.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dependency.buffer = dstBuffer;
    dependency.offset = dstOffset;
    dependency.size = size;
    m_dispatch.vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 1, &dependency, 0, nullptr);
    m_dispatch.vkCmdCopyBuffer(cb, m_stagingBuffer, dstBuffer, 1, &region);
    dependency.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    m_dispatch.vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 1, &dependency, 0, nullptr);
    return submitCommandBuffer();
}

Svga3VlknStatus VlknBackend::downloadFromBuffer(void *dstData, VkBuffer srcBuffer, VkDeviceSize srcOffset, VkDeviceSize size) {
    std::lock_guard<std::mutex> lock(m_stagingMutex);
    if (size > m_stagingSize) {
        /* Allocate a temporary staging buffer for large transfers (mirrors uploadToBuffer) */
        VkBuffer tempBuffer;
        VkDeviceMemory tempMemory;
        Svga3VlknStatus st = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          &tempBuffer, &tempMemory);
        if (st != SVGA3_VLKN_SUCCESS) return st;

        VkCommandBuffer cb = getActiveCommandBuffer();
        if (!cb) {
            destroyBuffer(tempBuffer, tempMemory);
            return SVGA3_VLKN_ERROR_DEVICE_LOST;
        }
        VkBufferCopy region = {};
        region.srcOffset = srcOffset;
        region.dstOffset = 0;
        region.size = size;
        m_dispatch.vkCmdCopyBuffer(cb, srcBuffer, tempBuffer, 1, &region);
        recordHostReadBarrier(cb, tempBuffer, 0, size);
        const uint64_t serial = recordingSerial();
        st = flushCommandBuffer();
        if (st != SVGA3_VLKN_SUCCESS) {
            retireBuffer(tempBuffer, tempMemory, nullptr, serial, 0);
            return st;
        }

        void *mapped = nullptr;
        VkResult mapRes = m_dispatch.vkMapMemory(m_device, tempMemory, 0, size, 0, &mapped);
        if (mapRes != VK_SUCCESS || !mapped) {
            log_msg("[libqemu_svga3d] downloadFromBuffer: vkMapMemory failed (%d)\n", mapRes);
            destroyBuffer(tempBuffer, tempMemory);
            return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
        }
        memcpy(dstData, mapped, (size_t)size);
        m_dispatch.vkUnmapMemory(m_device, tempMemory);

        destroyBuffer(tempBuffer, tempMemory);
        return SVGA3_VLKN_SUCCESS;
    }

    /* Bump-allocate so a pending unflushed range is never clobbered. This
     * is a reader: the flush below also drains any pending uploads first. */
    VkDeviceSize stagingOffset = stagingAlloc(size);
    if (stagingOffset == kStagingAllocFailed) {
        Svga3VlknStatus fst = flushCommandBuffer();
        if (fst != SVGA3_VLKN_SUCCESS) return fst;
        stagingOffset = stagingAlloc(size);
        if (stagingOffset == kStagingAllocFailed) return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }

    VkCommandBuffer cb = getActiveCommandBuffer();
    if (!cb) return SVGA3_VLKN_ERROR_DEVICE_LOST;
    VkBufferCopy region = {};
    region.srcOffset = srcOffset;
    region.dstOffset = stagingOffset;
    region.size = size;

    m_dispatch.vkCmdCopyBuffer(cb, srcBuffer, m_stagingBuffer, 1, &region);
    recordHostReadBarrier(cb, m_stagingBuffer, stagingOffset, size);
    Svga3VlknStatus st = flushCommandBuffer();
    if (st != SVGA3_VLKN_SUCCESS) return st;

    memcpy(dstData, stagingMappedAt(stagingOffset), (size_t)size);
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknBackend::enableFramebufferImport(bool enabled) {
    if (!enabled) {
        auto status = releaseFramebufferImport();
        if (status != SVGA3_VLKN_SUCCESS) return status;
    }
    m_allowFramebufferImport = enabled;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknBackend::releaseFramebufferImport() {
    if (!m_framebufferBuffer) {
        m_framebufferPointer = nullptr;
        m_framebufferSize = 0;
        return SVGA3_VLKN_SUCCESS;
    }
    auto status = flushCommandBuffer();
    if (status != SVGA3_VLKN_SUCCESS) return status;
    destroyBuffer(m_framebufferBuffer, m_framebufferMemory);
    m_framebufferBuffer = VK_NULL_HANDLE;
    m_framebufferMemory = VK_NULL_HANDLE;
    m_framebufferPointer = nullptr;
    m_framebufferSize = 0;
    return SVGA3_VLKN_SUCCESS;
}

VkBuffer VlknBackend::importFramebuffer(void *pointer, size_t size, size_t requiredSize) {
    if (!m_allowFramebufferImport || m_deviceLost || !m_getHostPointerProperties || !m_hostImportAlignment || !pointer ||
        uintptr_t(pointer) % m_hostImportAlignment || !size)
        return VK_NULL_HANDLE;
    if (requiredSize) {
        if (requiredSize > size || requiredSize > SIZE_MAX - (m_hostImportAlignment - 1)) return VK_NULL_HANDLE;
        const size_t rounded = ((requiredSize + m_hostImportAlignment - 1) / m_hostImportAlignment) * m_hostImportAlignment;
        if (rounded > size) return VK_NULL_HANDLE;
        size = rounded;
    }
    if (size % m_hostImportAlignment) return VK_NULL_HANDLE;
    if (pointer == m_framebufferPointer && size == m_framebufferSize) return m_framebufferBuffer;
    if (releaseFramebufferImport() != SVGA3_VLKN_SUCCESS) return VK_NULL_HANDLE;
    // Remember failed imports too; an unavailable platform mapping must not
    // cause allocation attempts on every frame. Re-registration clears it.
    m_framebufferPointer = pointer;
    m_framebufferSize = size;

    VkMemoryHostPointerPropertiesEXT host{};
    host.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
    VkResult result = m_getHostPointerProperties(m_device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
                                                pointer, &host);
    observeDeviceResult(result);
    if (result != VK_SUCCESS) return VK_NULL_HANDLE;
    VkExternalMemoryBufferCreateInfo external{};
    external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = &external;
    info.size = size;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = m_dispatch.vkCreateBuffer(m_device, &info, nullptr, &buffer);
    observeDeviceResult(result);
    if (result != VK_SUCCESS) return VK_NULL_HANDLE;
    VkMemoryRequirements requirements{};
    m_dispatch.vkGetBufferMemoryRequirements(m_device, buffer, &requirements);
    int type = findMemoryType(requirements.memoryTypeBits & host.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    if (type < 0 || requirements.size > size) {
        destroyBuffer(buffer, memory);
        return VK_NULL_HANDLE;
    }
    VkImportMemoryHostPointerInfoEXT import{};
    import.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
    import.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    import.pHostPointer = pointer;
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.pNext = &import;
    allocation.allocationSize = size;
    allocation.memoryTypeIndex = type;
    result = m_dispatch.vkAllocateMemory(m_device, &allocation, nullptr, &memory);
    if (result == VK_SUCCESS) result = m_dispatch.vkBindBufferMemory(m_device, buffer, memory, 0);
    observeDeviceResult(result);
    if (result != VK_SUCCESS) {
        destroyBuffer(buffer, memory);
        return VK_NULL_HANDLE;
    }
    m_framebufferBuffer = buffer;
    m_framebufferMemory = memory;
    log_msg("[libqemu_svga3d] Imported coherent framebuffer: %p size=%zu memory_type=%d\n", pointer, size, type);
    return buffer;
}

void VlknBackend::recordHostReadBarrier(VkCommandBuffer commands, VkBuffer buffer,
                                       VkDeviceSize offset, VkDeviceSize size) {
    // Queue completion supplies execution ordering; this dependency makes
    // transfer writes available and visible to the subsequent CPU reads.
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;
    m_dispatch.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

VkCommandBuffer VlknBackend::getActiveCommandBuffer() {
    if (m_deviceLost) return VK_NULL_HANDLE;
    if (!m_cmdBufferRecording) {
        if (m_activeQueryPool != VK_NULL_HANDLE && !m_activeQueryFailed && m_activeQuerySlot == 15) {
            for (auto &slot : m_submissions) {
                if (!slot.pending) continue;
                VkResult wRes = waitForSubmission(slot.fence);
                observeDeviceResult(wRes);
                if (wRes != VK_SUCCESS) { m_activeQueryFailed = true; return VK_NULL_HANDLE; }
                slot.pending = false;
            }
            m_completedSubmissionSerial = m_lastSubmittedSerial;
            cleanupRetiredBuffers(false);
            cleanupRetiredImages();
            uint64_t slotResults[16] = {};
            VkResult qRes = m_dispatch.vkGetQueryPoolResults(
                m_device, m_activeQueryPool, 0, 16, sizeof(slotResults), slotResults,
                sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            observeDeviceResult(qRes);
            if (qRes != VK_SUCCESS) { m_activeQueryFailed = true; return VK_NULL_HANDLE; }
            for (uint64_t v : slotResults) m_activeQueryAccumulated += v;
            m_activeQuerySlot = UINT32_MAX;
        }
        if (prepareSubmissionSlot() != SVGA3_VLKN_SUCCESS) return VK_NULL_HANDLE;
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkResult result = m_dispatch.vkBeginCommandBuffer(m_cmdBuffer, &beginInfo);
        observeDeviceResult(result);
        if (result != VK_SUCCESS) return VK_NULL_HANDLE;
        m_cmdBufferRecording = true;
        m_boundGraphicsPipeline = VK_NULL_HANDLE;
        m_graphicsDrawStateValid = false;
        ++m_recordingSerial;
        if (m_activeQueryPool != VK_NULL_HANDLE && !m_activeQueryFailed) {
            m_activeQuerySlot = (m_activeQuerySlot == UINT32_MAX) ? 0 : (m_activeQuerySlot + 1);
            m_dispatch.vkCmdResetQueryPool(m_cmdBuffer, m_activeQueryPool, m_activeQuerySlot, 1);
            m_dispatch.vkCmdBeginQuery(m_cmdBuffer, m_activeQueryPool, m_activeQuerySlot, 0);
        }
    }
    return m_cmdBuffer;
}

void VlknBackend::bindGraphicsPipeline(VkCommandBuffer commands, VkPipeline pipeline) {
    // Binding state belongs to the shared command buffer, not a context.
    // A new recording invalidates it; interleaved contexts update it here.
    if (pipeline != m_boundGraphicsPipeline) {
        m_dispatch.vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        m_boundGraphicsPipeline = pipeline;
    }
}

void VlknBackend::setGraphicsDrawState(VkCommandBuffer commands, const GraphicsDrawState &state) {
    // All renderer pipelines declare these states dynamic. Values persist
    // across compatible pipeline binds and render passes, but not recordings.
    const auto &old = m_graphicsDrawState;
    const bool first = !m_graphicsDrawStateValid;
    if (first || memcmp(&state.viewport, &old.viewport, sizeof(state.viewport)))
        m_dispatch.vkCmdSetViewport(commands, 0, 1, &state.viewport);
    if (first || memcmp(&state.scissor, &old.scissor, sizeof(state.scissor)))
        m_dispatch.vkCmdSetScissor(commands, 0, 1, &state.scissor);
    if (first || state.blendColor != old.blendColor) {
        const uint32_t color = state.blendColor;
        const float values[4] = {float((color >> 16) & 255) / 255,
            float((color >> 8) & 255) / 255, float(color & 255) / 255, float(color >> 24) / 255};
        m_dispatch.vkCmdSetBlendConstants(commands, values);
    }
    if (first || memcmp(&state.constantBias, &old.constantBias, sizeof(float)) ||
        memcmp(&state.slopeBias, &old.slopeBias, sizeof(float)))
        m_dispatch.vkCmdSetDepthBias(commands, state.constantBias, 0, state.slopeBias);
    const PFN_vkCmdSetStencilReference setters[] = {m_dispatch.vkCmdSetStencilReference,
        m_dispatch.vkCmdSetStencilCompareMask, m_dispatch.vkCmdSetStencilWriteMask};
    for (size_t i = 0; i < state.stencil.size(); ++i) {
        const auto &faces = state.stencil[i];
        const bool front = first || faces[0] != old.stencil[i][0];
        const bool back = first || faces[1] != old.stencil[i][1];
        if ((front || back) && faces[0] == faces[1])
            setters[i](commands, VK_STENCIL_FACE_FRONT_AND_BACK, faces[0]);
        else {
            if (front) setters[i](commands, VK_STENCIL_FACE_FRONT_BIT, faces[0]);
            if (back) setters[i](commands, VK_STENCIL_FACE_BACK_BIT, faces[1]);
        }
    }
    m_graphicsDrawState = state;
    m_graphicsDrawStateValid = true;
}

void VlknBackend::retireBuffer(VkBuffer buffer, VkDeviceMemory memory, void *mapped, uint64_t serial, size_t budgetBytes) {
    if (buffer == VK_NULL_HANDLE && memory == VK_NULL_HANDLE) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_retiredBuffers.push_back({buffer, memory, mapped, serial, budgetBytes});
}

void VlknBackend::retireImage(VkImage image, VkDeviceMemory memory, std::vector<VkImageView> views) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_retiredImages.push_back({image,memory,std::move(views),m_recordingSerial});
}

void VlknBackend::cleanupRetiredImages() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it=m_retiredImages.begin();it!=m_retiredImages.end();) {
        if (it->serial > m_completedSubmissionSerial) { ++it; continue; }
        for (auto view:it->views) if (view) m_dispatch.vkDestroyImageView(m_device,view,nullptr);
        if (it->image) m_dispatch.vkDestroyImage(m_device,it->image,nullptr);
        if (it->memory) m_dispatch.vkFreeMemory(m_device,it->memory,nullptr);
        it=m_retiredImages.erase(it);
    }
}

void VlknBackend::cleanupRetiredBuffers(bool forceAll) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_retiredBuffers.begin(); it != m_retiredBuffers.end(); ) {
        if (forceAll || it->serial <= m_completedSubmissionSerial) {
            if (it->mapped && it->memory && m_dispatch.vkUnmapMemory) {
                m_dispatch.vkUnmapMemory(m_device, it->memory);
            }
            if (it->buffer && m_dispatch.vkDestroyBuffer) {
                m_dispatch.vkDestroyBuffer(m_device, it->buffer, nullptr);
            }
            if (it->memory && m_dispatch.vkFreeMemory) {
                m_dispatch.vkFreeMemory(m_device, it->memory, nullptr);
            }
            m_budgets.releaseSurfaceBytes(it->budgetBytes);
            it = m_retiredBuffers.erase(it);
        } else {
            ++it;
        }
    }
}

void VlknBackend::cmdBeginRenderPass(VkCommandBuffer cb, const VkRenderPassBeginInfo *pBegin, VkSubpassContents contents) {
    if (m_renderPassActive) {
        log_msg("[libqemu_svga3d] WARNING: cmdBeginRenderPass called while render pass active; ending previous\n");
        m_dispatch.vkCmdEndRenderPass(cb);
        m_renderPassActive = false;
    }
    m_dispatch.vkCmdBeginRenderPass(cb, pBegin, contents);
    m_renderPassActive = true;
}

void VlknBackend::cmdEndRenderPass(VkCommandBuffer cb) {
    if (!m_renderPassActive) {
        log_msg("[libqemu_svga3d] WARNING: cmdEndRenderPass called while NO render pass active; skipping to prevent crash\n");
        return;
    }
    if (!m_cmdBufferRecording) {
        log_msg("[libqemu_svga3d] WARNING: cmdEndRenderPass called while command buffer not recording; skipping\n");
        m_renderPassActive = false;
        return;
    }
    m_dispatch.vkCmdEndRenderPass(cb);
    m_renderPassActive = false;
}

bool VlknBackend::enableSubmissionFenceWait(uint32_t spinMicroseconds) {
    if (!m_device || m_deviceLost || m_dispatch.isMock || !m_dispatch.vkGetDeviceProcAddr) return false;
    auto create = (PFN_vkCreateFence)m_dispatch.vkGetDeviceProcAddr(m_device, "vkCreateFence");
    auto destroy = (PFN_vkDestroyFence)m_dispatch.vkGetDeviceProcAddr(m_device, "vkDestroyFence");
    auto reset = (PFN_vkResetFences)m_dispatch.vkGetDeviceProcAddr(m_device, "vkResetFences");
    auto status = (PFN_vkGetFenceStatus)m_dispatch.vkGetDeviceProcAddr(m_device, "vkGetFenceStatus");
    auto wait = (PFN_vkWaitForFences)m_dispatch.vkGetDeviceProcAddr(m_device, "vkWaitForFences");
    if (!create || !destroy || !reset || !status || !wait) return false;
    // Each slot already owns a fence. Preserve the old mode if resolution fails.
    m_resetSubmissionFence = reset;
    m_submissionFenceStatus = status;
    m_waitSubmissionFence = wait;
    m_fenceSpinMicroseconds = std::min(spinMicroseconds, 2000u);
    return true;
}

VkResult VlknBackend::waitForSubmission(VkFence fence) {
    if (!m_waitSubmissionFence)
        return m_dispatch.vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
    VkResult result = VK_NOT_READY;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(m_fenceSpinMicroseconds);
    while (m_fenceSpinMicroseconds && result == VK_NOT_READY && std::chrono::steady_clock::now() < deadline)
        result = m_submissionFenceStatus(m_device, fence);
    return result == VK_NOT_READY ? m_waitSubmissionFence(m_device, 1, &fence, VK_TRUE, UINT64_MAX) : result;
}

Svga3VlknStatus VlknBackend::flushCommandBuffer() {
    Svga3VlknStatus status = submitCommandBuffer();
    if (status != SVGA3_VLKN_SUCCESS) return status;
    return drainSubmissions();
}

Svga3VlknStatus VlknBackend::submitCommandBuffer() {
    if (m_deviceLost) return SVGA3_VLKN_ERROR_DEVICE_LOST;
    if (!m_cmdBufferRecording) return SVGA3_VLKN_SUCCESS;

    /* Hook runs without the flush holding any backend lock, but transfer
     * callers may hold m_stagingMutex (non-recursive) across this whole
     * function — see the threading contract in vlkn_backend.h. The hook
     * must not call transfer APIs. */
    if (m_preFlushHook) {
        auto hook = m_preFlushHook;
        m_preFlushHook = nullptr;
        hook();
        m_preFlushHook = hook;
        if (!m_cmdBufferRecording) return SVGA3_VLKN_SUCCESS;
    }

    auto &slot = m_submissions[m_currentSlot];
    auto resetFence = m_resetSubmissionFence ? m_resetSubmissionFence : m_dispatch.vkResetFences;
    VkResult fenceResult = resetFence(m_device, 1, &slot.fence);
    observeDeviceResult(fenceResult);
    if (fenceResult != VK_SUCCESS) return SVGA3_VLKN_ERROR_DEVICE_LOST;

    if (m_renderPassActive) {
        m_dispatch.vkCmdEndRenderPass(m_cmdBuffer);
        m_renderPassActive = false;
    }
    if (m_activeQueryPool != VK_NULL_HANDLE && !m_activeQueryFailed) {
        m_dispatch.vkCmdEndQuery(m_cmdBuffer, m_activeQueryPool, m_activeQuerySlot);
    }

    VkResult res = m_dispatch.vkEndCommandBuffer(m_cmdBuffer);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        log_msg("[libqemu_svga3d] vkEndCommandBuffer error: %d\n", res);
        m_cmdBufferRecording = false;
        m_renderPassActive = false;
        if (m_activeQueryPool != VK_NULL_HANDLE) m_activeQueryFailed = true;
        VkResult resetRes = m_deviceLost ? VK_ERROR_DEVICE_LOST : m_dispatch.vkResetCommandBuffer(m_cmdBuffer, 0);
        observeDeviceResult(resetRes);
        if (resetRes != VK_SUCCESS)
            log_msg("[libqemu_svga3d] vkResetCommandBuffer error after end failure: %d\n", resetRes);
        return SVGA3_VLKN_ERROR_DEVICE_LOST;
    }
    m_cmdBufferRecording = false;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_cmdBuffer;

    res = m_dispatch.vkQueueSubmit(m_queue, 1, &submitInfo, slot.fence);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        log_msg("[libqemu_svga3d] vkQueueSubmit error: %d\n", res);
        if (m_activeQueryPool != VK_NULL_HANDLE) m_activeQueryFailed = true;
        VkResult resetRes = m_deviceLost ? VK_ERROR_DEVICE_LOST : m_dispatch.vkResetCommandBuffer(m_cmdBuffer, 0);
        observeDeviceResult(resetRes);
        if (resetRes != VK_SUCCESS)
            log_msg("[libqemu_svga3d] vkResetCommandBuffer error after submit failure: %d\n", resetRes);
        return SVGA3_VLKN_ERROR_DEVICE_LOST;
    }
    slot.pending = true;
    slot.needsReset = true;
    m_lastSubmittedSerial = m_recordingSerial;
    if (m_measurePerformance) ++m_performanceCounters.queueSubmissions;
    m_currentSlot = (m_currentSlot + 1) % kSubmissionSlots;
    return SVGA3_VLKN_SUCCESS;
}

VkRenderPass VlknBackend::getOrCreateRenderPass(VkFormat colorFormat, VkFormat depthFormat) {
    return getOrCreateRenderPass({colorFormat, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED}, depthFormat);
}

VkRenderPass VlknBackend::getOrCreateRenderPass(const std::array<VkFormat, 4> &colorFormats, VkFormat depthFormat,
                                              uint32_t clearFlags) {
    clearFlags &= SVGA3D_CLEAR_COLOR | SVGA3D_CLEAR_DEPTH | SVGA3D_CLEAR_STENCIL;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &entry : m_renderPasses) {
        if (entry.colorFormats == colorFormats && entry.depthFormat == depthFormat && entry.clearFlags == clearFlags) {
            return entry.renderPass;
        }
    }

    VkAttachmentDescription attachments[5] = {};
    uint32_t attachmentCount = 0;

    VkAttachmentReference colorRefs[4] = {};
    uint32_t colorCount = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        colorRefs[i].attachment = VK_ATTACHMENT_UNUSED;
        colorRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        if (colorFormats[i] == VK_FORMAT_UNDEFINED) continue;
        auto &a = attachments[attachmentCount];
        a.format = colorFormats[i];
        a.samples = VK_SAMPLE_COUNT_1_BIT;
        a.loadOp = (clearFlags & SVGA3D_CLEAR_COLOR) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout = a.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorRefs[i].attachment = attachmentCount++;
        colorCount = i + 1;
    }

    VkAttachmentReference depthRef = {};
    depthRef.attachment = VK_ATTACHMENT_UNUSED;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    if (depthFormat != VK_FORMAT_UNDEFINED) {
        attachments[attachmentCount].format = depthFormat;
        attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[attachmentCount].loadOp = (clearFlags & SVGA3D_CLEAR_DEPTH) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        bool hasStencil = (depthFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
                           depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                           depthFormat == VK_FORMAT_D16_UNORM_S8_UINT);
        attachments[attachmentCount].stencilLoadOp = hasStencil ?
            ((clearFlags & SVGA3D_CLEAR_STENCIL) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD) : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].stencilStoreOp = hasStencil ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthRef.attachment = attachmentCount;
        attachmentCount++;
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if (colorCount) {
        subpass.colorAttachmentCount = colorCount;
        subpass.pColorAttachments = colorRefs;
    }
    if (depthRef.attachment != VK_ATTACHMENT_UNUSED) {
        subpass.pDepthStencilAttachment = &depthRef;
    }

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = attachmentCount;
    rpInfo.pAttachments = attachments;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;

    VkRenderPass rp = VK_NULL_HANDLE;
    VkResult res = m_dispatch.vkCreateRenderPass(m_device, &rpInfo, nullptr, &rp);
    if (res != VK_SUCCESS) {
        observeDeviceResult(res);
        log_msg("[libqemu_svga3d] getOrCreateRenderPass: vkCreateRenderPass failed (%d)\n", res);
        return VK_NULL_HANDLE;
    }

    m_renderPasses.push_back({ colorFormats, depthFormat, rp, clearFlags });
    return rp;
}

} // namespace svga3_vlkn
