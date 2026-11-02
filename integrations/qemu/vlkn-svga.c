/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/pci/pci_device.h"
#include "ui/console.h"
#include "exec/address-spaces.h"
#include "migration/blocker.h"
#include "qemu/error-report.h"
#include "qemu/units.h"
#include "qemu/rcu.h"
#include "vlkn-svga.h"
#undef PCI_VENDOR_ID_VMWARE
#include <svga3_vlkn.h>
#include <vmsvga/svga3d_caps.h>

struct VlknSvga {
    Svga3VlknDevice *device;
    PCIDevice *pci;
    QemuConsole *console;
    MemoryRegion *vram;
    Error *migration_blocker;
};

/* Callbacks run under the BQL. Only ordinary RAM is accessible: routing a
 * guest-supplied GMR through MMIO could reenter this device under core locks.
 * Preflight the complete range before copying, so a bad tail writes nothing. */
static bool guest_transfer(VlknSvga *s, uint64_t gpa, void *data, size_t size, bool write)
{
    if (size && gpa > UINT64_MAX - (size - 1)) return false;
    RCU_READ_LOCK_GUARD();
    for (unsigned pass = 0; pass < 2; ++pass) {
        size_t done = 0;
        while (done < size) {
            hwaddr offset, length = size - done;
            MemoryRegion *mr = address_space_translate(pci_get_address_space(s->pci),
                gpa + done, &offset, &length, write, MEMTXATTRS_UNSPECIFIED);
            if (!length || !memory_region_is_ram(mr) || (write && memory_region_is_rom(mr))) return false;
            if (pass) {
                uint8_t *ram = memory_region_get_ram_ptr(mr);
                if (write) {
                    memcpy(ram + offset, (uint8_t *)data + done, length);
                    memory_region_set_dirty(mr, offset, length);
                } else memcpy((uint8_t *)data + done, ram + offset, length);
            }
            done += length;
        }
    }
    return true;
}

static bool guest_read(void *opaque, uint64_t gpa, void *dst, size_t size)
{
    return guest_transfer(opaque, gpa, dst, size, false);
}

static bool guest_write(void *opaque, uint64_t gpa, const void *src, size_t size)
{
    return guest_transfer(opaque, gpa, (void *)src, size, true);
}

static void display_update(void *opaque, int32_t x, int32_t y,
                           int32_t width, int32_t height)
{
    VlknSvga *s = opaque;
    memory_region_set_dirty(s->vram, 0, memory_region_size(s->vram));
    dpy_gfx_update(s->console, x, y, width, height);
}

VlknSvga *vlkn_svga_new(PCIDevice *pci, QemuConsole *console,
                       MemoryRegion *vram, Error **errp)
{
    VlknSvga *s = g_new0(VlknSvga, 1);
    Svga3VlknConfigV1 config;
    char diagnostic[256];
    svga3_vlkn_config_init(&config);
    config.device.appName = "QEMU SVGA3D";
    config.device.enableValidationLayers = g_strcmp0(g_getenv("SVGA3_VLKN_VALIDATE"), "1") == 0;
    s->pci = pci;
    s->console = console;
    s->vram = vram;
    if (svga3_vlkn_device_create_ex(&config, &s->device, diagnostic,
                                  sizeof(diagnostic)) != SVGA3_VLKN_SUCCESS) {
        error_setg(errp, "SVGA3D initialization failed: %s", diagnostic);
        g_free(s);
        return NULL;
    }
    Svga3HostAdapter adapter = {
        .opaque = s, .guestRamRead = guest_read, .guestRamWrite = guest_write,
        .displayUpdate = display_update,
    };
    svga3_vlkn_device_set_host_adapter(s->device, &adapter);
    error_setg(&s->migration_blocker,
               "SVGA3D Vulkan state cannot be migrated or snapshotted");
    if (migrate_add_blocker(&s->migration_blocker, errp) < 0) {
        svga3_vlkn_device_destroy(s->device);
        g_free(s);
        return NULL;
    }
    return s;
}

void vlkn_svga_free(VlknSvga *s)
{
    if (!s) {
        return;
    }
    migrate_del_blocker(&s->migration_blocker);
    svga3_vlkn_device_destroy(s->device);
    g_free(s);
}

void vlkn_svga_reset(VlknSvga *s)
{
    if (s && svga3_vlkn_device_reset(s->device) != SVGA3_VLKN_SUCCESS) {
        error_report("SVGA3D reset failed; restart QEMU to recreate the Vulkan device");
    }
}

