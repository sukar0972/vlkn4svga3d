#ifndef ___SHIM_D3D9TYPES_H___
#define ___SHIM_D3D9TYPES_H___

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef DWORD D3DCOLOR;

#ifndef D3DCOLOR_ARGB
#define D3DCOLOR_ARGB(a,r,g,b) \
    ((D3DCOLOR)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))
#endif
#ifndef D3DCOLOR_RGBA
#define D3DCOLOR_RGBA(r,g,b,a) D3DCOLOR_ARGB(a,r,g,b)
#endif
#ifndef D3DCOLOR_XRGB
#define D3DCOLOR_XRGB(r,g,b)   D3DCOLOR_ARGB(0xff,r,g,b)
#endif

typedef struct _D3DCOLORVALUE {
    float r;
    float g;
    float b;
    float a;
} D3DCOLORVALUE;

typedef struct _D3DRECT {
    LONG x1;
    LONG y1;
    LONG x2;
    LONG y2;
} D3DRECT;

typedef struct _D3DVIEWPORT9 {
    DWORD X;
    DWORD Y;
    DWORD Width;
    DWORD Height;
    float MinZ;
    float MaxZ;
} D3DVIEWPORT9;

typedef struct _D3DLOCKED_RECT {
    INT  Pitch;
    void *pBits;
} D3DLOCKED_RECT;

typedef struct _D3DBOX {
    UINT Left;
    UINT Top;
    UINT Right;
    UINT Bottom;
    UINT Front;
    UINT Back;
} D3DBOX;

typedef struct _D3DLOCKED_BOX {
    INT  RowPitch;
    INT  SlicePitch;
    void *pBits;
} D3DLOCKED_BOX;

/* D3DDEVTYPE */
typedef enum _D3DDEVTYPE {
    D3DDEVTYPE_HAL         = 1,
    D3DDEVTYPE_NULLREF     = 2,
    D3DDEVTYPE_REF         = 3,
    D3DDEVTYPE_SW          = 4,
    D3DDEVTYPE_FORCE_DWORD = 0x7fffffff
} D3DDEVTYPE;

/* D3DFORMAT */
typedef enum _D3DFORMAT {
    D3DFMT_UNKNOWN              =  0,
    D3DFMT_R8G8B8               = 20,
    D3DFMT_A8R8G8B8             = 21,
    D3DFMT_X8R8G8B8             = 22,
    D3DFMT_R5G6B5               = 23,
    D3DFMT_X1R5G5B5             = 24,
    D3DFMT_A1R5G5B5             = 25,
    D3DFMT_A4R4G4B4             = 26,
    D3DFMT_R3G3B2               = 27,
    D3DFMT_A8                   = 28,
    D3DFMT_A8R3G3B2             = 29,
    D3DFMT_X4R4G4B4             = 30,
    D3DFMT_A2B10G10R10          = 31,
    D3DFMT_A8B8G8R8             = 32,
    D3DFMT_X8B8G8R8             = 33,
    D3DFMT_G16R16               = 34,
    D3DFMT_A2R10G10B10          = 35,
    D3DFMT_A16B16G16R16         = 36,
    D3DFMT_A8P8                 = 40,
    D3DFMT_P8                   = 41,
    D3DFMT_L8                   = 50,
    D3DFMT_A8L8                 = 51,
    D3DFMT_A4L4                 = 52,
    D3DFMT_V8U8                 = 60,
    D3DFMT_L6V5U5               = 61,
    D3DFMT_X8L8V8U8             = 62,
    D3DFMT_Q8W8V8U8             = 63,
    D3DFMT_V16U16               = 64,
    D3DFMT_A2W10V10U10          = 67,
    D3DFMT_UYVY                 = MAKEFOURCC('U', 'Y', 'V', 'Y'),
    D3DFMT_R8G8_B8G8            = MAKEFOURCC('R', 'G', 'B', 'G'),
    D3DFMT_YUY2                 = MAKEFOURCC('Y', 'U', 'Y', '2'),
    D3DFMT_G8R8_G8B8            = MAKEFOURCC('G', 'R', 'G', 'B'),
    D3DFMT_DXT1                 = MAKEFOURCC('D', 'X', 'T', '1'),
    D3DFMT_DXT2                 = MAKEFOURCC('D', 'X', 'T', '2'),
    D3DFMT_DXT3                 = MAKEFOURCC('D', 'X', 'T', '3'),
    D3DFMT_DXT4                 = MAKEFOURCC('D', 'X', 'T', '4'),
    D3DFMT_DXT5                 = MAKEFOURCC('D', 'X', 'T', '5'),
    D3DFMT_D16_LOCKABLE         = 70,
    D3DFMT_D32                  = 71,
    D3DFMT_D15S1                = 73,
    D3DFMT_D24S8                = 75,
    D3DFMT_D24X8                = 77,
    D3DFMT_D24X4S4              = 79,
    D3DFMT_D16                  = 80,
    D3DFMT_L16                  = 81,
    D3DFMT_D32F_LOCKABLE        = 82,
    D3DFMT_D24FS8               = 83,
    D3DFMT_D32_LOCKABLE         = 84,
    D3DFMT_S8_LOCKABLE          = 85,
    D3DFMT_VERTEXDATA           = 100,
    D3DFMT_INDEX16              = 101,
    D3DFMT_INDEX32              = 102,
    D3DFMT_Q16W16V16U16         = 110,
    D3DFMT_R16F                 = 111,
    D3DFMT_G16R16F              = 112,
    D3DFMT_A16B16G16R16F        = 113,
    D3DFMT_R32F                 = 114,
    D3DFMT_G32R32F              = 115,
    D3DFMT_A32B32G32R32F        = 116,
    D3DFMT_CxV8U8               = 117,
    D3DFMT_A1                   = 118,
    D3DFMT_A2B10G10R10_XR_BIAS  = 119,
    D3DFMT_BINARYBUFFER         = 199,
    D3DFMT_FORCE_DWORD          = 0x7fffffff
} D3DFORMAT;

