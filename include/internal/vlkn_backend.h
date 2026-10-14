/*
 * SVGA3=VLKN - Vulkan Backend Device & Memory Management
 */

#ifndef ___VLKN_BACKEND_H___
#define ___VLKN_BACKEND_H___

#include "svga3_vlkn.h"
#include "vlkn_dispatch.h"
#include "vlkn_resource_budgets.h"
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <mutex>
#include <functional>

namespace svga3_vlkn {

/* Device-level calls are externally serialized. Three command buffers share
 * one queue; each submission owns a fence until its buffer can be reused.
 * Uploads retain disjoint staging ranges across submissions. Staging wraps,
 * CPU readbacks, guest fences and resource destruction drain all submissions
 * before reusing memory. Completion serials advance only at these drains, so
 * context constant rings cannot reset over another in-flight draw.
 * The pre-flush hook only ends render passes; it must not call transfer APIs
 * because callers may hold the non-recursive staging mutex. */
struct PerformanceCounters {
    uint64_t presentationCalls = 0;
    uint64_t readbackBytes = 0;
    uint64_t framebufferCopyBytes = 0;
    uint64_t framebufferCopyCalls = 0;
    uint64_t queueSubmissions = 0;
    uint64_t queueWaitNanoseconds = 0;
    uint64_t identicalShaderDefinitions = 0;
};

struct GraphicsDrawState {
    VkViewport viewport{};
    VkRect2D scissor{};
    uint32_t blendColor = 0;
    float constantBias = 0, slopeBias = 0;
    std::array<std::array<uint32_t, 2>, 3> stencil{};
};

class VlknBackend {
public:
    VlknBackend();
    ~VlknBackend();

    Svga3VlknStatus init(const Svga3VlknConfig *config);
    void shutdown();
    const char *initializationError() const { return m_initializationError; }
    Svga3VlknStatus waitIdle();
    bool isDeviceLost() const { return m_deviceLost; }
    uint64_t completedSubmissionSerial() const { return m_completedSubmissionSerial; }

    /* Memory allocation helpers */
    /* Returns the memory type index, or -1 if no type in typeFilter has all
     * requested properties. Never falls back to a type lacking the properties. */
    int findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties,
                       VkMemoryPropertyFlags preferredProperties = 0);
    Svga3VlknStatus allocateMemory(VkDeviceSize size, uint32_t memoryTypeIndex, VkDeviceMemory *outMemory);
    void freeMemory(VkDeviceMemory memory);

    /* Buffer creation helpers */
    Svga3VlknStatus createBuffer(VkDeviceSize size,
                                 VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags properties,
                                 VkBuffer *outBuffer,
                                 VkDeviceMemory *outMemory,
                                 VkMemoryPropertyFlags preferredProperties = 0);
    void destroyBuffer(VkBuffer buffer, VkDeviceMemory memory);

