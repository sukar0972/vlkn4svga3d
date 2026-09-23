#ifndef ___SHIM_D3D9CAPS_H___
#define ___SHIM_D3D9CAPS_H___

#include "d3d9types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define D3DVS_VERSION(major, minor) (0xFFFE0000 | ((major) << 8) | (minor))
#define D3DPS_VERSION(major, minor) (0xFFFF0000 | ((major) << 8) | (minor))
#define D3DSHADER_VERSION_MAJOR(version) (((version) >> 8) & 0xFF)
#define D3DSHADER_VERSION_MINOR(version) (((version)) & 0xFF)

/* DevCaps2 */
#define D3DDEVCAPS2_STREAMOFFSET                        0x00000001L
#define D3DDEVCAPS2_DMAPNPATCH                          0x00000002L
#define D3DDEVCAPS2_ADAPTIVETESSRTPATCH                 0x00000004L
#define D3DDEVCAPS2_ADAPTIVETESSNPATCH                  0x00000008L
#define D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES       0x00000010L
#define D3DDEVCAPS2_PRESAMPLEDDMAPNPATCH                0x00000020L
#define D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET  0x00000040L

/* Caps2 */
#define D3DCAPS2_FULLSCREENGAMMA        0x00020000L
#define D3DCAPS2_CANCALIBRATEGAMMA      0x00100000L
#define D3DCAPS2_CANMANAGERESOURCE      0x10000000L
#define D3DCAPS2_DYNAMICTEXTURES        0x20000000L
#define D3DCAPS2_CANAUTOGENMIPMAP       0x40000000L
#define D3DCAPS2_CANSHARERESOURCE       0x80000000L

/* Caps3 */
#define D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD   0x00000020L
#define D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION        0x00000080L
#define D3DCAPS3_COPY_TO_VIDMEM                     0x00000100L
#define D3DCAPS3_COPY_TO_SYSTEMMEM                  0x00000200L
#define D3DCAPS3_DXVAHD                             0x00000400L

/* DevCaps */
#define D3DDEVCAPS_EXECUTESYSTEMMEMORY      0x00000010L
#define D3DDEVCAPS_EXECUTEVIDEOMEMORY       0x00000020L
#define D3DDEVCAPS_TLVERTEXSYSTEMMEMORY     0x00000040L
#define D3DDEVCAPS_TLVERTEXVIDEOMEMORY      0x00000080L
#define D3DDEVCAPS_TEXTURESYSTEMMEMORY      0x00000100L
#define D3DDEVCAPS_TEXTUREVIDEOMEMORY       0x00000200L
#define D3DDEVCAPS_DRAWPRIMTLVERTEX         0x00000400L
#define D3DDEVCAPS_CANRENDERAFTERFLIP       0x00000800L
#define D3DDEVCAPS_TEXTURENONLOCALVIDMEM    0x00001000L
#define D3DDEVCAPS_DRAWPRIMITIVES2          0x00002000L
#define D3DDEVCAPS_SEPARATETEXTUREMEMORIES  0x00004000L
#define D3DDEVCAPS_DRAWPRIMITIVES2EX        0x00008000L
#define D3DDEVCAPS_HWTRANSFORMANDLIGHT      0x00010000L
#define D3DDEVCAPS_CANBLTSYSTONONLOCAL      0x00020000L
#define D3DDEVCAPS_HWRASTERIZATION          0x00080000L
#define D3DDEVCAPS_PUREDEVICE               0x00100000L
#define D3DDEVCAPS_QUINTICRTPATCHES         0x00200000L
#define D3DDEVCAPS_RTPATCHES                0x00400000L
#define D3DDEVCAPS_RTPATCHHANDLEZERO        0x00800000L
#define D3DDEVCAPS_NPATCHES                 0x01000000L

