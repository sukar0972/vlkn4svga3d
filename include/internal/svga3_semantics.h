#ifndef SVGA3_SEMANTICS_H
#define SVGA3_SEMANTICS_H
#include <cstdint>
namespace svga3_vlkn {
// D3D declaration usages: reserve distinct slots instead of declaration-order aliases.
inline uint32_t vertexSemanticLocation(uint32_t usage,uint32_t index) {
    if (usage==0 || usage==9) return index==0 ? 0 : UINT32_MAX;
    if (usage==5) return index<8 ? 2+index : UINT32_MAX;
    if (usage==10) return index==0 ? 1 : (index<4 ? 9+index : UINT32_MAX);
    if (index) return UINT32_MAX;
    switch (usage) {
    case 3: return 13; // NORMAL
    case 1: return 14; // BLENDWEIGHT
    case 2: return 15; // BLENDINDICES
    case 4: return 16; // PSIZE
    case 6: return 17; // TANGENT
    case 7: return 18; // BINORMAL
    case 8: return 19; // TESSFACTOR
    case 11: return 20; // FOG
    case 12: return 21; // DEPTH
    case 13: return 22; // SAMPLE
    default: return UINT32_MAX;
    }
}
// The implemented interpolator interface supports colors and eight texture coordinates.
inline uint32_t varyingSemanticLocation(uint32_t usage,uint32_t index) {
    if (usage==10 && index<4) return index<2 ? index : 8+index;
    if (usage==5 && index<8) return 2+index;
    return UINT32_MAX;
}
}
#endif