/* D3DRESOURCETYPE */
typedef enum _D3DRESOURCETYPE {
    D3DRTYPE_SURFACE        = 1,
    D3DRTYPE_VOLUME         = 2,
    D3DRTYPE_TEXTURE        = 3,
    D3DRTYPE_VOLUMETEXTURE  = 4,
    D3DRTYPE_CUBETEXTURE    = 5,
    D3DRTYPE_VERTEXBUFFER   = 6,
    D3DRTYPE_INDEXBUFFER    = 7,
    D3DRTYPE_FORCE_DWORD    = 0x7fffffff
} D3DRESOURCETYPE;

/* D3DPOOL */
typedef enum _D3DPOOL {
    D3DPOOL_DEFAULT         = 0,
    D3DPOOL_MANAGED         = 1,
    D3DPOOL_SYSTEMMEM       = 2,
    D3DPOOL_SCRATCH         = 3,
    D3DPOOL_FORCE_DWORD     = 0x7fffffff
} D3DPOOL;

/* D3DMULTISAMPLE_TYPE */
typedef enum _D3DMULTISAMPLE_TYPE {
    D3DMULTISAMPLE_NONE         = 0,
    D3DMULTISAMPLE_NONMASKABLE   = 1,
    D3DMULTISAMPLE_2_SAMPLES     = 2,
    D3DMULTISAMPLE_3_SAMPLES     = 3,
    D3DMULTISAMPLE_4_SAMPLES     = 4,
    D3DMULTISAMPLE_5_SAMPLES     = 5,
    D3DMULTISAMPLE_6_SAMPLES     = 6,
    D3DMULTISAMPLE_7_SAMPLES     = 7,
    D3DMULTISAMPLE_8_SAMPLES     = 8,
    D3DMULTISAMPLE_9_SAMPLES     = 9,
    D3DMULTISAMPLE_10_SAMPLES    = 10,
    D3DMULTISAMPLE_11_SAMPLES    = 11,
    D3DMULTISAMPLE_12_SAMPLES    = 12,
    D3DMULTISAMPLE_13_SAMPLES    = 13,
    D3DMULTISAMPLE_14_SAMPLES    = 14,
    D3DMULTISAMPLE_15_SAMPLES    = 15,
    D3DMULTISAMPLE_16_SAMPLES    = 16,
    D3DMULTISAMPLE_FORCE_DWORD   = 0x7fffffff
} D3DMULTISAMPLE_TYPE;

/* D3DPRIMITIVETYPE */
typedef enum _D3DPRIMITIVETYPE {
    D3DPT_POINTLIST     = 1,
    D3DPT_LINELIST      = 2,
    D3DPT_LINESTRIP     = 3,
    D3DPT_TRIANGLELIST  = 4,
    D3DPT_TRIANGLESTRIP = 5,
    D3DPT_TRIANGLEFAN   = 6,
    D3DPT_FORCE_DWORD   = 0x7fffffff
} D3DPRIMITIVETYPE;

/* D3DCUBEMAP_FACES */
typedef enum _D3DCUBEMAP_FACES {
    D3DCUBEMAP_FACE_POSITIVE_X  = 0,
    D3DCUBEMAP_FACE_NEGATIVE_X  = 1,
    D3DCUBEMAP_FACE_POSITIVE_Y  = 2,
    D3DCUBEMAP_FACE_NEGATIVE_Y  = 3,
    D3DCUBEMAP_FACE_POSITIVE_Z  = 4,
    D3DCUBEMAP_FACE_NEGATIVE_Z  = 5,
    D3DCUBEMAP_FACE_FORCE_DWORD = 0x7fffffff
} D3DCUBEMAP_FACES;

/* D3DBACKBUFFER_TYPE */
typedef enum _D3DBACKBUFFER_TYPE {
    D3DBACKBUFFER_TYPE_MONO         = 0,
    D3DBACKBUFFER_TYPE_LEFT         = 1,
    D3DBACKBUFFER_TYPE_RIGHT        = 2,
    D3DBACKBUFFER_TYPE_FORCE_DWORD  = 0x7fffffff
} D3DBACKBUFFER_TYPE;