/* TextureCaps */
#define D3DPTEXTURECAPS_PERSPECTIVE         0x00000001L
#define D3DPTEXTURECAPS_POW2                0x00000002L
#define D3DPTEXTURECAPS_ALPHA               0x00000004L
#define D3DPTEXTURECAPS_SQUAREONLY          0x00000020L
#define D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE 0x00000040L
#define D3DPTEXTURECAPS_ALPHAPALETTE        0x00000080L
#define D3DPTEXTURECAPS_NONPOW2CONDITIONAL  0x00000100L
#define D3DPTEXTURECAPS_PROJECTED           0x00000400L
#define D3DPTEXTURECAPS_CUBEMAP             0x00000800L
#define D3DPTEXTURECAPS_VOLUMEMAP           0x00002000L
#define D3DPTEXTURECAPS_MIPMAP              0x00004000L
#define D3DPTEXTURECAPS_MIPCUBEMAP          0x00010000L
#define D3DPTEXTURECAPS_MIPVOLUMEMAP        0x00020000L
#define D3DPTEXTURECAPS_CUBEMAP_POW2        0x00040000L
#define D3DPTEXTURECAPS_VOLUMEMAP_POW2      0x00080000L
#define D3DPTEXTURECAPS_NOPROJECTEDBUMPENV  0x00200000L

/* TextureFilterCaps */
#define D3DPTFILTERCAPS_MINFPOINT           0x00000100L
#define D3DPTFILTERCAPS_MINFLINEAR          0x00000200L
#define D3DPTFILTERCAPS_MINFANISOTROPIC     0x00000400L
#define D3DPTFILTERCAPS_MINFPYRAMIDALQUAD   0x00000800L
#define D3DPTFILTERCAPS_MINFGAUSSIANQUAD    0x00001000L
#define D3DPTFILTERCAPS_MIPFPOINT           0x00010000L
#define D3DPTFILTERCAPS_MIPFLINEAR          0x00020000L
#define D3DPTFILTERCAPS_MAGFPOINT           0x01000000L
#define D3DPTFILTERCAPS_MAGFLINEAR          0x02000000L
#define D3DPTFILTERCAPS_MAGFANISOTROPIC     0x04000000L
#define D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD   0x08000000L
#define D3DPTFILTERCAPS_MAGFGAUSSIANQUAD    0x10000000L
#define D3DPTFILTERCAPS_CONVOLUTIONMONO     0x00040000L

/* TextureAddressCaps */
#define D3DPTADDRESSCAPS_WRAP               0x00000001L
#define D3DPTADDRESSCAPS_MIRROR             0x00000002L
#define D3DPTADDRESSCAPS_CLAMP              0x00000004L
#define D3DPTADDRESSCAPS_BORDER             0x00000008L
#define D3DPTADDRESSCAPS_INDEPENDENTUV      0x00000010L
#define D3DPTADDRESSCAPS_MIRRORONCE         0x00000020L

/* TextureOpCaps */
#define D3DTEXOPCAPS_DISABLE                    0x00000001L
#define D3DTEXOPCAPS_SELECTARG1                 0x00000002L
#define D3DTEXOPCAPS_SELECTARG2                 0x00000004L
#define D3DTEXOPCAPS_MODULATE                   0x00000008L
#define D3DTEXOPCAPS_MODULATE2X                 0x00000010L
#define D3DTEXOPCAPS_MODULATE4X                 0x00000020L
#define D3DTEXOPCAPS_ADD                        0x00000040L
#define D3DTEXOPCAPS_ADDSIGNED                  0x00000080L
#define D3DTEXOPCAPS_ADDSIGNED2X                0x00000100L
#define D3DTEXOPCAPS_SUBTRACT                   0x00000200L
#define D3DTEXOPCAPS_ADDSMOOTH                  0x00000400L
#define D3DTEXOPCAPS_BLENDDIFFUSEALPHA          0x00000800L
#define D3DTEXOPCAPS_BLENDTEXTUREALPHA          0x00001000L
#define D3DTEXOPCAPS_BLENDFACTORALPHA           0x00002000L
#define D3DTEXOPCAPS_BLENDTEXTUREALPHAPM        0x00004000L
#define D3DTEXOPCAPS_BLENDCURRENTALPHA          0x00008000L
#define D3DTEXOPCAPS_PREMODULATE                0x00010000L
#define D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR     0x00020000L
#define D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA     0x00040000L
#define D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR  0x00080000L
#define D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA  0x00100000L
#define D3DTEXOPCAPS_BUMPENVMAP                 0x00200000L
#define D3DTEXOPCAPS_BUMPENVMAPLUMINANCE        0x00400000L
#define D3DTEXOPCAPS_DOTPRODUCT3                0x00800000L
#define D3DTEXOPCAPS_MULTIPLYADD                0x01000000L
#define D3DTEXOPCAPS_LERP                       0x02000000L

