#pragma once

#include "qemu_vmsvga.h"

namespace qemu_vmsvga {

/*
 * Minimal Guest Test Driver
 *
 * Implements the in-VM operating system side that drives the hardware
 * strictly through PCI discovery, I/O ports, BAR1, BAR2, and Guest RAM.
 * NEVER calls host-side rendering functions directly.
 */
class GuestVmsvgaDriver {
public:
    GuestVmsvgaDriver(QemuVmsvgaDevice *device, size_t guestRamSize = 16 * 1024 * 1024);
    ~GuestVmsvgaDriver();

    /* 1. Discover device via PCI configuration space */
    bool discoverDevice();

    /* 2. Read advertised capabilities and negotiate version */
    bool initializeDevice(uint32_t width, uint32_t height, uint32_t bpp);

    /* 3. Register a Guest Physical Address memory region with the device */
    bool registerGuestMemoryRegion(uint32_t gmrId, uint64_t gpa, size_t size);

    /* 4. Write command packet into FIFO ring buffer (handles wraparound) */
    bool writeFifo(const void *data, size_t sizeBytes);

    /* 5. Synchronize and wait for fence completion */
    bool sync(uint32_t fenceId, uint32_t timeoutLoops = 1000000);

    /* Direct guest memory access */
    uint8_t* guestRam() { return m_guestRam.data(); }
    uint64_t guestRamBaseGpa() const { return m_ramGpaBase; }
    size_t   guestRamSize() const { return m_guestRam.size(); }

    /* Read pixel directly from BAR1 Framebuffer (as guest OS would) */
    uint32_t readFbPixel(uint32_t x, uint32_t y) const;

    /* Advertised capabilities read by guest */
    uint32_t advertisedCapabilities() const { return m_caps; }
    uint32_t negotiatedId() const { return m_negotiatedId; }

    /* Wraparound counter on guest side */
    uint32_t guestWraparounds() const { return m_wraparounds; }

private:
    void outl(uint16_t port, uint32_t val);
    uint32_t inl(uint16_t port);
    void writeReg(uint32_t index, uint32_t val);
    uint32_t readReg(uint32_t index);

    QemuVmsvgaDevice *m_device;
    std::vector<uint8_t> m_guestRam;
    uint64_t m_ramGpaBase = 0x10000000ULL; /* Guest RAM GPA: 256MB mark */

    uint16_t m_ioBase = 0;
    uint32_t *m_fifo = nullptr;
    uint32_t *m_fb = nullptr;
    uint32_t m_caps = 0;
    uint32_t m_negotiatedId = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_bpp = 32;
    uint32_t m_pitch = 0;
    uint32_t m_wraparounds = 0;
};

} /* namespace qemu_vmsvga */
