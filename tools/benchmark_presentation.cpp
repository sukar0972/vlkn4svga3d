// Real Vulkan -> registered CPU framebuffer benchmark, not an FPS prediction.
#include "svga3_device.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

int main(int argc, char **argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 200;
    if (iterations <= 0 || iterations > 1000000) return 1;
    Svga3VlknConfig config{};
    config.appName = "Presentation benchmark";
    config.enableValidationLayers = std::getenv("BENCHMARK_VALIDATE") != nullptr;
    auto *dev = svga3_vlkn_device_create(&config);
    if (!dev || dev->backend->dispatch().isMock) return 2;
    constexpr uint32_t width = 800, height = 600, baseline = 0x12345678;
    std::vector<uint32_t> pixels(width * height), framebuffer(width * height);
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            pixels[y * width + x] = 0xff000000 | ((x * 257 + y * 65537) & 0xffffff);
    const SVGA3dSize size{width, height, 1};
    if (svga3_vlkn_surface_define(dev, 1, SVGA3D_SURFACE_HINT_TEXTURE, SVGA3D_A8R8G8B8, &size, 1) ||
        svga3_vlkn_surface_dma_upload(dev, 1, 0, nullptr, pixels.data(), width * 4) ||
        svga3_vlkn_device_set_framebuffer(dev, framebuffer.data(), 0, framebuffer.size() * 4,
                                        width, height, width * 4, 4) ||
        svga3_vlkn_device_wait_idle(dev)) return 3;
    const SVGA3dCopyRect small{30, 40, 64, 64, 100, 120};
    const SVGA3dCopyRect outside{width + 10, 0, 64, 64, 0, 0};
    const SVGA3dSurfaceImageId image{1, 0, 0};
    const SVGASignedRect full{0, 0, int32_t(width), int32_t(height)};
    const SVGASignedRect overlap[]{{100, 120, 228, 248}, {164, 120, 292, 248}};
    dev->backend->enablePerformanceCounters(true);
    for (const char *scenario : {"full", "small", "overlap", "offscreen"}) {
        const std::string name(scenario);
        std::fill(framebuffer.begin(), framebuffer.end(), baseline);
        auto present = [&]() {
            if (name == "overlap") return dev->surfaceMgr->blitSurfaceToScreen(image, full, 0, full, overlap, 2, dev->guestMem.get());
            const auto *rect = name == "small" ? &small : name == "offscreen" ? &outside : nullptr;
            return svga3_vlkn_surface_present(dev, 1, rect, rect ? 1 : 0);
        };
        for (int i = 0; i < 10; ++i) if (present()) return 4;
        if (svga3_vlkn_device_wait_idle(dev)) return 5;
        dev->backend->resetPerformanceCounters();
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) if (present()) return 6;
        if (svga3_vlkn_device_wait_idle(dev)) return 7;
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t expected = baseline;
                if (name == "full") expected = pixels[y * width + x];
                if (name == "small" && x >= small.x && x < small.x + small.w && y >= small.y && y < small.y + small.h)
                    expected = pixels[(y - small.y + small.srcy) * width + x - small.x + small.srcx];
                if (name == "overlap" && x >= 100 && x < 292 && y >= 120 && y < 248) expected = pixels[y * width + x];
                if (framebuffer[y * width + x] != expected) return 8;
            }
        }
        const auto c = *dev->backend->performanceCounters();
        std::printf("{\"scenario\":\"%s\",\"iterations\":%d,\"ns_per_present\":%.3f,\"readback_bytes\":%llu,\"cpu_copy_bytes\":%llu,\"cpu_copy_calls\":%llu,\"submissions\":%llu,\"queue_wait_ns\":%llu,\"pixels_verified\":true}\n", scenario, iterations, double(elapsed)/iterations,
                    (unsigned long long)c.readbackBytes, (unsigned long long)c.framebufferCopyBytes, (unsigned long long)c.framebufferCopyCalls,
                    (unsigned long long)c.queueSubmissions, (unsigned long long)c.queueWaitNanoseconds);
    }
    // Repeated definitions retain translation/module work, but this CPU timing
    // is separate from presentation throughput and end-to-end frame rate.
    if (svga3_vlkn_context_create(dev, 1)) return 11;
    const uint32_t shader[]{0xffff0300, 0x0000ffff};
    if (svga3_vlkn_context_define_shader(dev, 1, 1, SVGA3D_SHADERTYPE_PS, shader, 2)) return 12;
    dev->backend->resetPerformanceCounters();
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i)
        if (svga3_vlkn_context_define_shader(dev, 1, 1, SVGA3D_SHADERTYPE_PS, shader, 2)) return 13;
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    std::printf("{\"scenario\":\"identical_shader\",\"iterations\":%d,\"ns_per_define\":%.3f,\"reused_definitions\":%llu}\n",
                iterations, double(elapsed)/iterations, (unsigned long long)dev->backend->performanceCounters()->identicalShaderDefinitions);
    dev->contextMgr->clear(); dev->surfaceMgr->clear();
    if (svga3_vlkn_device_wait_idle(dev)) return 9;
    dev->backend->shutdown();
    const bool clean = dev->backend->validationErrors() == 0 && dev->backend->validationWarnings() == 0;
    svga3_vlkn_device_destroy(dev);
    return clean ? 0 : 10;
}