/* D3DSWAPEFFECT */
typedef enum _D3DSWAPEFFECT {
    D3DSWAPEFFECT_DISCARD       = 1,
    D3DSWAPEFFECT_FLIP          = 2,
    D3DSWAPEFFECT_COPY          = 3,
    D3DSWAPEFFECT_OVERLAY       = 4,
    D3DSWAPEFFECT_FLIPEX        = 5,
    D3DSWAPEFFECT_FORCE_DWORD   = 0x7fffffff
} D3DSWAPEFFECT;

/* D3DSHADEMODE */
typedef enum _D3DSHADEMODE {
    D3DSHADE_FLAT       = 1,
    D3DSHADE_GOURAUD    = 2,
    D3DSHADE_PHONG      = 3,
    D3DSHADE_FORCE_DWORD = 0x7fffffff
} D3DSHADEMODE;

/* D3DFILLMODE */
typedef enum _D3DFILLMODE {
    D3DFILL_POINT       = 1,
    D3DFILL_WIREFRAME   = 2,
    D3DFILL_SOLID       = 3,
    D3DFILL_FORCE_DWORD = 0x7fffffff
} D3DFILLMODE;

/* D3DBLEND */
typedef enum _D3DBLEND {
    D3DBLEND_ZERO               =  1,
    D3DBLEND_ONE                =  2,
    D3DBLEND_SRCCOLOR           =  3,
    D3DBLEND_INVSRCCOLOR        =  4,
    D3DBLEND_SRCALPHA           =  5,
    D3DBLEND_INVSRCALPHA        =  6,
    D3DBLEND_DESTALPHA          =  7,
    D3DBLEND_INVDESTALPHA       =  8,
    D3DBLEND_DESTCOLOR          =  9,
    D3DBLEND_INVDESTCOLOR       = 10,
    D3DBLEND_SRCALPHASAT        = 11,
    D3DBLEND_BOTHSRCALPHA       = 12,
    D3DBLEND_BOTHINVSRCALPHA    = 13,
    D3DBLEND_BLENDFACTOR        = 14,
    D3DBLEND_INVBLENDFACTOR     = 15,
    D3DBLEND_SRCCOLOR2          = 16,
    D3DBLEND_INVSRCCOLOR2       = 17,
    D3DBLEND_FORCE_DWORD        = 0x7fffffff
} D3DBLEND;

/* D3DBLENDOP */
typedef enum _D3DBLENDOP {
    D3DBLENDOP_ADD          = 1,
    D3DBLENDOP_SUBTRACT     = 2,
    D3DBLENDOP_REVSUBTRACT  = 3,
    D3DBLENDOP_MIN          = 4,
    D3DBLENDOP_MAX          = 5,
    D3DBLENDOP_FORCE_DWORD  = 0x7fffffff
} D3DBLENDOP;

/* D3DCULL */
typedef enum _D3DCULL {
    D3DCULL_NONE        = 1,
    D3DCULL_CW          = 2,
    D3DCULL_CCW         = 3,
    D3DCULL_FORCE_DWORD = 0x7fffffff
} D3DCULL;

/* D3DCMPFUNC */
typedef enum _D3DCMPFUNC {
    D3DCMP_NEVER        = 1,
    D3DCMP_LESS         = 2,
    D3DCMP_EQUAL        = 3,
    D3DCMP_LESSEQUAL    = 4,
    D3DCMP_GREATER      = 5,
    D3DCMP_NOTEQUAL     = 6,
    D3DCMP_GREATEREQUAL = 7,
    D3DCMP_ALWAYS       = 8,
    D3DCMP_FORCE_DWORD  = 0x7fffffff
} D3DCMPFUNC;

/* D3DSTENCILOP */
typedef enum _D3DSTENCILOP {
    D3DSTENCILOP_KEEP       = 1,
    D3DSTENCILOP_ZERO       = 2,
    D3DSTENCILOP_REPLACE    = 3,
    D3DSTENCILOP_INCRSAT    = 4,
    D3DSTENCILOP_DECRSAT    = 5,
    D3DSTENCILOP_INVERT     = 6,
    D3DSTENCILOP_INCR       = 7,
    D3DSTENCILOP_DECR       = 8,
    D3DSTENCILOP_FORCE_DWORD = 0x7fffffff
} D3DSTENCILOP;

/* D3DFOGMODE */
typedef enum _D3DFOGMODE {
    D3DFOG_NONE         = 0,
    D3DFOG_EXP          = 1,
    D3DFOG_EXP2         = 2,
    D3DFOG_LINEAR       = 3,
    D3DFOG_FORCE_DWORD  = 0x7fffffff
} D3DFOGMODE;

/* D3DMATERIALCOLORSOURCE */
typedef enum _D3DMATERIALCOLORSOURCE {
    D3DMCS_MATERIAL     = 0,
    D3DMCS_COLOR1       = 1,
    D3DMCS_COLOR2       = 2,
    D3DMCS_FORCE_DWORD  = 0x7fffffff
} D3DMATERIALCOLORSOURCE;

