/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "svga3_device.h"
#include <array>
#include <cstdio>
#include <vector>
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main() {
    Svga3VlknConfig config{};
    config.enableValidationLayers = true;
    config.stagingBufferSize = 1024;
    auto *dev = svga3_vlkn_device_create(&config);
    CHECK(dev);
    auto &backend = *dev->backend;
    std::array<VkBuffer, 7> buffers{};
    std::array<VkDeviceMemory, 7> memories{};
    std::array<std::vector<uint32_t>, 7> expected;
    for (unsigned i = 0; i < buffers.size(); ++i) {
        CHECK(backend.createBuffer(256, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              &buffers[i], &memories[i]) == SVGA3_VLKN_SUCCESS);
        expected[i].assign(64, 0x10203040u + i);
        auto scratch = expected[i];
        CHECK(backend.uploadToBuffer(buffers[i], 0, scratch.data(), 256) == SVGA3_VLKN_SUCCESS);
        // Reuse the caller's upload memory immediately; GPU work owns its copy.
        scratch.assign(64, 0xff);
    }
    // More submissions than command-buffer slots and a staging wrap must keep
    // each buffer's data intact. A later overwrite must win without changing peers.
    expected[1].assign(64, 0xaabbccddu);
    CHECK(backend.uploadToBuffer(buffers[1], 0, expected[1].data(), 256) == SVGA3_VLKN_SUCCESS);
    for (unsigned i = 0; i < buffers.size(); ++i) {
        std::vector<uint32_t> observed(64);
        CHECK(backend.downloadFromBuffer(observed.data(), buffers[i], 0, 256) == SVGA3_VLKN_SUCCESS);
        CHECK(observed == expected[i]);
    }
    // wait_idle must also submit commands that have not yet reached the queue.
    auto commands = backend.getActiveCommandBuffer();
    CHECK(commands);
    VkBufferCopy copy{0, 0, 256};
    backend.dispatch().vkCmdCopyBuffer(commands, buffers[1], buffers[0], 1, &copy);
    CHECK(svga3_vlkn_device_wait_idle(dev) == SVGA3_VLKN_SUCCESS);
    std::vector<uint32_t> observed(64);
    CHECK(backend.downloadFromBuffer(observed.data(), buffers[0], 0, 256) == SVGA3_VLKN_SUCCESS);
    CHECK(observed == expected[1]);
    // A transfer larger than staging retains its temporary allocation until completion.
    VkBuffer large{}; VkDeviceMemory largeMemory{};
    CHECK(backend.createBuffer(8192, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
          &large, &largeMemory) == SVGA3_VLKN_SUCCESS);
    std::vector<uint32_t> largeData(2048, 0x88776655u), largeResult(2048);
    CHECK(backend.uploadToBuffer(large, 0, largeData.data(), 8192) == SVGA3_VLKN_SUCCESS);
    CHECK(backend.downloadFromBuffer(largeResult.data(), large, 0, 8192) == SVGA3_VLKN_SUCCESS && largeResult == largeData);
    CHECK(svga3_vlkn_device_wait_idle(dev) == SVGA3_VLKN_SUCCESS);
    backend.destroyBuffer(large, largeMemory);
    for (unsigned i = 0; i < buffers.size(); ++i) backend.destroyBuffer(buffers[i], memories[i]);
    dev->backend->setPreFlushHook(nullptr);
    dev->contextMgr.reset(); dev->surfaceMgr.reset(); dev->guestMem.reset();
    backend.shutdown();
    CHECK(backend.validationErrors() == 0 && backend.validationWarnings() == 0);
    svga3_vlkn_device_destroy(dev);
    std::puts("Fence submissions, staging wrap, overwrite order, idle and large-transfer lifetime: PASS");
}
