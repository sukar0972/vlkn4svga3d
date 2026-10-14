/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef QEMU_VLKN_SVGA_H
#define QEMU_VLKN_SVGA_H
/* Keep VMware's protocol names out of vmware_vga.c's legacy enum namespace. */
typedef struct VlknSvga VlknSvga;
VlknSvga *vlkn_svga_new(PCIDevice *pci, QemuConsole *console,
                       MemoryRegion *vram, Error **errp);
void vlkn_svga_free(VlknSvga *renderer);
void vlkn_svga_submit(VlknSvga *renderer);
void vlkn_svga_reset(VlknSvga *renderer);
void vlkn_svga_fifo_init(VlknSvga *renderer, uint32_t *fifo);
uint32_t vlkn_svga_reg(VlknSvga *renderer, uint32_t reg);
/* Returns consumed bytes, 0 for legacy commands, -1 for incomplete/error. */
int vlkn_svga_dispatch(VlknSvga *renderer, uint8_t *fifo, uint32_t min,
                       uint32_t max, uint32_t next, uint32_t stop);
#endif