/* D3DVERTEXBLENDFLAGS */
typedef enum _D3DVERTEXBLENDFLAGS {
    D3DVBF_DISABLE      = 0,
    D3DVBF_1WEIGHTS     = 1,
    D3DVBF_2WEIGHTS     = 2,
    D3DVBF_3WEIGHTS     = 3,
    D3DVBF_TWEENING     = 255,
    D3DVBF_0WEIGHTS     = 256,
    D3DVBF_FORCE_DWORD  = 0x7fffffff
} D3DVERTEXBLENDFLAGS;

/* D3DRENDERSTATETYPE */
typedef enum _D3DRENDERSTATETYPE {
    D3DRS_ZENABLE                   = 7,
    D3DRS_FILLMODE                  = 8,
    D3DRS_SHADEMODE                 = 9,
    D3DRS_ZWRITEENABLE              = 14,
    D3DRS_ALPHATESTENABLE           = 15,
    D3DRS_LASTPIXEL                 = 16,
    D3DRS_SRCBLEND                  = 19,
    D3DRS_DESTBLEND                 = 20,
    D3DRS_CULLMODE                  = 22,
    D3DRS_ZFUNC                     = 23,
    D3DRS_ALPHAREF                  = 24,
    D3DRS_ALPHAFUNC                 = 25,
    D3DRS_DITHERENABLE              = 26,
    D3DRS_ALPHABLENDENABLE          = 27,
    D3DRS_FOGENABLE                 = 28,
    D3DRS_SPECULARENABLE            = 29,
    D3DRS_FOGCOLOR                  = 34,
    D3DRS_FOGTABLEMODE              = 35,
    D3DRS_FOGSTART                  = 36,
    D3DRS_FOGEND                    = 37,
    D3DRS_FOGDENSITY                = 38,
    D3DRS_RANGEFOGENABLE            = 48,
    D3DRS_STENCILENABLE             = 52,
    D3DRS_STENCILFAIL               = 53,
    D3DRS_STENCILZFAIL              = 54,
    D3DRS_STENCILPASS               = 55,
    D3DRS_STENCILFUNC               = 56,
    D3DRS_STENCILREF                = 57,
    D3DRS_STENCILMASK               = 58,
    D3DRS_STENCILWRITEMASK          = 59,
    D3DRS_TEXTUREFACTOR             = 60,
    D3DRS_WRAP0                     = 128,
    D3DRS_WRAP1                     = 129,
    D3DRS_WRAP2                     = 130,
    D3DRS_WRAP3                     = 131,
    D3DRS_WRAP4                     = 132,
    D3DRS_WRAP5                     = 133,
    D3DRS_WRAP6                     = 134,
    D3DRS_WRAP7                     = 135,
    D3DRS_CLIPPING                  = 136,
    D3DRS_LIGHTING                  = 137,
    D3DRS_AMBIENT                   = 139,
    D3DRS_FOGVERTEXMODE             = 140,
    D3DRS_COLORVERTEX               = 141,
    D3DRS_LOCALVIEWER               = 142,
    D3DRS_NORMALIZENORMALS          = 143,
    D3DRS_DIFFUSEMATERIALSOURCE     = 145,
    D3DRS_SPECULARMATERIALSOURCE    = 146,
    D3DRS_AMBIENTMATERIALSOURCE     = 147,
    D3DRS_EMISSIVEMATERIALSOURCE    = 148,
    D3DRS_VERTEXBLEND               = 151,
    D3DRS_CLIPPLANEENABLE           = 152,
    D3DRS_POINTSIZE                 = 154,
    D3DRS_POINTSIZE_MIN             = 155,
    D3DRS_POINTSPRITEENABLE         = 156,
    D3DRS_POINTSCALEENABLE          = 157,
    D3DRS_POINTSCALE_A              = 158,
    D3DRS_POINTSCALE_B              = 159,
    D3DRS_POINTSCALE_C              = 160,
    D3DRS_MULTISAMPLEANTIALIAS      = 161,
    D3DRS_MULTISAMPLEMASK           = 162,
    D3DRS_PATCHEDGESTYLE            = 163,
    D3DRS_DEBUGMONITORTOKEN         = 165,
    D3DRS_POINTSIZE_MAX             = 166,
    D3DRS_INDEXEDVERTEXBLENDENABLE  = 167,
    D3DRS_COLORWRITEENABLE          = 168,
    D3DRS_TWEENFACTOR               = 170,
    D3DRS_BLENDOP                   = 171,
    D3DRS_POSITIONDEGREE            = 172,
    D3DRS_NORMALDEGREE              = 173,
    D3DRS_SCISSORTESTENABLE         = 174,
    D3DRS_SLOPESCALEDEPTHBIAS       = 175,
    D3DRS_ANTIALIASEDLINEENABLE     = 176,
    D3DRS_MINTESSELLATIONLEVEL      = 178,
    D3DRS_MAXTESSELLATIONLEVEL      = 179,
    D3DRS_ADAPTIVETESS_X            = 180,
    D3DRS_ADAPTIVETESS_Y            = 181,
    D3DRS_ADAPTIVETESS_Z            = 182,
    D3DRS_ADAPTIVETESS_W            = 183,
    D3DRS_ENABLEADAPTIVETESSELLATION = 184,
    D3DRS_TWOSIDEDSTENCILMODE       = 185,
    D3DRS_CCW_STENCILFAIL           = 186,
    D3DRS_CCW_STENCILZFAIL          = 187,
    D3DRS_CCW_STENCILPASS           = 188,
    D3DRS_CCW_STENCILFUNC           = 189,
    D3DRS_COLORWRITEENABLE1         = 190,
    D3DRS_COLORWRITEENABLE2         = 191,
    D3DRS_COLORWRITEENABLE3         = 192,
    D3DRS_BLENDFACTOR               = 193,
    D3DRS_SRGBWRITEENABLE           = 194,
    D3DRS_DEPTHBIAS                 = 195,
    D3DRS_WRAP8                     = 198,
    D3DRS_WRAP9                     = 199,
    D3DRS_WRAP10                    = 200,
    D3DRS_WRAP11                    = 201,
    D3DRS_WRAP12                    = 202,
    D3DRS_WRAP13                    = 203,
    D3DRS_WRAP14                    = 204,
    D3DRS_WRAP15                    = 205,
    D3DRS_SEPARATEALPHABLENDENABLE  = 206,
    D3DRS_SRCBLENDALPHA             = 207,
    D3DRS_DESTBLENDALPHA            = 208,
    D3DRS_BLENDOPALPHA              = 209,
    D3DRS_FORCE_DWORD               = 0x7fffffff
} D3DRENDERSTATETYPE;

