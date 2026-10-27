/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "svga3_device.h"
#include "vlkn_backend.h"
#include <cstdio>

// Keep the backend alive long enough to inspect messages emitted by teardown.
inline bool finishValidation(Svga3VlknDevice *dev) {
    auto &backend = *dev->backend;
    const auto status = backend.waitIdle();
    dev->contextMgr.reset();
    dev->surfaceMgr.reset();
    dev->guestMem.reset();
    backend.shutdown();
    const bool clean = status == SVGA3_VLKN_SUCCESS &&
        backend.validationErrors() == 0 && backend.validationWarnings() == 0;
    if (!clean) std::fprintf(stderr, "Validation failed: idle=%d errors=%u warnings=%u\n",
        status, backend.validationErrors(), backend.validationWarnings());
    return clean;
}
