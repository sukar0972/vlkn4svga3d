#include "svga3_device.h"
#include "svga3_context.h"
#include "svga3_guest_mem.h"
#include "vlkn_backend.h"
#include <cstdio>
#include <cstring>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
static VkResult VKAPI_CALL failSubmit(VkQueue, uint32_t, const VkSubmitInfo *, VkFence) { return VK_ERROR_OUT_OF_HOST_MEMORY; }
static VkResult VKAPI_CALL failPool(VkDevice, const VkQueryPoolCreateInfo *, const VkAllocationCallbacks *, VkQueryPool *) { return VK_ERROR_OUT_OF_HOST_MEMORY; }
int main() {
    Svga3VlknConfig cfg{};
    cfg.forceMockBackend = true;
    auto *dev = svga3_vlkn_device_create(&cfg);
    CHECK(dev);
    CHECK(dev->contextMgr->createContext(1) == SVGA3_VLKN_SUCCESS);
    auto *ctx = dev->contextMgr->getContext(1);
    auto &dispatch = dev->backend->dispatch();
    auto type = SVGA3D_QUERYTYPE_OCCLUSION;
    uint32_t count = 0xdeadbeef;
    CHECK(ctx->waitForQuery(type, &count) == SVGA3_VLKN_ERROR_INVALID_PARAM);
    CHECK(ctx->beginQuery(type) == SVGA3_VLKN_SUCCESS);
    CHECK(ctx->waitForQuery(type, &count) == SVGA3_VLKN_ERROR_INVALID_PARAM && count == 0xdeadbeef);
    CHECK(ctx->isQueryActive(type));
    CHECK(ctx->endQuery(type) == SVGA3_VLKN_SUCCESS);
    CHECK(ctx->waitForQuery(type, &count) == SVGA3_VLKN_SUCCESS && count == 1920*1080);
    CHECK(ctx->beginQuery(type) == SVGA3_VLKN_SUCCESS);
    auto savedSubmit = dispatch.vkQueueSubmit;
    dispatch.vkQueueSubmit = failSubmit;
    CHECK(ctx->endQuery(type) != SVGA3_VLKN_SUCCESS && !ctx->isQueryActive(type));
    count = 0xdeadbeef;
    CHECK(ctx->waitForQuery(type, &count) != SVGA3_VLKN_SUCCESS && count == 0xdeadbeef);
    dispatch.vkQueueSubmit = savedSubmit;
    CHECK(ctx->beginQuery(type) == SVGA3_VLKN_SUCCESS && ctx->endQuery(type) == SVGA3_VLKN_SUCCESS);

    uint8_t framebuffer[64]{};
    CHECK(dev->guestMem->setFramebuffer(framebuffer, 0, sizeof(framebuffer), 4, 4, 16, 4) == SVGA3_VLKN_SUCCESS);
    SVGA3dCmdEndQuery end{};
    end.cid = 1; end.type = type; end.guestResult = {SVGA_GMR_FRAMEBUFFER, 0};
    SVGA3dCmdWaitForQuery wait{};
    wait.cid = 1; wait.type = type; wait.guestResult = end.guestResult;
    auto execute = [&](uint32_t cmd, const void *body, size_t size) {
        std::vector<uint32_t> wire(2 + (size+3)/4);
        wire[0] = cmd; wire[1] = size;
        std::memcpy(wire.data()+2, body, size);
        size_t consumed = 0;
        auto status = svga3_vlkn_fifo_execute(dev, wire.data(), wire.size()*4, &consumed);
        if (consumed != wire.size()*4) return SVGA3_VLKN_ERROR_INVALID_COMMAND_BUFFER;
        return status;
    };
    SVGA3dQueryResult result{};
    auto readResult = [&] { std::memcpy(&result, framebuffer, sizeof(result)); };
    CHECK(ctx->beginQuery(type) == SVGA3_VLKN_SUCCESS);
    CHECK(execute(SVGA_3D_CMD_END_QUERY, &end, sizeof(end)) == SVGA3_VLKN_SUCCESS);
    readResult(); CHECK(result.state == SVGA3D_QUERYSTATE_PENDING && result.totalSize == sizeof(result));
    CHECK(execute(SVGA_3D_CMD_WAIT_FOR_QUERY, &wait, sizeof(wait)) == SVGA3_VLKN_SUCCESS);
    readResult(); CHECK(result.state == SVGA3D_QUERYSTATE_SUCCEEDED && result.result32 == 1920*1080);
    CHECK(ctx->beginQuery(type) == SVGA3_VLKN_SUCCESS);
    dispatch.vkQueueSubmit = failSubmit;
    CHECK(execute(SVGA_3D_CMD_END_QUERY, &end, sizeof(end)) != SVGA3_VLKN_SUCCESS);
    readResult(); CHECK(result.state == SVGA3D_QUERYSTATE_FAILED);
    CHECK(execute(SVGA_3D_CMD_WAIT_FOR_QUERY, &wait, sizeof(wait)) != SVGA3_VLKN_SUCCESS);
    readResult(); CHECK(result.state == SVGA3D_QUERYSTATE_FAILED);
    dispatch.vkQueueSubmit = savedSubmit;
    CHECK(ctx->beginQuery(type) == SVGA3_VLKN_SUCCESS && ctx->endQuery(type) == SVGA3_VLKN_SUCCESS);
    wait.guestResult.offset = sizeof(framebuffer)-1;
    CHECK(execute(SVGA_3D_CMD_WAIT_FOR_QUERY, &wait, sizeof(wait)) == SVGA3_VLKN_ERROR_INVALID_PARAM);

    auto savedPool = dispatch.vkCreateQueryPool;
    dispatch.vkCreateQueryPool = failPool;
    CHECK(dev->contextMgr->createContext(2) == SVGA3_VLKN_SUCCESS);
    auto *unavailable = dev->contextMgr->getContext(2);
    count = 0xdeadbeef;
    CHECK(unavailable->beginQuery(type) != SVGA3_VLKN_SUCCESS && unavailable->endQuery(type) != SVGA3_VLKN_SUCCESS);
    CHECK(unavailable->waitForQuery(type, &count) != SVGA3_VLKN_SUCCESS && count == 0xdeadbeef);
    dispatch.vkCreateQueryPool = savedPool;

    VkQueryPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.queryType = VK_QUERY_TYPE_OCCLUSION; info.queryCount = 2;
    VkQueryPool pool;
    CHECK(dispatch.vkCreateQueryPool(dev->backend->device(), &info, nullptr, &pool) == VK_SUCCESS);
    uint64_t slots[6]; std::fill(std::begin(slots),std::end(slots),UINT64_MAX);
    auto flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
    CHECK(dispatch.vkGetQueryPoolResults(dev->backend->device(), pool, 0, 2, sizeof(slots), slots, 24, flags|VK_QUERY_RESULT_WAIT_BIT) == VK_NOT_READY);
    CHECK(slots[0] == UINT64_MAX && slots[1] == 0 && slots[2] == UINT64_MAX && slots[4] == 0);
    auto cb = dev->backend->getActiveCommandBuffer();
    dispatch.vkCmdBeginQuery(cb,pool,0,0); dispatch.vkCmdEndQuery(cb,pool,0);
    CHECK(dispatch.vkGetQueryPoolResults(dev->backend->device(), pool, 0, 1, sizeof(slots), slots, 24, flags) == VK_SUCCESS);
    CHECK(slots[0] == 1920*1080 && slots[1] == 1 && slots[2] == UINT64_MAX);
    dispatch.vkCmdResetQueryPool(cb,pool,0,1);
    CHECK(dispatch.vkGetQueryPoolResults(dev->backend->device(), pool, 0, 1, sizeof(slots), slots, 24, flags) == VK_NOT_READY);
    CHECK(dev->backend->flushCommandBuffer() == SVGA3_VLKN_SUCCESS);
    dispatch.vkDestroyQueryPool(dev->backend->device(),pool,nullptr);
    svga3_vlkn_device_destroy(dev);
    std::puts("Query FSM, guest completion states, failed result writes and mock readiness: PASS");
}