/* Z-enable modes */
#define D3DZB_FALSE         0
#define D3DZB_TRUE          1
#define D3DZB_USEW          2

/* Color write enable */
#define D3DCOLORWRITEENABLE_RED     (1L<<0)
#define D3DCOLORWRITEENABLE_GREEN   (1L<<1)
#define D3DCOLORWRITEENABLE_BLUE    (1L<<2)
#define D3DCOLORWRITEENABLE_ALPHA   (1L<<3)

/* D3DTEXTURESTAGESTATETYPE */
typedef enum _D3DTEXTURESTAGESTATETYPE {
    D3DTSS_COLOROP                  = 1,
    D3DTSS_COLORARG1                = 2,
    D3DTSS_COLORARG2                = 3,
    D3DTSS_ALPHAOP                  = 4,
    D3DTSS_ALPHAARG1                = 5,
    D3DTSS_ALPHAARG2                = 6,
    D3DTSS_BUMPENVMAT00             = 7,
    D3DTSS_BUMPENVMAT01             = 8,
    D3DTSS_BUMPENVMAT10             = 9,
    D3DTSS_BUMPENVMAT11             = 10,
    D3DTSS_TEXCOORDINDEX            = 11,
    D3DTSS_BUMPENVLSCALE            = 22,
    D3DTSS_BUMPENVLOFFSET           = 23,
    D3DTSS_TEXTURETRANSFORMFLAGS    = 24,
    D3DTSS_COLORARG0                = 26,
    D3DTSS_ALPHAARG0                = 27,
    D3DTSS_RESULTARG                = 28,
    D3DTSS_CONSTANT                 = 32,
    D3DTSS_FORCE_DWORD              = 0x7fffffff
} D3DTEXTURESTAGESTATETYPE;

/* D3DTEXTUREOP */
typedef enum _D3DTEXTUREOP {
    D3DTOP_DISABLE                  = 1,
    D3DTOP_SELECTARG1               = 2,
    D3DTOP_SELECTARG2               = 3,
    D3DTOP_MODULATE                 = 4,
    D3DTOP_MODULATE2X               = 5,
    D3DTOP_MODULATE4X               = 6,
    D3DTOP_ADD                      = 7,
    D3DTOP_ADDSIGNED                = 8,
    D3DTOP_ADDSIGNED2X              = 9,
    D3DTOP_SUBTRACT                 = 10,
    D3DTOP_ADDSMOOTH                = 11,
    D3DTOP_BLENDDIFFUSEALPHA        = 12,
    D3DTOP_BLENDTEXTUREALPHA        = 13,
    D3DTOP_BLENDFACTORALPHA         = 14,
    D3DTOP_BLENDTEXTUREALPHAPM      = 15,
    D3DTOP_BLENDCURRENTALPHA        = 16,
    D3DTOP_PREMODULATE              = 17,
    D3DTOP_MODULATEALPHA_ADDCOLOR   = 18,
    D3DTOP_MODULATECOLOR_ADDALPHA   = 19,
    D3DTOP_MODULATEINVALPHA_ADDCOLOR = 20,
    D3DTOP_MODULATEINVCOLOR_ADDALPHA = 21,
    D3DTOP_BUMPENVMAP               = 22,
    D3DTOP_BUMPENVMAPLUMINANCE      = 23,
    D3DTOP_DOTPRODUCT3              = 24,
    D3DTOP_MULTIPLYADD              = 25,
    D3DTOP_LERP                     = 26,
    D3DTOP_FORCE_DWORD              = 0x7fffffff
} D3DTEXTUREOP;