void vlkn_svga_fifo_init(VlknSvga *s, uint32_t *fifo)
{
    /* The capability record occupies FIFO registers, before FIFO_MIN. */
    stl_le_p(&fifo[SVGA_FIFO_CAPABILITIES], SVGA_FIFO_CAP_FENCE);
    stl_le_p(&fifo[SVGA_FIFO_3D_HWVERSION], SVGA3D_HWVERSION_WS8_B1);
    stl_le_p(&fifo[SVGA_FIFO_FENCE], 0);
    uint32_t count = 0;
    for (uint32_t cap = 0; cap < SVGA3D_DEVCAP_MAX; ++cap) {
        uint32_t value = 0;
        if (!svga3_vlkn_query_cap(s->device, cap, &value)) {
            continue;
        }
        if ((cap == SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8 ||
             cap == SVGA3D_DEVCAP_SURFACEFMT_R5G6B5 ||
             cap == SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5) &&
            (value & SVGA3DFORMAT_OP_OFFSCREEN_RENDERTARGET))
            value |= SVGA3DFORMAT_OP_DISPLAYMODE | SVGA3DFORMAT_OP_3DACCELERATION
                | SVGA3DFORMAT_OP_MEMBEROFGROUP_ARGB | SVGA3DFORMAT_OP_CONVERT_TO_ARGB;
        stl_le_p(&fifo[34 + 2 * count], cap);
        stl_le_p(&fifo[35 + 2 * count++], value);
    }
    stl_le_p(&fifo[32], 2 + 2 * count);
    stl_le_p(&fifo[33], SVGA3DCAPS_RECORD_DEVCAPS);
    stl_le_p(&fifo[34 + 2 * count], 0);
}

uint32_t vlkn_svga_reg(VlknSvga *s, uint32_t reg)
{
    switch (reg) {
    case SVGA_REG_CAPABILITIES:
        return SVGA_CAP_3D | SVGA_CAP_EXTENDED_FIFO | SVGA_CAP_GMR2;
    case SVGA_REG_MEM_REGS: return 1024;
    case SVGA_REG_GMR_MAX_IDS: return 1024;
    case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH: return 2048;
    case SVGA_REG_GMRS_MAX_PAGES: return 1048576;
    case SVGA_REG_MEMORY_SIZE: return 256 * MiB;
    default: return 0;
    }
}

int vlkn_svga_dispatch(VlknSvga *s, uint8_t *fifo, uint32_t min,
                       uint32_t max, uint32_t next, uint32_t stop)
{
    uint32_t cmd = ldl_le_p(fifo + stop);
    if (!(cmd >= SVGA_3D_CMD_BASE && cmd < SVGA_3D_CMD_FUTURE_MAX) &&
        cmd != SVGA_CMD_FENCE && cmd != SVGA_CMD_DEFINE_GMR2 &&
        cmd != SVGA_CMD_REMAP_GMR2) {
        return 0;
    }
    uint32_t available = next >= stop ? next - stop : max - stop + next - min;
    g_autofree uint8_t *packet = g_malloc(available);
    uint32_t tail = MIN(available, max - stop);
    memcpy(packet, fifo + stop, tail);
    memcpy(packet + tail, fifo + min, available - tail);
    size_t packet_size = 0;
    if (svga3_vlkn_fifo_packet_size(packet, available, &packet_size) != SVGA3_VLKN_SUCCESS) {
        return -1;
    }
    DisplaySurface *surface = qemu_console_surface(s->console);
    if (!surface) {
        return -1;
    }
    uint32_t width = surface_width(surface), height = surface_height(surface);
    if (svga3_vlkn_device_set_framebuffer(s->device,
            memory_region_get_ram_ptr(s->vram), pci_get_bar_addr(s->pci, 1),
            memory_region_size(s->vram), width, height,
            surface_stride(surface), surface_bits_per_pixel(surface)) != SVGA3_VLKN_SUCCESS) {
        return -1;
    }
    size_t consumed = 0;
    Svga3VlknStatus status = svga3_vlkn_fifo_execute(s->device, packet,
                                                   packet_size, &consumed);
    if (status != SVGA3_VLKN_SUCCESS || consumed != packet_size) {
        error_report_once("SVGA3D command %u failed: %d", cmd, status);
        return -1;
    }
    if (cmd == SVGA_CMD_FENCE) {
        stl_le_p(fifo + SVGA_FIFO_FENCE * 4, ldl_le_p(packet + 4));
    }
    return packet_size;
}

void vlkn_svga_submit(VlknSvga *s) {
    if (s && svga3_vlkn_device_submit(s->device) != SVGA3_VLKN_SUCCESS)
        error_report_once("SVGA3D queue submission failed");
}
