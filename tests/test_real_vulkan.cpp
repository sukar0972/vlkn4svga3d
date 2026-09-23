#include "svga3_vlkn.h"
#include "svga3_device.h"
#include "vlkn_backend.h"
#include <iostream>
#include <cstring>

int main() {
    std::cout << "Testing Real Vulkan Backend Initialization..." << std::endl;

    Svga3VlknConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.appName = "SVGA3=VLKN Real Vulkan Runner";
    cfg.apiVersion = VK_API_VERSION_1_1;
    cfg.forceMockBackend = false;
    cfg.enableValidationLayers = true;
    cfg.stagingBufferSize = 16 * 1024 * 1024;

    std::cout << "Calling svga3_vlkn_device_create..." << std::endl;
    Svga3VlknDevice *dev = svga3_vlkn_device_create(&cfg);
    std::cout << "svga3_vlkn_device_create returned: " << dev << std::endl;
    if (!dev) {
        std::cerr << "FAIL: Failed to create device on real Vulkan driver!" << std::endl;
        return 1;
    }

    svga3_vlkn::VlknBackend *backend = dev->backend.get();
    if (backend->dispatch().isMock) {
        std::cerr << "FAIL: Backend silently fell back to mock!" << std::endl;
        svga3_vlkn_device_destroy(dev);
        return 1;
    }

    std::cout << "SUCCESS: Real Vulkan Driver Loaded!" << std::endl;
    std::cout << "  Device Name:    " << backend->driverName() << std::endl;
    std::cout << "  Vendor ID:      0x" << std::hex << backend->vendorId() << std::dec << std::endl;
    std::cout << "  Device ID:      0x" << std::hex << backend->deviceId() << std::dec << std::endl;
    std::cout << "  API Version:    " << VK_VERSION_MAJOR(backend->apiVersion()) << "."
              << VK_VERSION_MINOR(backend->apiVersion()) << "."
              << VK_VERSION_PATCH(backend->apiVersion()) << std::endl;
    std::cout << "  Driver Version: 0x" << std::hex << backend->driverVersion() << std::dec << std::endl;
    std::cout << "  Is Software:    " << (backend->isSoftware() ? "YES (Lavapipe/CPU)" : "NO (Hardware GPU)") << std::endl;
    std::cout << "  Validation Errs: " << backend->validationErrors() << std::endl;
    std::cout << "  Validation Warn: " << backend->validationWarnings() << std::endl;

    /* Write driver identity artifact */
    FILE *fDriver = fopen("artifacts/vulkan_driver_info.log", "w");
    if (fDriver) {
        fprintf(fDriver, "Vulkan Driver Information & Hardware Identity\n");
        fprintf(fDriver, "==============================================\n");
        fprintf(fDriver, "Device Name:        %s\n", backend->driverName());
        fprintf(fDriver, "Vendor ID:          0x%04x\n", backend->vendorId());
        fprintf(fDriver, "Device ID:          0x%04x\n", backend->deviceId());
        fprintf(fDriver, "API Version:        %u.%u.%u\n",
                VK_VERSION_MAJOR(backend->apiVersion()),
                VK_VERSION_MINOR(backend->apiVersion()),
                VK_VERSION_PATCH(backend->apiVersion()));
        fprintf(fDriver, "Driver Version:     0x%08x\n", backend->driverVersion());
        fprintf(fDriver, "Device Type:        %s\n", backend->isSoftware() ? "CPU / Software (llvmpipe/lavapipe)" : "Discrete / Integrated GPU");
        fprintf(fDriver, "Mock Fallback:      DISABLED (forceMockBackend=false, isMock=%s)\n", backend->dispatch().isMock ? "TRUE" : "FALSE");
        fprintf(fDriver, "Validation Layers:  ENABLED (VK_LAYER_KHRONOS_validation)\n");
        fclose(fDriver);
    }

    /* Write validation layer log artifact */
    FILE *fVal = fopen("artifacts/vulkan_validation.log", "w");
    if (fVal) {
        fprintf(fVal, "Vulkan Validation Layer Execution Log\n");
        fprintf(fVal, "======================================\n");
        fprintf(fVal, "Validation Errors:   %u\n", backend->validationErrors());
        fprintf(fVal, "Validation Warnings: %u\n", backend->validationWarnings());
        fprintf(fVal, "Validation Messages Total: %zu\n", backend->validationMessages().size());
        for (size_t i = 0; i < backend->validationMessages().size(); ++i) {
            fprintf(fVal, "[%03zu] %s\n", i, backend->validationMessages()[i].c_str());
        }
        if (backend->validationMessages().empty()) {
            fprintf(fVal, "CLEAN: Zero validation layer errors or warnings reported.\n");
        }
        fclose(fVal);
    }

    /* Verify a buffer allocation and upload/download */
    VkBuffer buf;
    VkDeviceMemory mem;
    Svga3VlknStatus st = backend->createBuffer(1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                              &buf, &mem);
    if (st != SVGA3_VLKN_SUCCESS) {
        std::cerr << "FAIL: Failed to create buffer on real Vulkan driver!" << std::endl;
        svga3_vlkn_device_destroy(dev);
        return 1;
    }
    std::cout << "SUCCESS: Buffer allocated on real Vulkan device!" << std::endl;

    uint32_t testData[4] = { 0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00 };
    st = backend->uploadToBuffer(buf, 0, testData, sizeof(testData));
    if (st != SVGA3_VLKN_SUCCESS) {
        std::cerr << "FAIL: uploadToBuffer failed: " << st << std::endl;
        svga3_vlkn_device_destroy(dev);
        return 1;
    }

    uint32_t readBack[4] = { 0 };
    st = backend->downloadFromBuffer(readBack, buf, 0, sizeof(readBack));
    if (st != SVGA3_VLKN_SUCCESS) {
        std::cerr << "FAIL: downloadFromBuffer failed: " << st << std::endl;
        svga3_vlkn_device_destroy(dev);
        return 1;
    }

    if (memcmp(testData, readBack, sizeof(testData)) != 0) {
        std::cerr << "FAIL: Data mismatch on buffer readback!" << std::endl;
        svga3_vlkn_device_destroy(dev);
        return 1;
    }
    std::cout << "SUCCESS: Buffer upload and download verified bit-exact on real Vulkan!" << std::endl;

    backend->destroyBuffer(buf, mem);
    svga3_vlkn_device_destroy(dev);
    std::cout << "All Real Vulkan Device sanity checks PASSED!" << std::endl;
    return 0;
}