/* D3DTA (Texture Argument) flags */
#define D3DTA_SELECTMASK        0x0000000f
#define D3DTA_DIFFUSE           0x00000000
#define D3DTA_CURRENT           0x00000001
#define D3DTA_TEXTURE           0x00000002
#define D3DTA_TFACTOR           0x00000003
#define D3DTA_SPECULAR          0x00000004
#define D3DTA_TEMP              0x00000005
#define D3DTA_CONSTANT          0x00000006
#define D3DTA_COMPLEMENT        0x00000010
#define D3DTA_ALPHAREPLICATE    0x00000020

/* D3DTTFF (Texture Transform Flags) */
#define D3DTTFF_DISABLE         0
#define D3DTTFF_COUNT1          1
#define D3DTTFF_COUNT2          2
#define D3DTTFF_COUNT3          3
#define D3DTTFF_COUNT4          4
#define D3DTTFF_PROJECTED       256

/* D3DSAMPLERSTATETYPE */
typedef enum _D3DSAMPLERSTATETYPE {
    D3DSAMP_ADDRESSU        = 1,
    D3DSAMP_ADDRESSV        = 2,
    D3DSAMP_ADDRESSW        = 3,
    D3DSAMP_BORDERCOLOR     = 4,
    D3DSAMP_MAGFILTER       = 5,
    D3DSAMP_MINFILTER       = 6,
    D3DSAMP_MIPFILTER       = 7,
    D3DSAMP_MIPMAPLODBIAS   = 8,
    D3DSAMP_MAXMIPLEVEL     = 9,
    D3DSAMP_MAXANISOTROPY   = 10,
    D3DSAMP_SRGBTEXTURE     = 11,
    D3DSAMP_ELEMENTINDEX    = 12,
    D3DSAMP_DMAPOFFSET      = 13,
    D3DSAMP_FORCE_DWORD     = 0x7fffffff
} D3DSAMPLERSTATETYPE;

/* D3DTEXTUREADDRESS */
typedef enum _D3DTEXTUREADDRESS {
    D3DTADDRESS_WRAP        = 1,
    D3DTADDRESS_MIRROR      = 2,
    D3DTADDRESS_CLAMP       = 3,
    D3DTADDRESS_BORDER      = 4,
    D3DTADDRESS_MIRRORONCE  = 5,
    D3DTADDRESS_FORCE_DWORD = 0x7fffffff
} D3DTEXTUREADDRESS;

/* D3DTEXTUREFILTERTYPE */
typedef enum _D3DTEXTUREFILTERTYPE {
    D3DTEXF_NONE            = 0,
    D3DTEXF_POINT           = 1,
    D3DTEXF_LINEAR          = 2,
    D3DTEXF_ANISOTROPIC     = 3,
    D3DTEXF_PYRAMIDALQUAD   = 6,
    D3DTEXF_GAUSSIANQUAD    = 7,
    D3DTEXF_CONVOLUTIONMONO = 8,
    D3DTEXF_FORCE_DWORD     = 0x7fffffff
} D3DTEXTUREFILTERTYPE;

/* D3DDECLTYPE */
typedef enum _D3DDECLTYPE {
    D3DDECLTYPE_FLOAT1      =  0,
    D3DDECLTYPE_FLOAT2      =  1,
    D3DDECLTYPE_FLOAT3      =  2,
    D3DDECLTYPE_FLOAT4      =  3,
    D3DDECLTYPE_D3DCOLOR    =  4,
    D3DDECLTYPE_UBYTE4      =  5,
    D3DDECLTYPE_SHORT2      =  6,
    D3DDECLTYPE_SHORT4      =  7,
    D3DDECLTYPE_UBYTE4N     =  8,
    D3DDECLTYPE_SHORT2N     =  9,
    D3DDECLTYPE_SHORT4N     = 10,
    D3DDECLTYPE_USHORT2N    = 11,
    D3DDECLTYPE_USHORT4N    = 12,
    D3DDECLTYPE_UDEC3       = 13,
    D3DDECLTYPE_DEC3N       = 14,
    D3DDECLTYPE_FLOAT16_2   = 15,
    D3DDECLTYPE_FLOAT16_4   = 16,
    D3DDECLTYPE_UNUSED      = 17
} D3DDECLTYPE;

/* D3DDECLMETHOD */
typedef enum _D3DDECLMETHOD {
    D3DDECLMETHOD_DEFAULT           = 0,
    D3DDECLMETHOD_PARTIALU          = 1,
    D3DDECLMETHOD_PARTIALV          = 2,
    D3DDECLMETHOD_CROSSUV           = 3,
    D3DDECLMETHOD_UV                = 4,
    D3DDECLMETHOD_LOOKUP            = 5,
    D3DDECLMETHOD_LOOKUPPRESAMPLED  = 6
} D3DDECLMETHOD;

/* D3DDECLUSAGE */
typedef enum _D3DDECLUSAGE {
    D3DDECLUSAGE_POSITION       =  0,
    D3DDECLUSAGE_BLENDWEIGHT    =  1,
    D3DDECLUSAGE_BLENDINDICES   =  2,
    D3DDECLUSAGE_NORMAL         =  3,
    D3DDECLUSAGE_PSIZE          =  4,
    D3DDECLUSAGE_TEXCOORD       =  5,
    D3DDECLUSAGE_TANGENT        =  6,
    D3DDECLUSAGE_BINORMAL       =  7,
    D3DDECLUSAGE_TESSFACTOR     =  8,
    D3DDECLUSAGE_POSITIONT      =  9,
    D3DDECLUSAGE_COLOR          = 10,
    D3DDECLUSAGE_FOG            = 11,
    D3DDECLUSAGE_DEPTH          = 12,
    D3DDECLUSAGE_SAMPLE         = 13
} D3DDECLUSAGE;

