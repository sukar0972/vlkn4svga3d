// Isolated CPU dispatch benchmark; this does not measure frame rate.
#include "svga3_device.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
    const int count = argc > 1 ? std::atoi(argv[1]) : 2000000;
    if (count <= 0) return 1;
    Svga3VlknConfig config{};
    config.forceMockBackend = true;
    config.appName = "FIFO dispatch benchmark";
    auto *device = svga3_vlkn_device_create(&config);
    if (!device) return 2;
    if (svga3_vlkn_context_create(device, 1) != SVGA3_VLKN_SUCCESS) {
        svga3_vlkn_device_destroy(device);
        return 3;
    }
    struct {
        SVGA3dCmdSetRenderState command;
        SVGA3dRenderState state;
    } packet{};
    packet.command.cid = 1;
    packet.state.state = SVGA3D_RS_ALPHATESTENABLE;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < count; ++i) {
        packet.state.uintValue = i & 1;
        size_t consumed = 0;
        const auto status = svga3_vlkn::processFifoPacket(
            device, SVGA_3D_CMD_SETRENDERSTATE,
            reinterpret_cast<const uint8_t *>(&packet), sizeof(packet), &consumed);
        if (status != SVGA3_VLKN_SUCCESS || consumed != sizeof(packet)) {
            svga3_vlkn_device_destroy(device);
            return 4;
        }
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();
    uint32_t value = 0;
    const auto status = device->contextMgr->getContext(1)->getRenderState(
        SVGA3D_RS_ALPHATESTENABLE, &value);
    svga3_vlkn_device_destroy(device);
    if (status != SVGA3_VLKN_SUCCESS || value != uint32_t((count - 1) & 1)) return 5;
    std::printf("{\"commands\":%d,\"nanoseconds\":%lld,"
                "\"ns_per_command\":%.3f,\"state_verified\":true}\n",
                count, static_cast<long long>(elapsed), double(elapsed) / count);
    return 0;
}
