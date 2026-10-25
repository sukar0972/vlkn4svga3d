/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "validation_check.h"
#include <cstring>
int main(int argc, char **argv) {
    Svga3VlknConfig cfg{};
    cfg.forceMockBackend = true;
    auto *dev = svga3_vlkn_device_create(&cfg);
    if (!dev) return 2;
    if (argc > 1) dev->backend->addValidationMessage(
        std::strcmp(argv[1], "error") == 0, "Injected validation regression");
    const bool clean = finishValidation(dev);
    svga3_vlkn_device_destroy(dev);
    return clean ? 0 : 1;
}
