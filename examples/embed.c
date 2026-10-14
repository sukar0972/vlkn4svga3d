/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <svga3_vlkn.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    Svga3VlknConfigV1 config;
    svga3_vlkn_config_init(&config);
    config.device.appName = "vlkn embedding example";
    config.device.forceMockBackend = argc == 2 && strcmp(argv[1], "--mock") == 0;
    config.device.maxSurfaces = 16;
    config.device.maxContexts = 2;
    char diagnostic[256];
    Svga3VlknDevice *device = NULL;
    Svga3VlknStatus status = svga3_vlkn_device_create_ex(
        &config, &device, diagnostic, sizeof(diagnostic));
    if (status != SVGA3_VLKN_SUCCESS) {
        fprintf(stderr, "Device initialization failed (%d): %s\n", status, diagnostic);
        return 1;
    }
    printf("vlkn %s: device initialized\n", svga3_vlkn_version());
    svga3_vlkn_device_destroy(device);
    return 0;
}