typedef struct _D3DVERTEXELEMENT9 {
    WORD    Stream;
    WORD    Offset;
    BYTE    Type;
    BYTE    Method;
    BYTE    Usage;
    BYTE    UsageIndex;
} D3DVERTEXELEMENT9;

#define D3DDECL_END() {0xFF,0,D3DDECLTYPE_UNUSED,0,0,0}

/* D3DMATRIX */
typedef struct _D3DMATRIX {
    union {
        struct {
            float        _11, _12, _13, _14;
            float        _21, _22, _23, _24;
            float        _31, _32, _33, _34;
            float        _41, _42, _43, _44;
        };
        float m[4][4];
    };
} D3DMATRIX;

/* D3DTRANSFORMSTATETYPE */
typedef enum _D3DTRANSFORMSTATETYPE {
    D3DTS_VIEW          = 2,
    D3DTS_PROJECTION    = 3,
    D3DTS_TEXTURE0      = 16,
    D3DTS_TEXTURE1      = 17,
    D3DTS_TEXTURE2      = 18,
    D3DTS_TEXTURE3      = 19,
    D3DTS_TEXTURE4      = 20,
    D3DTS_TEXTURE5      = 21,
    D3DTS_TEXTURE6      = 22,
    D3DTS_TEXTURE7      = 23,
    D3DTS_FORCE_DWORD   = 0x7fffffff
} D3DTRANSFORMSTATETYPE;
#define D3DTS_WORLDMATRIX(index) (D3DTRANSFORMSTATETYPE)(index + 256)
#define D3DTS_WORLD              D3DTS_WORLDMATRIX(0)
#define D3DTS_WORLD1             D3DTS_WORLDMATRIX(1)
#define D3DTS_WORLD2             D3DTS_WORLDMATRIX(2)
#define D3DTS_WORLD3             D3DTS_WORLDMATRIX(3)

/* D3DWRAP */
#define D3DWRAP_U                   0x00000001L
#define D3DWRAP_V                   0x00000002L
#define D3DWRAP_W                   0x00000004L
#define D3DWRAPCOORD_0              0x00000001L
#define D3DWRAPCOORD_1              0x00000002L
#define D3DWRAPCOORD_2              0x00000004L
#define D3DWRAPCOORD_3              0x00000008L

/* Sampler constants */
#define D3DDMAPSAMPLER              256

/* D3DMATERIAL9 */
typedef struct _D3DMATERIAL9 {
    D3DCOLORVALUE Diffuse;
    D3DCOLORVALUE Ambient;
    D3DCOLORVALUE Specular;
    D3DCOLORVALUE Emissive;
    float         Power;
} D3DMATERIAL9;

/* D3DLIGHTTYPE */
typedef enum _D3DLIGHTTYPE {
    D3DLIGHT_POINT          = 1,
    D3DLIGHT_SPOT           = 2,
    D3DLIGHT_DIRECTIONAL    = 3,
    D3DLIGHT_FORCE_DWORD    = 0x7fffffff
} D3DLIGHTTYPE;

typedef struct _D3DVECTOR {
    float x;
    float y;
    float z;
} D3DVECTOR;

/* D3DLIGHT9 */
typedef struct _D3DLIGHT9 {
    D3DLIGHTTYPE  Type;
    D3DCOLORVALUE Diffuse;
    D3DCOLORVALUE Specular;
    D3DCOLORVALUE Ambient;
    D3DVECTOR     Position;
    D3DVECTOR     Direction;
    float         Range;
    float         Falloff;
    float         Attenuation0;
    float         Attenuation1;
    float         Attenuation2;
    float         Theta;
    float         Phi;
} D3DLIGHT9;

/* D3DPRESENT_PARAMETERS */
typedef struct _D3DPRESENT_PARAMETERS_ {
    UINT                BackBufferWidth;
    UINT                BackBufferHeight;
    D3DFORMAT           BackBufferFormat;
    UINT                BackBufferCount;
    D3DMULTISAMPLE_TYPE MultiSampleType;
    DWORD               MultiSampleQuality;
    D3DSWAPEFFECT       SwapEffect;
    HWND                hDeviceWindow;
    BOOL                Windowed;
    BOOL                EnableAutoDepthStencil;
    D3DFORMAT           AutoDepthStencilFormat;
    DWORD               Flags;
    UINT                FullScreen_RefreshRateInHz;
    UINT                PresentationInterval;
} D3DPRESENT_PARAMETERS;

