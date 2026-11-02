#include "guest_vmsvga_driver.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cstring>

namespace qemu_vmsvga {

/* =========================================================================
 * GuestVmsvgaDriver Implementation
 * ========================================================================= */

GuestVmsvgaDriver::GuestVmsvgaDriver(QemuVmsvgaDevice *device, size_t guestRamSize)
    : m_device(device)
{
    m_guestRam.resize(guestRamSize, 0);

    /* Map guest RAM into device's memory manager */
    m_device->mapGuestRam(m_ramGpaBase, m_guestRam.data(), m_guestRam.size());

    m_fifo = reinterpret_cast<uint32_t*>(m_device->getFifoHva());
    m_fb = reinterpret_cast<uint32_t*>(m_device->getFramebufferHva());
}

GuestVmsvgaDriver::~GuestVmsvgaDriver()
{
    m_device->unmapGuestRam(m_ramGpaBase);
}

void GuestVmsvgaDriver::outl(uint16_t port, uint32_t val)
{
    m_device->ioWrite(port - m_ioBase, val, 4);
}

uint32_t GuestVmsvgaDriver::inl(uint16_t port)
{
    return m_device->ioRead(port - m_ioBase, 4);
}

void GuestVmsvgaDriver::writeReg(uint32_t index, uint32_t val)
{
    outl(m_ioBase + SVGA_INDEX_PORT, index);
    outl(m_ioBase + SVGA_VALUE_PORT, val);
}

uint32_t GuestVmsvgaDriver::readReg(uint32_t index)
{
    outl(m_ioBase + SVGA_INDEX_PORT, index);
    return inl(m_ioBase + SVGA_VALUE_PORT);
}

bool GuestVmsvgaDriver::discoverDevice()
{
    /* Read PCI Config Space */
    uint16_t vendorId = static_cast<uint16_t>(m_device->pciConfigRead(0x00, 2));
    uint16_t deviceId = static_cast<uint16_t>(m_device->pciConfigRead(0x02, 2));
    uint32_t classCode = m_device->pciConfigRead(0x08, 4) >> 8;
    uint32_t bar0 = m_device->pciConfigRead(0x10, 4);
    uint32_t bar1 = m_device->pciConfigRead(0x14, 4);
    uint32_t bar2 = m_device->pciConfigRead(0x18, 4);

    if (vendorId != 0x15AD || deviceId != 0x0405) {
        return false;
    }
    if ((classCode >> 8) != 0x0300) { /* Display class */
        return false;
    }
    if ((bar0 & 0x1) != 0x1) { /* BAR0 must be I/O */
        return false;
    }

    m_ioBase = bar0 & ~0x3;
    (void)bar1;
    (void)bar2;
    return true;
}

bool GuestVmsvgaDriver::initializeDevice(uint32_t width, uint32_t height, uint32_t bpp)
{
    /* 1. Negotiate Version ID (Guest requests ID_2) */
    writeReg(SVGA_REG_ID, SVGA_ID_2);
    m_negotiatedId = readReg(SVGA_REG_ID);
    if (m_negotiatedId != SVGA_ID_2) {
        return false;
    }

    /* 2. Read Advertised Capabilities */
    m_caps = readReg(SVGA_REG_CAPABILITIES);

    /* 3. Configure Display Geometry */
    m_width = width;
    m_height = height;
    m_bpp = bpp;
    writeReg(SVGA_REG_WIDTH, width);
    writeReg(SVGA_REG_HEIGHT, height);
    writeReg(SVGA_REG_BITS_PER_PIXEL, bpp);

    m_pitch = readReg(SVGA_REG_BYTES_PER_LINE);

    /* 4. Enable Device */
    writeReg(SVGA_REG_ENABLE, 1);
    writeReg(SVGA_REG_CONFIG_DONE, 1);

    /* 5. Initialize Extended FIFO */
    uint32_t minOffset = 4 * 1024;
    uint32_t maxOffset = static_cast<uint32_t>(m_device->getFifoSize());
    m_fifo[SVGA_FIFO_MIN] = minOffset;
    m_fifo[SVGA_FIFO_MAX] = maxOffset;
    m_fifo[SVGA_FIFO_NEXT_CMD] = minOffset;
    m_fifo[SVGA_FIFO_STOP] = minOffset;
    m_fifo[SVGA_FIFO_CAPABILITIES] = SVGA_FIFO_CAP_FENCE | SVGA_FIFO_CAP_3D_HWVERSION_REVISED;
    m_fifo[SVGA_FIFO_FLAGS] = 0;
    m_fifo[SVGA_FIFO_FENCE] = 0;
    m_fifo[SVGA_FIFO_3D_HWVERSION] = SVGA3D_HWVERSION_WS6_B1;

    return true;
}

bool GuestVmsvgaDriver::registerGuestMemoryRegion(uint32_t gmrId, uint64_t gpa, size_t size)
{
    /* Use SVGA_CMD_DEFINE_GMR2 and SVGA_CMD_REMAP_GMR2 via FIFO */
    uint32_t numPages = static_cast<uint32_t>((size + 4095) / 4096);

    struct {
        uint32_t cmd;
        SVGAFifoCmdDefineGMR2 defineCmd;
    } definePacket;

    definePacket.cmd = SVGA_CMD_DEFINE_GMR2;
    definePacket.defineCmd.gmrId = gmrId;
    definePacket.defineCmd.numPages = numPages;

    if (!writeFifo(&definePacket, sizeof(definePacket))) {
        return false;
    }

    /* Remap with single PPN flag */
#pragma pack(push, 1)
    struct {
        uint32_t cmd;
        SVGAFifoCmdRemapGMR2 remapCmd;
        uint64_t ppn;
    } remapPacket;
#pragma pack(pop)

    remapPacket.cmd = SVGA_CMD_REMAP_GMR2;
    remapPacket.remapCmd.gmrId = gmrId;
    remapPacket.remapCmd.flags = static_cast<SVGARemapGMR2Flags>(SVGA_REMAP_GMR2_PPN64 | SVGA_REMAP_GMR2_SINGLE_PPN);
    remapPacket.remapCmd.offsetPages = 0;
    remapPacket.remapCmd.numPages = numPages;
    remapPacket.ppn = gpa >> 12;

    if (!writeFifo(&remapPacket, sizeof(remapPacket))) {
        return false;
    }

    return true;
}

bool GuestVmsvgaDriver::writeFifo(const void *data, size_t sizeBytes)
{
    if (sizeBytes == 0) return true;

    /* Align size to 4-byte multiple */
    size_t alignedSize = (sizeBytes + 3) & ~3;

    uint32_t min = m_fifo[SVGA_FIFO_MIN];
    uint32_t max = m_fifo[SVGA_FIFO_MAX];
    uint32_t next = m_fifo[SVGA_FIFO_NEXT_CMD];
    uint8_t *fifoBytes = reinterpret_cast<uint8_t*>(m_fifo);

    uint32_t stop = m_fifo[SVGA_FIFO_STOP];
    size_t occupied = next >= stop ? next - stop : max - stop + next - min;
    if (alignedSize > max - min - occupied - 4) return false;
    const uint8_t *source = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < alignedSize; ++i) {
        fifoBytes[next++] = i < sizeBytes ? source[i] : 0;
        if (next == max) { next = min; ++m_wraparounds; }
    }

    m_fifo[SVGA_FIFO_NEXT_CMD] = next;
    return true;
}

bool GuestVmsvgaDriver::sync(uint32_t fenceId, uint32_t timeoutLoops)
{
    /* Insert SVGA_CMD_FENCE command */
    struct {
        uint32_t cmd;
        uint32_t fence;
    } fencePacket;

    fencePacket.cmd = SVGA_CMD_FENCE;
    fencePacket.fence = fenceId;

    if (!writeFifo(&fencePacket, sizeof(fencePacket))) {
        return false;
    }

    /* Kick FIFO via I/O Port */
    writeReg(SVGA_REG_SYNC, 1);

    /* Spin wait until hardware posts fence */
    for (uint32_t i = 0; i < timeoutLoops; ++i) {
        if (m_fifo[SVGA_FIFO_FENCE] >= fenceId) {
            return true;
        }
        std::this_thread::yield();
    }

    return false;
}

uint32_t GuestVmsvgaDriver::readFbPixel(uint32_t x, uint32_t y) const
{
    if (x >= m_width || y >= m_height || !m_fb) return 0;
    uint32_t pitchDwords = m_pitch / 4;
    return m_fb[y * pitchDwords + x];
}

} /* namespace qemu_vmsvga */