typedef struct _D3DVSHADERCAPS2_0 {
    DWORD Caps;
    INT   DynamicFlowControlDepth;
    INT   NumTemps;
    INT   StaticFlowControlDepth;
} D3DVSHADERCAPS2_0;

typedef struct _D3DPSHADERCAPS2_0 {
    DWORD Caps;
    INT   DynamicFlowControlDepth;
    INT   NumTemps;
    INT   StaticFlowControlDepth;
    INT   NumInstructionSlots;
} D3DPSHADERCAPS2_0;

typedef struct _D3DCAPS9 {
    D3DDEVTYPE          DeviceType;
    UINT                AdapterOrdinal;
    DWORD               Caps;
    DWORD               Caps2;
    DWORD               Caps3;
    DWORD               PresentationIntervals;
    DWORD               CursorCaps;
    DWORD               DevCaps;
    DWORD               PrimitiveMiscCaps;
    DWORD               RasterCaps;
    DWORD               ZCmpCaps;
    DWORD               SrcBlendCaps;
    DWORD               DestBlendCaps;
    DWORD               AlphaCmpCaps;
    DWORD               ShadeCaps;
    DWORD               TextureCaps;
    DWORD               TextureFilterCaps;
    DWORD               CubeTextureFilterCaps;
    DWORD               VolumeTextureFilterCaps;
    DWORD               TextureAddressCaps;
    DWORD               VolumeTextureAddressCaps;
    DWORD               LineCaps;
    DWORD               MaxTextureWidth;
    DWORD               MaxTextureHeight;
    DWORD               MaxVolumeExtent;
    DWORD               MaxTextureRepeat;
    DWORD               MaxTextureAspectRatio;
    DWORD               MaxAnisotropy;
    float               MaxVertexW;
    float               GuardBandLeft;
    float               GuardBandTop;
    float               GuardBandRight;
    float               GuardBandBottom;
    float               ExtentsAdjust;
    DWORD               StencilCaps;
    DWORD               FVFCaps;
    DWORD               TextureOpCaps;
    DWORD               MaxTextureBlendStages;
    DWORD               MaxSimultaneousTextures;
    DWORD               VertexProcessingCaps;
    DWORD               MaxActiveLights;
    DWORD               MaxUserClipPlanes;
    DWORD               MaxVertexBlendMatrices;
    DWORD               MaxVertexBlendMatrixIndex;
    float               MaxPointSize;
    DWORD               MaxPrimitiveCount;
    DWORD               MaxVertexIndex;
    DWORD               MaxStreams;
    DWORD               MaxStreamStride;
    DWORD               VertexShaderVersion;
    DWORD               MaxVertexShaderConst;
    DWORD               PixelShaderVersion;
    float               PixelShader1xMaxValue;
    DWORD               DevCaps2;
    float               MaxNpatchTessellationLevel;
    DWORD               Reserved5;
    UINT                MasterAdapterOrdinal;
    UINT                AdapterOrdinalInGroup;
    UINT                NumberOfAdaptersInGroup;
    DWORD               DeclTypes;
    DWORD               NumSimultaneousRTs;
    DWORD               StretchRectFilterCaps;
    D3DVSHADERCAPS2_0   VS20Caps;
    D3DPSHADERCAPS2_0   PS20Caps;
    DWORD               VertexTextureFilterCaps;
    DWORD               MaxVShaderInstructionsExecuted;
    DWORD               MaxPShaderInstructionsExecuted;
    DWORD               MaxVertexShader30InstructionSlots;
    DWORD               MaxPixelShader30InstructionSlots;
} D3DCAPS9;

#ifdef __cplusplus
}
#endif

#endif /* ___SHIM_D3D9CAPS_H___ */