    /* Staging buffer operations */
    Svga3VlknStatus uploadToBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, const void *srcData, VkDeviceSize size);
    Svga3VlknStatus downloadFromBuffer(void *dstData, VkBuffer srcBuffer, VkDeviceSize srcOffset, VkDeviceSize size);
    void recordHostReadBarrier(VkCommandBuffer commands, VkBuffer buffer,
                               VkDeviceSize offset, VkDeviceSize size);
    // Optional coherent import of the registered framebuffer. Every use is
    // completed before returning to its owner; registration changes release it.
    VkBuffer importFramebuffer(void *pointer, size_t size, size_t requiredSize = 0);
    // Only platform-owned allocations valid through process exit may opt in:
    // the device-loss policy retains GPU allocations when completion is unknown.
    Svga3VlknStatus enableFramebufferImport(bool enabled);
    VkDeviceSize framebufferImportAlignment() const { return m_allowFramebufferImport ? m_hostImportAlignment : 0; }
    Svga3VlknStatus releaseFramebufferImport();

    // Opt-in measurements; callers serialize access through the device lock.
    PerformanceCounters *performanceCounters() { return m_measurePerformance ? &m_performanceCounters : nullptr; }
    void enablePerformanceCounters(bool enabled) { m_measurePerformance = enabled; }
    void resetPerformanceCounters() { m_performanceCounters = {}; }

    /* Command buffer management */
    VkCommandBuffer getActiveCommandBuffer();
    void bindGraphicsPipeline(VkCommandBuffer commands, VkPipeline pipeline);
    void setGraphicsDrawState(VkCommandBuffer commands, const GraphicsDrawState &state);
    // Submit without draining. At most three command buffers are in flight.
    Svga3VlknStatus submitCommandBuffer();
    Svga3VlknStatus flushCommandBuffer();
    bool cmdBufferRecording() const { return m_cmdBufferRecording; }
    uint64_t recordingSerial() const { return m_recordingSerial; }
    void retireBuffer(VkBuffer buffer, VkDeviceMemory memory, void *mapped, uint64_t serial, size_t budgetBytes);
    void cleanupRetiredBuffers(bool forceAll = false);
    void retireImage(VkImage image, VkDeviceMemory memory, std::vector<VkImageView> views);
    void cleanupRetiredImages();

    /* Render pass execution with state tracking and safety guard */
    bool isRenderPassActive() const { return m_renderPassActive; }
    void cmdBeginRenderPass(VkCommandBuffer cb, const VkRenderPassBeginInfo *pBegin, VkSubpassContents contents);
    void cmdEndRenderPass(VkCommandBuffer cb);

    /* Accessors */
    VlknDispatchTable& dispatch() { return m_dispatch; }
    VkInstance instance() const { return m_instance; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice device() const { return m_device; }
    VkQueue queue() const { return m_queue; }
    bool enableSubmissionFenceWait(uint32_t spinMicroseconds);
    uint32_t queueFamilyIndex() const { return m_queueFamilyIndex; }
    const VkPhysicalDeviceProperties& properties() const { return m_props; }
    const VkPhysicalDeviceFeatures& features() const { return m_features; }
    bool customBorderColors() const { return m_customBorderColors; }
    bool imageFormatList() const { return m_imageFormatList; }
    const VkPhysicalDeviceMemoryProperties& memoryProperties() const { return m_memProps; }
    VkDescriptorPool descriptorPool() const { return m_descriptorPool; }

    /* Validation & Driver Info */
    uint32_t validationErrors() const { return m_validationErrors; }
    uint32_t validationWarnings() const { return m_validationWarnings; }
    const std::vector<std::string>& validationMessages() const { return m_validationMessages; }
    void addValidationMessage(bool isError, const std::string &msg);

    const char* driverName() const { return m_props.deviceName; }
    uint32_t apiVersion() const { return m_props.apiVersion; }
    uint32_t driverVersion() const { return m_props.driverVersion; }
    uint32_t vendorId() const { return m_props.vendorID; }
    uint32_t deviceId() const { return m_props.deviceID; }
    bool isSoftware() const { return m_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU; }

    /* Device-wide aggregate resource budgets (guest-exhaustion guard).
     * Shared by the surface and context managers; see
     * vlkn_resource_budgets.h. */
    VlknResourceBudgets& resourceBudgets() { return m_budgets; }
    const VlknResourceBudgets& resourceBudgets() const { return m_budgets; }

    /* Default / shared RenderPass for color + depth/stencil */
    VkRenderPass getOrCreateRenderPass(VkFormat colorFormat, VkFormat depthFormat);
    VkRenderPass getOrCreateRenderPass(const std::array<VkFormat, 4> &colorFormats, VkFormat depthFormat,
                                     uint32_t clearFlags = 0);

    /* Staging Buffer Accessors */
    VkBuffer stagingBuffer() const { return m_stagingBuffer; }
    void* stagingMapped() const { return m_stagingMapped; }
    size_t stagingSize() const { return m_stagingSize; }
    std::mutex& stagingMutex() { return m_stagingMutex; }

    /* Staging bump allocator. The caller must hold stagingMutex(). Returns
     * the byte offset of a `size`-byte range, or kStagingAllocFailed when the
     * range does not fit in the remaining space. On failure the caller must
     * flushCommandBuffer() (which retires all pending ranges and resets the
     * bump) and retry once; if it still fails the transfer is larger than the
     * staging buffer and needs a temporary buffer. Returned offsets satisfy
     * optimalBufferCopyOffsetAlignment, so they are valid for both
     * vkCmdCopyBuffer and vkCmdCopyBufferToImage/vkCmdCopyImageToBuffer. */
    static constexpr VkDeviceSize kStagingAllocFailed = ~VkDeviceSize(0);
    VkDeviceSize stagingAlloc(VkDeviceSize size);
    void* stagingMappedAt(VkDeviceSize offset) {
        return static_cast<uint8_t*>(m_stagingMapped) + offset;
    }

    /* Fallback Buffer Accessors (always-valid dummy buffer for missing vertex/index inputs) */
    VkBuffer fallbackBuffer() const { return m_fallbackBuffer; }
    size_t fallbackSize() const { return m_fallbackSize; }

    /* Pre-flush hook (e.g. to end any active render passes before ending command buffer) */
    void setPreFlushHook(std::function<void()> hook) { m_preFlushHook = std::move(hook); }

private:
    bool m_deviceLost = false;
    void observeDeviceResult(VkResult result);
    const char *m_initializationError = "Vulkan initialization failed";
    bool m_measurePerformance = false;
    PerformanceCounters m_performanceCounters{};
    Svga3VlknStatus initInstance(const Svga3VlknConfig *config);
    Svga3VlknStatus selectPhysicalDevice(const Svga3VlknConfig *config);
    Svga3VlknStatus initDevice(const Svga3VlknConfig *config);
    Svga3VlknStatus initStagingBuffer(size_t size);
    Svga3VlknStatus initFallbackBuffer(size_t size);

    VlknDispatchTable m_dispatch;
    VkInstance m_instance;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkQueue m_queue;
    uint32_t m_queueFamilyIndex;

    VkPhysicalDeviceProperties m_props;
    VkPhysicalDeviceFeatures m_features;
    bool m_customBorderColors = false;
    bool m_imageFormatList = false;
    VkDeviceSize m_hostImportAlignment = 0;
    bool m_allowFramebufferImport = false;
    PFN_vkGetMemoryHostPointerPropertiesEXT m_getHostPointerProperties = nullptr;
    VkBuffer m_framebufferBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_framebufferMemory = VK_NULL_HANDLE;
    void *m_framebufferPointer = nullptr;
    size_t m_framebufferSize = 0;
    VkPhysicalDeviceMemoryProperties m_memProps;

    static constexpr uint32_t kSubmissionSlots = 3;
    struct SubmissionSlot {
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        bool pending = false;
        bool needsReset = false;
    };
    std::array<SubmissionSlot, kSubmissionSlots> m_submissions{};
    uint32_t m_currentSlot = 0;
    uint64_t m_lastSubmittedSerial = 0;
    Svga3VlknStatus prepareSubmissionSlot();
    Svga3VlknStatus drainSubmissions();
    VkResult waitForSubmission(VkFence fence);
    VkCommandPool m_cmdPool;
    VkCommandBuffer m_cmdBuffer;
    PFN_vkResetFences m_resetSubmissionFence = nullptr;
    PFN_vkGetFenceStatus m_submissionFenceStatus = nullptr;
    PFN_vkWaitForFences m_waitSubmissionFence = nullptr;
    uint32_t m_fenceSpinMicroseconds = 0;
    bool m_cmdBufferRecording;
    bool m_renderPassActive;
    VkPipeline m_boundGraphicsPipeline = VK_NULL_HANDLE;
    bool m_graphicsDrawStateValid = false;
    GraphicsDrawState m_graphicsDrawState{};
    uint64_t m_completedSubmissionSerial;
    uint64_t m_recordingSerial;

    struct RetiredBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
        void *mapped;
        uint64_t serial;
        size_t budgetBytes;
    };
    std::vector<RetiredBuffer> m_retiredBuffers;
    struct RetiredImage {
        VkImage image;
        VkDeviceMemory memory;
        std::vector<VkImageView> views;
        uint64_t serial;
    };
    std::vector<RetiredImage> m_retiredImages;

    VkDescriptorPool m_descriptorPool;

    /* Validation */
    VkDebugUtilsMessengerEXT m_debugMessenger;
    uint32_t m_validationErrors;
    uint32_t m_validationWarnings;
    std::vector<std::string> m_validationMessages;
    std::mutex m_validationMutex;

    std::function<void()> m_preFlushHook;

    /* Staging buffer */
    VkBuffer m_stagingBuffer;
    VkDeviceMemory m_stagingMemory;
    void *m_stagingMapped;
    size_t m_stagingSize;
    std::mutex m_stagingMutex;
    /* Bump offset into the staging buffer. Only touched while holding
     * m_stagingMutex. Reset to 0 whenever flushCommandBuffer() establishes
     * device idle, which retires all recorded staging ranges. */
    VkDeviceSize m_stagingBump;

    /* Fallback buffer */
    VkBuffer m_fallbackBuffer;
    VkDeviceMemory m_fallbackMemory;
    size_t m_fallbackSize;

    /* Default RenderPass cache */
    struct RenderPassEntry {
        std::array<VkFormat, 4> colorFormats;
        VkFormat depthFormat;
        VkRenderPass renderPass;
        uint32_t clearFlags;
    };
    std::vector<RenderPassEntry> m_renderPasses;
    std::mutex m_mutex;

    /* Aggregate device resource budgets (see vlkn_resource_budgets.h). */
    VlknResourceBudgets m_budgets;
};

} // namespace svga3_vlkn

#endif /* ___VLKN_BACKEND_H___ */
