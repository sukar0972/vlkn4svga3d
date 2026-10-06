#include "qemu_vmsvga.h"
#include "svga3_device.h"
#include "vlkn_backend.h"
#include "svga3_fifo_framing.h"
#include <cstdio>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
static VkResult VKAPI_CALL failSubmit(VkQueue, uint32_t, const VkSubmitInfo *, VkFence) { return VK_ERROR_OUT_OF_HOST_MEMORY; }

static unsigned lostSubmits;
static VkResult VKAPI_CALL loseSubmit(VkQueue, uint32_t, const VkSubmitInfo *, VkFence) {
    ++lostSubmits; return VK_ERROR_DEVICE_LOST;
}

int main() {
    qemu_vmsvga::QemuVmsvgaDevice host;
    CHECK(host.init(false, true));
    auto *dev = host.vlknDevice();
    auto *fifo = static_cast<uint32_t *>(host.getFifoHva());
    uint32_t min = fifo[SVGA_FIFO_MIN], max = fifo[SVGA_FIFO_MAX];
    auto run = [&](const std::vector<uint32_t> &packet, bool wrap) {
        uint32_t start = wrap ? max - 4 : min;
        fifo[SVGA_FIFO_STOP] = start;
        for (uint32_t word : packet) {
            fifo[start/4] = word;
            start += 4;
            if (start == max) start = min;
        }
        fifo[SVGA_FIFO_NEXT_CMD] = start;
        host.fifoRun();
        return fifo[SVGA_FIFO_STOP] == start;
    };
    const std::vector<std::vector<uint32_t>> commands = {
        {SVGA_CMD_UPDATE, 0, 0, 1, 1},
        {SVGA_CMD_UPDATE_VERBOSE, 0, 0, 1, 1, 0xdeadbeef},
        {SVGA_CMD_DEFINE_SCREEN, sizeof(SVGAScreenObject), 0, 0, 1, 1, 0, 0, 0, 0, 4, 0},
        {SVGA_CMD_DESTROY_SCREEN, 0},
        {SVGA_CMD_DEFINE_GMRFB, SVGA_GMR_FRAMEBUFFER, 0, 4, 0x1820},
        {SVGA_CMD_BLIT_GMRFB_TO_SCREEN, 0, 0, 0, 0, 1, 1, 0},
        {SVGA_CMD_BLIT_SCREEN_TO_GMRFB, 0, 0, 0, 0, 1, 1, 0},
        {SVGA_CMD_ANNOTATION_FILL, 0},
        {SVGA_CMD_ANNOTATION_COPY, 0, 0, 0},
        {2, 0, 0, 0, 1, 1},
        {SVGA_CMD_FRONT_ROP_FILL, 0, 0, 0, 1, 1, 3},
        {SVGA_CMD_DEFINE_ALPHA_CURSOR, 0, 0, 0, 1, 1, 0},
        {SVGA_CMD_DEFINE_CURSOR, 0, 0, 0, 1, 1, 1, 32, 0, 0},
    };
    for (const auto &command : commands) {
        uint64_t words = 0;
        CHECK(svga3_vlkn::fifoFrame(command.size(), [&](uint64_t i) { return command[i]; }, words) == svga3_vlkn::FifoFrameStatus::Complete);
        CHECK(words == command.size());
        auto stream = command;
        stream.insert(stream.end(), {SVGA_CMD_FENCE, 71});
        size_t consumed = 0;
        CHECK(svga3_vlkn_fifo_execute(dev, stream.data(), stream.size()*4, &consumed) == SVGA3_VLKN_SUCCESS);
        CHECK(consumed == stream.size()*4);
        for (bool wrap : {false, true}) {
            fifo[SVGA_FIFO_FENCE] = 0;
            CHECK(run(stream, wrap));
            CHECK(fifo[SVGA_FIFO_FENCE] == 71);
        }
    }
    // Failed framed DEFINE must leave the following UPDATE and fence aligned.
    std::vector<uint32_t> failed = {SVGA_CMD_DEFINE_GMR2, UINT32_MAX, 1, SVGA_CMD_UPDATE, 1, 2, 3, 4, SVGA_CMD_FENCE, 72};
    for (bool wrap : {false, true}) {
        host.clearDisplayUpdates();
        CHECK(run(failed, wrap));
        CHECK(fifo[SVGA_FIFO_FENCE] == 72 && host.displayUpdates().size() == 1);
    }
    std::vector<uint32_t> unknown = {0x12345678, SVGA_CMD_FENCE, 73};
    size_t consumed = 99;
    CHECK(svga3_vlkn_fifo_execute(dev, unknown.data(), unknown.size()*4, &consumed) == SVGA3_VLKN_ERROR_UNSUPPORTED_COMMAND && consumed == 0);
    CHECK(!run(unknown, false) && fifo[SVGA_FIFO_STOP] == min && fifo[SVGA_FIFO_FENCE] == 72);
    std::vector<uint32_t> incomplete = {SVGA_CMD_UPDATE_VERBOSE, 0, 0, 1, 1};
    CHECK(!run(incomplete, true) && fifo[SVGA_FIFO_STOP] == max - 4);
    CHECK(svga3_vlkn_fifo_execute(dev, incomplete.data(), incomplete.size()*4, &consumed) == SVGA3_VLKN_ERROR_INVALID_COMMAND_BUFFER && consumed == 0);

    CHECK(dev->contextMgr->createContext(2) == SVGA3_VLKN_SUCCESS);
    SVGA3dCmdDrawPrimitives draw{2,1,0};
    SVGA3dVertexDecl declaration{};
    std::vector<uint32_t> instanceWire(2 + (sizeof(draw)+sizeof(declaration)+4)/4);
    instanceWire[0]=SVGA_3D_CMD_DRAW_PRIMITIVES; instanceWire[1]=(instanceWire.size()-2)*4;
    std::memcpy(instanceWire.data()+2,&draw,sizeof(draw));
    std::memcpy(reinterpret_cast<uint8_t*>(instanceWire.data()+2)+sizeof(draw),&declaration,sizeof(declaration));
    for (uint32_t divisor : {0x40000002u,0x80000001u,2u}) {
        instanceWire.back()=divisor;
        CHECK(svga3_vlkn_fifo_execute(dev,instanceWire.data(),instanceWire.size()*4,&consumed) == SVGA3_VLKN_ERROR_INVALID_PARAM);
        CHECK(consumed == instanceWire.size()*4);
    }
    instanceWire.back()=1;
    CHECK(svga3_vlkn_fifo_execute(dev,instanceWire.data(),instanceWire.size()*4,&consumed) == SVGA3_VLKN_SUCCESS && consumed == instanceWire.size()*4);
    auto savedSubmit = dev->backend->dispatch().vkQueueSubmit;
    dev->backend->getActiveCommandBuffer();
    dev->backend->dispatch().vkQueueSubmit = failSubmit;
    CHECK(!run({SVGA_CMD_FENCE, 74}, false));
    CHECK(fifo[SVGA_FIFO_STOP] == min && fifo[SVGA_FIFO_FENCE] == 72);
    dev->backend->dispatch().vkQueueSubmit = savedSubmit;
    host.fifoRun();
    CHECK(fifo[SVGA_FIFO_STOP] == fifo[SVGA_FIFO_NEXT_CMD] && fifo[SVGA_FIFO_FENCE] == 74);
    // A true device loss is sticky, but UPDATE, screen objects and fences still drain.
    dev->backend->getActiveCommandBuffer();
    dev->backend->dispatch().vkQueueSubmit = loseSubmit;
    host.clearDisplayUpdates();
    CHECK(run({SVGA_CMD_FENCE,75,SVGA_3D_CMD_CONTEXT_DEFINE,4,999,
        SVGA_CMD_UPDATE,0,0,1,1,SVGA_CMD_DEFINE_SCREEN,sizeof(SVGAScreenObject),1,0,1,1,0,0,0,0,4,0,
        SVGA_CMD_FENCE,76},true));
    CHECK(dev->backend->isDeviceLost() && lostSubmits == 1 && fifo[SVGA_FIFO_FENCE] == 76);
    CHECK(!dev->contextMgr->getContext(999) && host.displayUpdates().size() == 1);
    CHECK(dev->backend->getActiveCommandBuffer() == VK_NULL_HANDLE);
    CHECK(dev->backend->flushCommandBuffer() == SVGA3_VLKN_ERROR_DEVICE_LOST && lostSubmits == 1);
    dev->backend->dispatch().vkQueueSubmit = savedSubmit;
    CHECK(svga3_vlkn_context_create(dev,999) == SVGA3_VLKN_ERROR_DEVICE_LOST);
    CHECK(svga3_vlkn_device_reset(dev) == SVGA3_VLKN_ERROR_DEVICE_LOST);
    CHECK(dev->backend->waitIdle() == SVGA3_VLKN_ERROR_DEVICE_LOST);
    CHECK(run({SVGA_CMD_UPDATE,0,0,1,1,SVGA_CMD_FENCE,77},false));
    CHECK(fifo[SVGA_FIFO_FENCE] == 77 && lostSubmits == 1);
    std::puts("Sticky device loss retains 2D, screen objects and fence framing: PASS");
    std::puts("Shared FIFO framing, wrapped packets, semantic errors and fence completion: PASS");
}
