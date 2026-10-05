#ifndef SVGA3_FIFO_FRAMING_H
#define SVGA3_FIFO_FRAMING_H

#include <cstdint>
#include "vmsvga/svga_reg.h"
#include "vmsvga/svga3d_reg.h"

namespace svga3_vlkn {
enum class FifoFrameStatus { Complete, Incomplete, Unsupported };

// Reader returns a dword by packet-relative index, allowing a contiguous
// buffer and a wrapped ring to share exactly the same wire framing.
template <typename Reader>
FifoFrameStatus fifoFrame(uint64_t available, Reader read, uint64_t &words) {
    words = 0;
    if (!available) return FifoFrameStatus::Incomplete;
    const uint32_t cmd = read(0);
    if (cmd >= SVGA_3D_CMD_BASE && cmd < SVGA_3D_CMD_FUTURE_MAX) {
        if (available < 2) return FifoFrameStatus::Incomplete;
        words = 2 + (uint64_t(read(1)) + 3) / 4;
    } else {
        switch (cmd) {
            case SVGA_CMD_UPDATE: words = 1 + sizeof(SVGAFifoCmdUpdate)/4; break;
            case SVGA_CMD_UPDATE_VERBOSE: words = 1 + sizeof(SVGAFifoCmdUpdateVerbose)/4; break;
            case 2: words = 6; break; // Legacy RECT_FILL: colour + rectangle.
            case SVGA_CMD_RECT_COPY: words = 1 + sizeof(SVGAFifoCmdRectCopy)/4; break;
            case SVGA_CMD_FRONT_ROP_FILL: words = 1 + sizeof(SVGAFifoCmdFrontRopFill)/4; break;
            case SVGA_CMD_FENCE: words = 2; break;
            case SVGA_CMD_DESTROY_SCREEN: words = 1 + sizeof(SVGAFifoCmdDestroyScreen)/4; break;
            case SVGA_CMD_DEFINE_GMRFB: words = 1 + sizeof(SVGAFifoCmdDefineGMRFB)/4; break;
            case SVGA_CMD_BLIT_GMRFB_TO_SCREEN: words = 1 + sizeof(SVGAFifoCmdBlitGMRFBToScreen)/4; break;
            case SVGA_CMD_BLIT_SCREEN_TO_GMRFB: words = 1 + sizeof(SVGAFifoCmdBlitScreenToGMRFB)/4; break;
            case SVGA_CMD_ANNOTATION_FILL: words = 1 + sizeof(SVGAFifoCmdAnnotationFill)/4; break;
            case SVGA_CMD_ANNOTATION_COPY: words = 1 + sizeof(SVGAFifoCmdAnnotationCopy)/4; break;
            case SVGA_CMD_DEFINE_GMR2: words = 1 + sizeof(SVGAFifoCmdDefineGMR2)/4; break;
            case SVGA_CMD_ESCAPE:
                if (available < 3) return FifoFrameStatus::Incomplete;
                words = 3 + (uint64_t(read(2)) + 3)/4;
                break;
            case SVGA_CMD_DEFINE_SCREEN:
                if (available < 2) return FifoFrameStatus::Incomplete;
                if (read(1) < sizeof(SVGAScreenObject) || (read(1) & 3))
                    return FifoFrameStatus::Unsupported;
                words = 1 + uint64_t(read(1))/4;
                break;
            case SVGA_CMD_DEFINE_CURSOR:
                if (available < 8) return FifoFrameStatus::Incomplete;
                if (read(4)>256 || read(5)>256 || !read(6) || read(6)>32 || !read(7) || read(7)>32)
                    return FifoFrameStatus::Unsupported;
                words = 8 + ((uint64_t(read(4))*read(6)+31)/32)*read(5)
                          + ((uint64_t(read(4))*read(7)+31)/32)*read(5);
                break;
            case SVGA_CMD_DEFINE_ALPHA_CURSOR:
                if (available < 6) return FifoFrameStatus::Incomplete;
                if (read(4)>256 || read(5)>256) return FifoFrameStatus::Unsupported;
                words = 6 + uint64_t(read(4))*read(5);
                break;
            case SVGA_CMD_REMAP_GMR2: {
                if (available < 5) return FifoFrameStatus::Incomplete;
                uint32_t flags = read(2);
                uint64_t descBytes = flags & SVGA_REMAP_GMR2_VIA_GMR ? sizeof(SVGAGuestPtr) :
                    (flags & SVGA_REMAP_GMR2_SINGLE_PPN ? 1 : uint64_t(read(4))) *
                    (flags & SVGA_REMAP_GMR2_PPN64 ? 8 : 4);
                words = 5 + (descBytes+3)/4;
                break;
            }
            default: return FifoFrameStatus::Unsupported;
        }
    }
    return words <= available ? FifoFrameStatus::Complete : FifoFrameStatus::Incomplete;
}
}
#endif