/* D3DQUERYTYPE */
typedef enum _D3DQUERYTYPE {
    D3DQUERYTYPE_VCACHE             = 4,
    D3DQUERYTYPE_RESOURCEMANAGER    = 5,
    D3DQUERYTYPE_VERTEXSTATS        = 6,
    D3DQUERYTYPE_EVENT              = 8,
    D3DQUERYTYPE_OCCLUSION          = 9,
    D3DQUERYTYPE_TIMESTAMP          = 10,
    D3DQUERYTYPE_TIMESTAMPDISJOINT  = 11,
    D3DQUERYTYPE_TIMESTAMPFREQ      = 12,
    D3DQUERYTYPE_PIPELINETIMINGS    = 13,
    D3DQUERYTYPE_INTERFACETIMINGS   = 14,
    D3DQUERYTYPE_VERTEXTIMINGS      = 15,
    D3DQUERYTYPE_PIXELTIMINGS       = 16,
    D3DQUERYTYPE_BANDWIDTHTIMINGS   = 17,
    D3DQUERYTYPE_CACHEHITS          = 18
} D3DQUERYTYPE;

#define D3DISSUE_END        (1 << 0)
#define D3DISSUE_BEGIN      (1 << 1)

#define D3DGETDATA_FLUSH    (1 << 0)

/* Common D3D9 Constants */
#define D3D_SDK_VERSION                 32
#define D3DADAPTER_DEFAULT              0

/* Create flags */
#define D3DCREATE_FPU_PRESERVE                  0x00000002L
#define D3DCREATE_MULTITHREADED                 0x00000004L
#define D3DCREATE_PUREDEVICE                    0x00000010L
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING     0x00000020L
#define D3DCREATE_HARDWARE_VERTEXPROCESSING     0x00000040L
#define D3DCREATE_MIXED_VERTEXPROCESSING        0x00000080L
#define D3DCREATE_DISABLE_DRIVER_MANAGEMENT     0x00000100L
#define D3DCREATE_ADAPTERGROUP_DEVICE           0x00000200L

/* Clear flags */
#define D3DCLEAR_TARGET         0x00000001L
#define D3DCLEAR_ZBUFFER        0x00000002L
#define D3DCLEAR_STENCIL        0x00000004L

/* Usage flags */
#define D3DUSAGE_RENDERTARGET                   0x00000001L
#define D3DUSAGE_DEPTHSTENCIL                   0x00000002L
#define D3DUSAGE_WRITEONLY                      0x00000008L
#define D3DUSAGE_DYNAMIC                        0x00000200L
#define D3DUSAGE_AUTOGENMIPMAP                  0x00000400L
#define D3DUSAGE_DMAP                           0x00004000L
#define D3DUSAGE_QUERY_SRGBREAD                 0x00010000L
#define D3DUSAGE_QUERY_FILTER                   0x00020000L
#define D3DUSAGE_QUERY_SRGBWRITE                0x00040000L
#define D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING 0x00080000L
#define D3DUSAGE_QUERY_VERTEXTEXTURE            0x00100000L
#define D3DUSAGE_QUERY_WRAPANDMIP               0x00200000L
#define D3DUSAGE_QUERY_LEGACYBUMPMAP            0x00400000L

/* Lock flags */
#define D3DLOCK_READONLY            0x00000010L
#define D3DLOCK_NOSERIALIZE         0x00000080L
#define D3DLOCK_NO_DIRTY_UPDATE     0x00000200L
#define D3DLOCK_NOSYSLOCK           0x00000800L
#define D3DLOCK_DISCARD             0x00002000L

/* Presentation intervals */
#define D3DPRESENT_INTERVAL_DEFAULT         0x00000000L
#define D3DPRESENT_INTERVAL_ONE             0x00000001L
#define D3DPRESENT_INTERVAL_TWO             0x00000002L
#define D3DPRESENT_INTERVAL_THREE           0x00000004L
#define D3DPRESENT_INTERVAL_FOUR            0x00000008L
#define D3DPRESENT_INTERVAL_IMMEDIATE       0x80000000L

/* Error / Status Codes */
#define _FACD3D  0x876
#define MAKE_D3DHRESULT(code)  ((HRESULT)(0x88760000 | (code)))
#define D3D_OK                  S_OK
#define D3DERR_WRONGVALUEFAILED ((HRESULT)0x88760868)
#define D3DERR_NOTFOUND         MAKE_D3DHRESULT(2150)
#define D3DERR_MOREDATA         MAKE_D3DHRESULT(2151)
#define D3DERR_DEVICELOST       MAKE_D3DHRESULT(2152)
#define D3DERR_DEVICENOTRESET   MAKE_D3DHRESULT(2153)
#define D3DERR_NOTAVAILABLE     MAKE_D3DHRESULT(2154)
#define D3DERR_OUTOFVIDEOMEMORY MAKE_D3DHRESULT(380)
#define D3DERR_INVALIDCALL      MAKE_D3DHRESULT(2156)
#define D3DERR_DRIVERINTERNALERROR MAKE_D3DHRESULT(2157)
#define S_PRESENT_OCCLUDED      MAKE_D3DHRESULT(2167)
#define S_PRESENT_MODE_CHANGED  MAKE_D3DHRESULT(2168)

#ifdef __cplusplus
}
#endif

#endif /* ___SHIM_D3D9TYPES_H___ */
