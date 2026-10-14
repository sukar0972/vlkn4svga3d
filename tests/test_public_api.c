/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "svga3_vlkn.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main(void) {
    Svga3VlknConfigV1 cfg;
    svga3_vlkn_config_init(&cfg);
    CHECK(cfg.structSize == sizeof(cfg) && cfg.version == SVGA3_VLKN_CONFIG_VERSION);
    cfg.device.forceMockBackend = true;
    cfg.device.maxContexts = 1;
    cfg.device.maxSurfaces = 2;
    Svga3VlknDevice *dev = NULL;
    char error[128];
    CHECK(svga3_vlkn_device_create_ex(&cfg, &dev, error, sizeof(error)) == SVGA3_VLKN_SUCCESS);
    CHECK(dev && !error[0]);
    CHECK(svga3_vlkn_context_create(dev, 1) == SVGA3_VLKN_SUCCESS);
    CHECK(svga3_vlkn_context_create(dev, 2) == SVGA3_VLKN_ERROR_OUT_OF_MEMORY);
    svga3_vlkn_device_destroy(dev);
    cfg.version++;
    CHECK(svga3_vlkn_device_create_ex(&cfg, &dev, error, sizeof(error)) == SVGA3_VLKN_ERROR_INVALID_PARAM);
    CHECK(!dev && error[0]);
    cfg.version = SVGA3_VLKN_CONFIG_VERSION;
    cfg.structSize = sizeof(cfg) - 1;
    CHECK(svga3_vlkn_device_create_ex(&cfg, &dev, error, sizeof(error)) == SVGA3_VLKN_ERROR_INVALID_PARAM);
    svga3_vlkn_config_init(&cfg);
    cfg.device.maxContexts = 65;
    CHECK(svga3_vlkn_device_create_ex(&cfg, &dev, error, sizeof(error)) == SVGA3_VLKN_ERROR_INVALID_PARAM);
    CHECK(svga3_vlkn_device_create_ex(NULL, NULL, error, 1) == SVGA3_VLKN_ERROR_INVALID_PARAM && !error[0]);
    uint32_t wire[] = {SVGA_CMD_FENCE, 123, SVGA_CMD_FENCE, 456};
    size_t size = 123;
    CHECK(svga3_vlkn_fifo_packet_size(wire, 4, &size) == SVGA3_VLKN_ERROR_INVALID_COMMAND_BUFFER && !size);
    CHECK(svga3_vlkn_fifo_packet_size(wire, sizeof(wire), &size) == SVGA3_VLKN_SUCCESS && size == 8);
    CHECK(!strcmp(svga3_vlkn_version(), SVGA3_VLKN_VERSION));
    puts("Public C ABI, config validation, diagnostics and packet framing: PASS");
    return 0;
}
