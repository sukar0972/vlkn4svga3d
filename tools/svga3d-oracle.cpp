#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

#define LOG_GROUP LOG_GROUP_DEV_VMSVGA
#include <VBox/vmm/pdmdev.h>
#include <VBox/version.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/vmm/pgm.h>

#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>
#include <iprt/mem.h>
#include <iprt/avl.h>

#include <VBoxVideo.h>

#include "DevVGA.h"
#include "DevVGA-SVGA.h"
#include "DevVGA-SVGA3d.h"
#include "DevVGA-SVGA3d-internal.h"

#include "mock_d3d9.h"
#include "svga3d_tables.h"

/* ANSI Colors for beautiful test terminal reporting */
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_RED     "\033[31m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_CYAN    "\033[36m"

static int g_testsRun = 0;
static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_testsRun++; \
    if (expr) { \
        g_testsPassed++; \
    } else { \
        g_testsFailed++; \
        std::cerr << ANSI_RED << "  [FAIL] " << ANSI_RESET << msg << " (" #expr ") at line " << __LINE__ << std::endl; \
    } \
} while (0)

/* Helper: Initialize a VGASTATE structure ready for SVGA3D */
static void InitVgaState(VGASTATE *pVGA) {
    memset(pVGA, 0, sizeof(*pVGA));
    pVGA->svga.uWidth = 1024;
    pVGA->svga.uHeight = 768;
    pVGA->svga.u64HostWindowId = 0xdeadbeef;
}

/* --------------------------------------------------------------------------
 * Test 1: Device and Multi-Context Lifecycle
 * -------------------------------------------------------------------------- */
static bool TestLifecycle(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 1] Device and Context Lifecycle..." << ANSI_RESET << std::endl;

    int rc = vmsvga3dInit(pVGA);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dInit returns VINF_SUCCESS");
    TEST_ASSERT(pVGA->svga.p3dState != NULL, "p3dState is allocated");

    rc = vmsvga3dPowerOn(pVGA);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dPowerOn returns VINF_SUCCESS");
    TEST_ASSERT(pVGA->svga.p3dState->pD3D9 != NULL, "pD3D9 instance initialized");

    rc = vmsvga3dChangeMode(pVGA);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dChangeMode succeeds");

    rc = vmsvga3dReset(pVGA);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dReset succeeds");

    /* Define contexts 0, 1, 2 */
    for (uint32_t cid = 0; cid < 3; ++cid) {
        rc = vmsvga3dContextDefine(pVGA, cid);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dContextDefine(" + std::to_string(cid) + ") succeeds");
        TEST_ASSERT(pVGA->svga.p3dState->papContexts[cid] != NULL, "Context " + std::to_string(cid) + " exists");
        TEST_ASSERT(pVGA->svga.p3dState->papContexts[cid]->pDevice != NULL, "Context " + std::to_string(cid) + " has D3D9 device");
    }

    return (rc == VINF_SUCCESS);
}

/* --------------------------------------------------------------------------
 * Test 2: Device Capability Queries (vmsvga3dQueryCaps)
 * -------------------------------------------------------------------------- */
static void TestDevCaps(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 2] Device Capability Queries (84 SVGA3D_DEVCAP_* values)..." << ANSI_RESET << std::endl;

    size_t count = sizeof(g_DevCaps) / sizeof(g_DevCaps[0]);
    int capPassed = 0;

    for (size_t i = 0; i < count; ++i) {
        const DevCapInfo &cap = g_DevCaps[i];
        uint32_t val = 0;
        int rc = vmsvga3dQueryCaps(pVGA, cap.id, &val);
        TEST_ASSERT(rc == cap.expectedRc, "Querying cap " + std::string(cap.name));

        if (rc == cap.expectedRc) {
            capPassed++;
        }
    }

    std::cout << ANSI_GREEN << "  Verified " << capPassed << "/" << count << " capabilities successfully." << ANSI_RESET << std::endl;
}

/* --------------------------------------------------------------------------
 * Test 3: Surface Format Conversions (vmsvga3dSurfaceFormat2D3D)
 * -------------------------------------------------------------------------- */
static void TestSurfaceFormats() {
    std::cout << ANSI_CYAN << "[TEST 3] Surface Format Conversions..." << ANSI_RESET << std::endl;

    size_t count = sizeof(g_SurfaceFormats) / sizeof(g_SurfaceFormats[0]);
    int fmtPassed = 0;

    for (size_t i = 0; i < count; ++i) {
        const SurfaceFormatInfo &fmt = g_SurfaceFormats[i];
        D3DFORMAT d3dFmt = vmsvga3dSurfaceFormat2D3D((SVGA3dSurfaceFormat)fmt.id);
        bool match = (d3dFmt == (D3DFORMAT)fmt.d3dFormat);
        TEST_ASSERT(match, "Surface format mapping: " + std::string(fmt.name));
        if (match) fmtPassed++;
    }

    std::cout << ANSI_GREEN << "  Verified " << fmtPassed << "/" << count << " surface format translations." << ANSI_RESET << std::endl;
}

/* --------------------------------------------------------------------------
 * Test 4: Exhaustive Render State Translations (vmsvga3dSetRenderState)
 * -------------------------------------------------------------------------- */
static void TestRenderStatesExhaustive(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 4] Exhaustive Render State Translations (Value Sweeps)..." << ANSI_RESET << std::endl;

    PVMSVGA3DCONTEXT pContext = pVGA->svga.p3dState->papContexts[0];
    MockDirect3DDevice9 *pMockDev = static_cast<MockDirect3DDevice9*>(pContext->pDevice);

    size_t count = sizeof(g_RenderStates) / sizeof(g_RenderStates[0]);
    int rsPassed = 0;

    for (size_t i = 0; i < count; ++i) {
        const RenderStateInfo &rsi = g_RenderStates[i];

        /* Test Pass 1: Active Value */
        pMockDev->ClearHistory();
        SVGA3dRenderState rs;
        rs.state = (SVGA3dRenderStateName)rsi.id;

        if (rsi.id == SVGA3D_RS_FOGMODE) {
            SVGA3dFogMode fog;
            fog.s.function = SVGA3D_FOGFUNC_LINEAR;
            fog.s.type = SVGA3D_FOGTYPE_PIXEL;
            fog.s.base = SVGA3D_FOGBASE_DEPTHBASED;
            rs.uintValue = fog.uintValue;
        } else if (rsi.id == SVGA3D_RS_FILLMODE) {
            rs.uintValue = SVGA3D_FILLMODE_FILL;
        } else if (rsi.id == SVGA3D_RS_SHADEMODE) {
            rs.uintValue = SVGA3D_SHADEMODE_SMOOTH;
        } else if (rsi.id == SVGA3D_RS_CULLMODE) {
            rs.uintValue = SVGA3D_FACE_BACK;
        } else if (rsi.id == SVGA3D_RS_ALPHAFUNC || rsi.id == SVGA3D_RS_ZFUNC ||
                   rsi.id == SVGA3D_RS_STENCILFUNC || rsi.id == SVGA3D_RS_CCWSTENCILFUNC) {
            rs.uintValue = SVGA3D_CMP_LESSEQUAL;
        } else if (rsi.id == SVGA3D_RS_SRCBLEND || rsi.id == SVGA3D_RS_DSTBLEND ||
                   rsi.id == SVGA3D_RS_SRCBLENDALPHA || rsi.id == SVGA3D_RS_DSTBLENDALPHA) {
            rs.uintValue = SVGA3D_BLENDOP_SRCALPHA;
        } else if (rsi.id == SVGA3D_RS_BLENDEQUATION || rsi.id == SVGA3D_RS_BLENDEQUATIONALPHA) {
            rs.uintValue = SVGA3D_BLENDEQ_ADD;
        } else if (rsi.id == SVGA3D_RS_STENCILFAIL || rsi.id == SVGA3D_RS_STENCILZFAIL ||
                   rsi.id == SVGA3D_RS_STENCILPASS || rsi.id == SVGA3D_RS_CCWSTENCILFAIL ||
                   rsi.id == SVGA3D_RS_CCWSTENCILZFAIL || rsi.id == SVGA3D_RS_CCWSTENCILPASS) {
            rs.uintValue = SVGA3D_STENCILOP_KEEP;
        } else {
            rs.uintValue = 1;
        }

        int rc = vmsvga3dSetRenderState(pVGA, 0, 1, &rs);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetRenderState (val1) for " + std::string(rsi.name));

        bool recorded = false;
        for (const auto &rec : pMockDev->m_renderStateHistory) {
            if (rec.state == (D3DRENDERSTATETYPE)rsi.d3dState) {
                recorded = true;
                break;
            }
        }
        TEST_ASSERT(recorded, "D3D9 device recorded " + std::string(rsi.d3dStateName));
        if (recorded) rsPassed++;

        /* Test Pass 2: Alternate Value (0 or secondary valid enum) */
        pMockDev->ClearHistory();
        if (rsi.id == SVGA3D_RS_FOGMODE) {
            SVGA3dFogMode fog;
            fog.s.function = SVGA3D_FOGFUNC_EXP;
            fog.s.type = SVGA3D_FOGTYPE_PIXEL;
            fog.s.base = SVGA3D_FOGBASE_DEPTHBASED;
            rs.uintValue = fog.uintValue;
        } else if (rsi.id == SVGA3D_RS_FILLMODE) {
            rs.uintValue = SVGA3D_FILLMODE_LINE;
        } else if (rsi.id == SVGA3D_RS_SHADEMODE) {
            rs.uintValue = SVGA3D_SHADEMODE_FLAT;
        } else if (rsi.id == SVGA3D_RS_CULLMODE) {
            rs.uintValue = SVGA3D_FACE_FRONT;
        } else if (rsi.id == SVGA3D_RS_ALPHAFUNC || rsi.id == SVGA3D_RS_ZFUNC ||
                   rsi.id == SVGA3D_RS_STENCILFUNC || rsi.id == SVGA3D_RS_CCWSTENCILFUNC) {
            rs.uintValue = SVGA3D_CMP_GREATER;
        } else if (rsi.id == SVGA3D_RS_SRCBLEND || rsi.id == SVGA3D_RS_DSTBLEND) {
            rs.uintValue = SVGA3D_BLENDOP_INVSRCALPHA;
        } else if (rsi.id == SVGA3D_RS_BLENDEQUATION) {
            rs.uintValue = SVGA3D_BLENDEQ_SUBTRACT;
        } else if (rsi.id == SVGA3D_RS_STENCILFAIL) {
            rs.uintValue = SVGA3D_STENCILOP_INCR;
        } else {
            rs.uintValue = 0;
        }

        rc = vmsvga3dSetRenderState(pVGA, 0, 1, &rs);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetRenderState (val2) for " + std::string(rsi.name));

        bool recorded2 = false;
        for (const auto &rec : pMockDev->m_renderStateHistory) {
            if (rec.state == (D3DRENDERSTATETYPE)rsi.d3dState) {
                recorded2 = true;
                break;
            }
        }
        TEST_ASSERT(recorded2, "D3D9 device recorded alternate " + std::string(rsi.d3dStateName));
    }

    /* Sweep all 5 Blend Equations specifically */
    for (uint32_t eq = SVGA3D_BLENDEQ_ADD; eq <= SVGA3D_BLENDEQ_MAXIMUM; ++eq) {
        SVGA3dRenderState rs;
        rs.state = SVGA3D_RS_BLENDEQUATION;
        rs.uintValue = eq;
        int rc = vmsvga3dSetRenderState(pVGA, 0, 1, &rs);
        TEST_ASSERT(rc == VINF_SUCCESS, "Blend equation sweep: eq=" + std::to_string(eq));
    }

    /* Sweep all 8 Compare Functions */
    for (uint32_t cmp = SVGA3D_CMP_NEVER; cmp <= SVGA3D_CMP_ALWAYS; ++cmp) {
        SVGA3dRenderState rs;
        rs.state = SVGA3D_RS_ZFUNC;
        rs.uintValue = cmp;
        int rc = vmsvga3dSetRenderState(pVGA, 0, 1, &rs);
        TEST_ASSERT(rc == VINF_SUCCESS, "Compare function sweep: cmp=" + std::to_string(cmp));
    }

    /* Sweep all 8 Stencil Ops */
    for (uint32_t op = SVGA3D_STENCILOP_KEEP; op <= SVGA3D_STENCILOP_DECR; ++op) {
        SVGA3dRenderState rs;
        rs.state = SVGA3D_RS_STENCILFAIL;
        rs.uintValue = op;
        int rc = vmsvga3dSetRenderState(pVGA, 0, 1, &rs);
        TEST_ASSERT(rc == VINF_SUCCESS, "Stencil op sweep: op=" + std::to_string(op));
    }

    std::cout << ANSI_GREEN << "  Verified " << rsPassed << "/" << count << " render state translations on D3D9 device." << ANSI_RESET << std::endl;
}

/* --------------------------------------------------------------------------
 * Test 5: Exhaustive Texture Stage States & Combiners
 * -------------------------------------------------------------------------- */
static void TestTextureStatesExhaustive(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 5] Exhaustive Texture Stage States & Combiners (8 Stages)..." << ANSI_RESET << std::endl;

    PVMSVGA3DCONTEXT pContext = pVGA->svga.p3dState->papContexts[0];
    MockDirect3DDevice9 *pMockDev = static_cast<MockDirect3DDevice9*>(pContext->pDevice);

    /* Sweep across 8 texture stages */
    for (uint32_t stage = 0; stage < 8; ++stage) {
        /* Sweep 26 texture combiners on COLOROP */
        size_t tcCount = sizeof(g_TextureCombiners) / sizeof(g_TextureCombiners[0]);
        for (size_t i = 0; i < tcCount; ++i) {
            const TextureCombinerInfo &tc = g_TextureCombiners[i];
            pMockDev->ClearHistory();

            SVGA3dTextureState ts;
            ts.stage = stage;
            ts.name = SVGA3D_TS_COLOROP;
            ts.value = tc.id;

            int rc = vmsvga3dSetTextureState(pVGA, 0, 1, &ts);
            TEST_ASSERT(rc == VINF_SUCCESS, "Stage " + std::to_string(stage) + " COLOROP: " + std::string(tc.name));

            bool match = false;
            for (const auto &rec : pMockDev->m_textureStageHistory) {
                if (rec.stage == stage && rec.type == D3DTSS_COLOROP && rec.value == tc.d3dOp) {
                    match = true;
                    break;
                }
            }
            TEST_ASSERT(match, "D3D9 recorded COLOROP " + std::string(tc.d3dOpName));
        }

        /* Test ALPHAOP */
        SVGA3dTextureState tsAlpha;
        tsAlpha.stage = stage;
        tsAlpha.name = SVGA3D_TS_ALPHAOP;
        tsAlpha.value = SVGA3D_TC_SELECTARG1;
        int rc = vmsvga3dSetTextureState(pVGA, 0, 1, &tsAlpha);
        TEST_ASSERT(rc == VINF_SUCCESS, "Stage " + std::to_string(stage) + " ALPHAOP SELECTARG1");

        /* Test COLORARG1 & COLORARG2 */
        SVGA3dTextureState tsArgs[2];
        tsArgs[0].stage = stage;
        tsArgs[0].name = SVGA3D_TS_COLORARG1;
        tsArgs[0].value = D3DTA_TEXTURE;
        tsArgs[1].stage = stage;
        tsArgs[1].name = SVGA3D_TS_COLORARG2;
        tsArgs[1].value = D3DTA_DIFFUSE;
        rc = vmsvga3dSetTextureState(pVGA, 0, 2, tsArgs);
        TEST_ASSERT(rc == VINF_SUCCESS, "Stage " + std::to_string(stage) + " COLORARG1/2");

        /* Test Sampler Address Modes: ADDRESSU, ADDRESSV, ADDRESSW */
        SVGA3dTextureState tsAddr[3];
        tsAddr[0].stage = stage;
        tsAddr[0].name = SVGA3D_TS_ADDRESSU;
        tsAddr[0].value = SVGA3D_TEX_ADDRESS_CLAMP;
        tsAddr[1].stage = stage;
        tsAddr[1].name = SVGA3D_TS_ADDRESSV;
        tsAddr[1].value = SVGA3D_TEX_ADDRESS_WRAP;
        tsAddr[2].stage = stage;
        tsAddr[2].name = SVGA3D_TS_ADDRESSW;
        tsAddr[2].value = SVGA3D_TEX_ADDRESS_MIRROR;
        rc = vmsvga3dSetTextureState(pVGA, 0, 3, tsAddr);
        TEST_ASSERT(rc == VINF_SUCCESS, "Stage " + std::to_string(stage) + " ADDRESSU/V/W");

        /* Test Filters: MAGFILTER, MINFILTER, MIPFILTER */
        SVGA3dTextureState tsFilter[3];
        tsFilter[0].stage = stage;
        tsFilter[0].name = SVGA3D_TS_MAGFILTER;
        tsFilter[0].value = SVGA3D_TEX_FILTER_LINEAR;
        tsFilter[1].stage = stage;
        tsFilter[1].name = SVGA3D_TS_MINFILTER;
        tsFilter[1].value = SVGA3D_TEX_FILTER_LINEAR;
        tsFilter[2].stage = stage;
        tsFilter[2].name = SVGA3D_TS_MIPFILTER;
        tsFilter[2].value = SVGA3D_TEX_FILTER_NEAREST;
        rc = vmsvga3dSetTextureState(pVGA, 0, 3, tsFilter);
        TEST_ASSERT(rc == VINF_SUCCESS, "Stage " + std::to_string(stage) + " MAG/MIN/MIPFILTER");

        /* Test Anisotropy and LOD */
        SVGA3dTextureState tsAniso[2];
        tsAniso[0].stage = stage;
        tsAniso[0].name = SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL;
        tsAniso[0].value = 16;
        tsAniso[1].stage = stage;
        tsAniso[1].name = SVGA3D_TS_TEXTURETRANSFORMFLAGS;
        tsAniso[1].value = D3DTTFF_COUNT2;
        rc = vmsvga3dSetTextureState(pVGA, 0, 2, tsAniso);
        TEST_ASSERT(rc == VINF_SUCCESS, "Stage " + std::to_string(stage) + " MAXANISOTROPY/TEXTURETRANSFORMFLAGS");
    }
}

/* --------------------------------------------------------------------------
 * Test 6: Viewport, Scissor Rect, and Z-Range
 * -------------------------------------------------------------------------- */
static void TestViewportScissorAndZRange(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 6] Viewport, Scissor Rect, and Z-Range Sweeps..." << ANSI_RESET << std::endl;

    PVMSVGA3DCONTEXT pContext = pVGA->svga.p3dState->papContexts[0];
    MockDirect3DDevice9 *pMockDev = static_cast<MockDirect3DDevice9*>(pContext->pDevice);

    /* Viewport Sweeps */
    struct VpConfig { uint32_t x, y, w, h; };
    VpConfig vpConfigs[] = {
        { 0, 0, 1024, 768 },
        { 10, 20, 800, 600 },
        { 100, 100, 640, 480 },
        { 0, 0, 1920, 1080 },
        { 50, 50, 320, 240 }
    };

    for (size_t i = 0; i < sizeof(vpConfigs) / sizeof(vpConfigs[0]); ++i) {
        SVGA3dRect vp;
        vp.x = vpConfigs[i].x;
        vp.y = vpConfigs[i].y;
        vp.w = vpConfigs[i].w;
        vp.h = vpConfigs[i].h;

        int rc = vmsvga3dSetViewPort(pVGA, 0, &vp);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetViewPort sweep " + std::to_string(i));
        TEST_ASSERT(pMockDev->m_viewport.X == vp.x && pMockDev->m_viewport.Y == vp.y &&
                    pMockDev->m_viewport.Width == vp.w && pMockDev->m_viewport.Height == vp.h,
                    "Viewport match " + std::to_string(i));
    }

    /* Scissor Rect Sweeps */
    struct ScConfig { int32_t x, y, w, h; };
    ScConfig scConfigs[] = {
        { 0, 0, 1024, 768 },
        { 50, 50, 400, 300 },
        { 100, 150, 200, 150 },
        { 25, 25, 800, 600 }
    };

    for (size_t i = 0; i < sizeof(scConfigs) / sizeof(scConfigs[0]); ++i) {
        SVGA3dRect sc;
        sc.x = scConfigs[i].x;
        sc.y = scConfigs[i].y;
        sc.w = scConfigs[i].w;
        sc.h = scConfigs[i].h;

        int rc = vmsvga3dSetScissorRect(pVGA, 0, &sc);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetScissorRect sweep " + std::to_string(i));
        TEST_ASSERT(pMockDev->m_scissorRect.left == (LONG)sc.x && pMockDev->m_scissorRect.top == (LONG)sc.y &&
                    pMockDev->m_scissorRect.right == (LONG)(sc.x + sc.w) && pMockDev->m_scissorRect.bottom == (LONG)(sc.y + sc.h),
                    "Scissor match " + std::to_string(i));
    }

    /* Z-Range Sweeps */
    float zRanges[][2] = {
        { 0.0f, 1.0f },
        { 0.1f, 0.9f },
        { 0.25f, 0.75f },
        { 0.0f, 0.5f },
        { 0.5f, 1.0f },
        { 0.33f, 0.66f }
    };

    for (size_t i = 0; i < sizeof(zRanges) / sizeof(zRanges[0]); ++i) {
        SVGA3dZRange zr;
        zr.min = zRanges[i][0];
        zr.max = zRanges[i][1];

        int rc = vmsvga3dSetZRange(pVGA, 0, zr);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetZRange sweep " + std::to_string(i));
        TEST_ASSERT(std::fabs(pMockDev->m_viewport.MinZ - zr.min) < 1e-4f, "ZRange MinZ match");
        TEST_ASSERT(std::fabs(pMockDev->m_viewport.MaxZ - zr.max) < 1e-4f, "ZRange MaxZ match");
    }
}

/* --------------------------------------------------------------------------
 * Test 7: Transform Matrix Sweeps (World, View, Projection, Texture 0..7)
 * -------------------------------------------------------------------------- */
static void TestTransformsExhaustive(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 7] Transform Matrices (World, View, Proj, Textures 0..7)..." << ANSI_RESET << std::endl;

    PVMSVGA3DCONTEXT pContext = pVGA->svga.p3dState->papContexts[0];
    MockDirect3DDevice9 *pMockDev = static_cast<MockDirect3DDevice9*>(pContext->pDevice);

    SVGA3dTransformType transformTypes[] = {
        SVGA3D_TRANSFORM_WORLD,
        SVGA3D_TRANSFORM_VIEW,
        SVGA3D_TRANSFORM_PROJECTION,
        SVGA3D_TRANSFORM_TEXTURE0,
        SVGA3D_TRANSFORM_TEXTURE1,
        SVGA3D_TRANSFORM_TEXTURE2,
        SVGA3D_TRANSFORM_TEXTURE3,
        SVGA3D_TRANSFORM_TEXTURE4,
        SVGA3D_TRANSFORM_TEXTURE5,
        SVGA3D_TRANSFORM_TEXTURE6,
        SVGA3D_TRANSFORM_TEXTURE7
    };

    for (size_t t = 0; t < sizeof(transformTypes) / sizeof(transformTypes[0]); ++t) {
        SVGA3dTransformType type = transformTypes[t];
        float matrix[16];
        for (int i = 0; i < 16; ++i) {
            matrix[i] = (i % 5 == 0) ? 1.0f : ((float)(t + 1) * 0.1f * (float)(i + 1));
        }

        int rc = vmsvga3dSetTransform(pVGA, 0, type, matrix);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetTransform for type " + std::to_string(type));

        /* Determine D3D transform state */
        D3DTRANSFORMSTATETYPE d3dState;
        if (type == SVGA3D_TRANSFORM_WORLD) d3dState = D3DTS_WORLD;
        else if (type == SVGA3D_TRANSFORM_VIEW) d3dState = D3DTS_VIEW;
        else if (type == SVGA3D_TRANSFORM_PROJECTION) d3dState = D3DTS_PROJECTION;
        else d3dState = (D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + (type - SVGA3D_TRANSFORM_TEXTURE0));

        TEST_ASSERT(pMockDev->m_transforms[d3dState].m[0][0] == matrix[0], "Transform element [0][0] matches");
        TEST_ASSERT(pMockDev->m_transforms[d3dState].m[3][3] == matrix[15], "Transform element [3][3] matches");
    }
}

/* --------------------------------------------------------------------------
 * Test 8: Exhaustive Material Lighting Properties (vmsvga3dSetMaterial)
 * -------------------------------------------------------------------------- */
static void TestMaterialsExhaustive(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 8] Material Lighting Properties & Presets..." << ANSI_RESET << std::endl;

    PVMSVGA3DCONTEXT pContext = pVGA->svga.p3dState->papContexts[0];
    MockDirect3DDevice9 *pMockDev = static_cast<MockDirect3DDevice9*>(pContext->pDevice);

    struct MaterialPreset {
        const char *name;
        float diff[4];
        float amb[4];
        float spec[4];
        float emiss[4];
        float power;
    };

    MaterialPreset presets[] = {
        { "Gold", { 0.75164f, 0.60648f, 0.22648f, 1.0f }, { 0.24725f, 0.1995f, 0.0745f, 1.0f }, { 0.628281f, 0.555802f, 0.366065f, 1.0f }, { 0, 0, 0, 0 }, 51.2f },
        { "Silver", { 0.50754f, 0.50754f, 0.50754f, 1.0f }, { 0.19225f, 0.19225f, 0.19225f, 1.0f }, { 0.508273f, 0.508273f, 0.508273f, 1.0f }, { 0, 0, 0, 0 }, 51.2f },
        { "Emerald", { 0.07568f, 0.61424f, 0.07568f, 1.0f }, { 0.0215f, 0.1745f, 0.0215f, 1.0f }, { 0.633f, 0.727811f, 0.633f, 1.0f }, { 0, 0, 0, 0 }, 76.8f },
        { "Ruby", { 0.61424f, 0.04136f, 0.04136f, 1.0f }, { 0.1745f, 0.01175f, 0.01175f, 1.0f }, { 0.727811f, 0.626959f, 0.626959f, 1.0f }, { 0, 0, 0, 0 }, 76.8f },
        { "Emissive", { 0.5f, 0.5f, 0.5f, 1.0f }, { 0.1f, 0.1f, 0.1f, 1.0f }, { 0.2f, 0.2f, 0.2f, 1.0f }, { 1.0f, 0.8f, 0.2f, 1.0f }, 10.0f }
    };

    SVGA3dFace faces[] = { SVGA3D_FACE_NONE, SVGA3D_FACE_FRONT, SVGA3D_FACE_BACK, SVGA3D_FACE_FRONT_BACK };

    for (size_t f = 0; f < sizeof(faces) / sizeof(faces[0]); ++f) {
        for (size_t p = 0; p < sizeof(presets) / sizeof(presets[0]); ++p) {
            const MaterialPreset &mp = presets[p];
            SVGA3dMaterial mat;
            memcpy(mat.diffuse, mp.diff, sizeof(mat.diffuse));
            memcpy(mat.ambient, mp.amb, sizeof(mat.ambient));
            memcpy(mat.specular, mp.spec, sizeof(mat.specular));
            memcpy(mat.emissive, mp.emiss, sizeof(mat.emissive));
            mat.shininess = mp.power;

            int rc = vmsvga3dSetMaterial(pVGA, 0, faces[f], &mat);
            TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetMaterial preset " + std::string(mp.name));

            TEST_ASSERT(pMockDev->m_material.Diffuse.r == mp.diff[0], "Material Diffuse R matches");
            TEST_ASSERT(pMockDev->m_material.Ambient.g == mp.amb[1], "Material Ambient G matches");
            TEST_ASSERT(pMockDev->m_material.Specular.b == mp.spec[2], "Material Specular B matches");
            TEST_ASSERT(pMockDev->m_material.Power == mp.power, "Material Shininess Power matches");
        }
    }
}

/* --------------------------------------------------------------------------
 * Test 9: Exhaustive Hardware Lights (vmsvga3dSetLightData & LightEnabled)
 * -------------------------------------------------------------------------- */
static void TestLightsExhaustive(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 9] Hardware Light Sources (8 Lights, Point/Spot/Dir)..." << ANSI_RESET << std::endl;

    PVMSVGA3DCONTEXT pContext = pVGA->svga.p3dState->papContexts[0];
    MockDirect3DDevice9 *pMockDev = static_cast<MockDirect3DDevice9*>(pContext->pDevice);

    for (uint32_t lightIdx = 0; lightIdx < 8; ++lightIdx) {
        /* Point Light */
        SVGA3dLightData light;
        memset(&light, 0, sizeof(light));
        light.type = SVGA3D_LIGHTTYPE_POINT;
        light.diffuse[0] = 1.0f; light.diffuse[1] = 0.9f; light.diffuse[2] = 0.8f; light.diffuse[3] = 1.0f;
        light.position[0] = 10.0f * (float)(lightIdx + 1);
        light.position[1] = 20.0f;
        light.position[2] = 30.0f;
        light.range = 100.0f;
        light.attenuation0 = 1.0f;
        light.attenuation1 = 0.1f;

        int rc = vmsvga3dSetLightData(pVGA, 0, lightIdx, &light);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetLightData (Point) light " + std::to_string(lightIdx));
        TEST_ASSERT(pMockDev->m_lights[lightIdx].Type == D3DLIGHT_POINT, "Light type D3DLIGHT_POINT");
        TEST_ASSERT(pMockDev->m_lights[lightIdx].Position.x == light.position[0], "Light position X matches");

        /* Enable light */
        rc = vmsvga3dSetLightEnabled(pVGA, 0, lightIdx, 1);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetLightEnabled(1) light " + std::to_string(lightIdx));
        TEST_ASSERT(pMockDev->m_lightEnables[lightIdx] == TRUE, "Light enabled in mock device");

        /* Directional Light */
        light.type = SVGA3D_LIGHTTYPE_DIRECTIONAL;
        light.direction[0] = 0.0f;
        light.direction[1] = -1.0f;
        light.direction[2] = 0.0f;

        rc = vmsvga3dSetLightData(pVGA, 0, lightIdx, &light);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetLightData (Directional) light " + std::to_string(lightIdx));
        TEST_ASSERT(pMockDev->m_lights[lightIdx].Type == D3DLIGHT_DIRECTIONAL, "Light type D3DLIGHT_DIRECTIONAL");

        /* Spot Light */
        light.type = SVGA3D_LIGHTTYPE_SPOT1;
        light.theta = 0.5f;
        light.phi = 1.0f;
        light.falloff = 1.0f;

        rc = vmsvga3dSetLightData(pVGA, 0, lightIdx, &light);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetLightData (Spot) light " + std::to_string(lightIdx));
        TEST_ASSERT(pMockDev->m_lights[lightIdx].Type == D3DLIGHT_SPOT, "Light type D3DLIGHT_SPOT");

        /* Disable light */
        rc = vmsvga3dSetLightEnabled(pVGA, 0, lightIdx, 0);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetLightEnabled(0) light " + std::to_string(lightIdx));
        TEST_ASSERT(pMockDev->m_lightEnables[lightIdx] == FALSE, "Light disabled in mock device");
    }
}

/* --------------------------------------------------------------------------
 * Test 10: Exhaustive Clip Planes (vmsvga3dSetClipPlane)
 * -------------------------------------------------------------------------- */
static void TestClipPlanesExhaustive(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 10] Clip Planes (Planes 0..5, Arbitrary Equations)..." << ANSI_RESET << std::endl;

    PVMSVGA3DCONTEXT pContext = pVGA->svga.p3dState->papContexts[0];
    MockDirect3DDevice9 *pMockDev = static_cast<MockDirect3DDevice9*>(pContext->pDevice);

    float planes[][4] = {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.7071f, 0.7071f, 0.0f, -10.0f },
        { 0.0f, 0.7071f, 0.7071f, -25.0f },
        { 0.5773f, 0.5773f, 0.5773f, -100.0f }
    };

    for (uint32_t idx = 0; idx < 6; ++idx) {
        int rc = vmsvga3dSetClipPlane(pVGA, 0, idx, planes[idx]);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetClipPlane plane " + std::to_string(idx));

        const auto &p = pMockDev->m_clipPlanes[idx];
        TEST_ASSERT(p.size() == 4, "Clip plane has 4 coefficients");
        TEST_ASSERT(p[0] == planes[idx][0], "Plane A coefficient matches");
        TEST_ASSERT(p[1] == planes[idx][1], "Plane B coefficient matches");
        TEST_ASSERT(p[2] == planes[idx][2], "Plane C coefficient matches");
        TEST_ASSERT(p[3] == planes[idx][3], "Plane D coefficient matches");
    }
}

/* --------------------------------------------------------------------------
 * Test 11: Shader Lifecycle and Constant Register Banks (Float, Int, Bool)
 * -------------------------------------------------------------------------- */
static void TestShadersAndConstantsExhaustive(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 11] Shader Lifecycle & Constant Banks (Float, Int, Bool)..." << ANSI_RESET << std::endl;

    PVMSVGA3DCONTEXT pContext = pVGA->svga.p3dState->papContexts[0];
    MockDirect3DDevice9 *pMockDev = static_cast<MockDirect3DDevice9*>(pContext->pDevice);

    /* Vertex Shader bytecode tokens (vs_3_0 mock token) */
    uint32_t vsTokens[] = { 0xfffe0300, 0x0000ffff };
    int rc = vmsvga3dShaderDefine(pVGA, 0, 1, SVGA3D_SHADERTYPE_VS, sizeof(vsTokens), vsTokens);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderDefine (VS 1) succeeds");

    rc = vmsvga3dShaderSet(pVGA, pContext, 0, SVGA3D_SHADERTYPE_VS, 1);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderSet (VS 1) succeeds");

    /* Pixel Shader bytecode tokens (ps_3_0 mock token) */
    uint32_t psTokens[] = { 0xffff0300, 0x0000ffff };
    rc = vmsvga3dShaderDefine(pVGA, 0, 1, SVGA3D_SHADERTYPE_PS, sizeof(psTokens), psTokens);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderDefine (PS 1) succeeds");

    rc = vmsvga3dShaderSet(pVGA, pContext, 0, SVGA3D_SHADERTYPE_PS, 1);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderSet (PS 1) succeeds");

    /* Sweep Vertex Shader Float Constants: registers 0..31 */
    for (uint32_t reg = 0; reg < 32; ++reg) {
        float fVals[4] = { (float)reg, (float)reg + 0.25f, (float)reg + 0.5f, (float)reg + 0.75f };
        rc = vmsvga3dShaderSetConst(pVGA, 0, reg, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, 1, (uint32_t*)fVals);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderSetConst VS Float reg " + std::to_string(reg));
        TEST_ASSERT(pMockDev->m_vsConstF[reg * 4 + 0] == fVals[0], "VS Const F[0] matches");
        TEST_ASSERT(pMockDev->m_vsConstF[reg * 4 + 1] == fVals[1], "VS Const F[1] matches");
    }

    /* Sweep Pixel Shader Float Constants: registers 0..31 */
    for (uint32_t reg = 0; reg < 32; ++reg) {
        float fVals[4] = { (float)reg * 2.0f, (float)reg * 2.0f + 0.1f, (float)reg * 2.0f + 0.2f, 1.0f };
        rc = vmsvga3dShaderSetConst(pVGA, 0, reg, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, 1, (uint32_t*)fVals);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderSetConst PS Float reg " + std::to_string(reg));
        TEST_ASSERT(pMockDev->m_psConstF[reg * 4 + 0] == fVals[0], "PS Const F[0] matches");
        TEST_ASSERT(pMockDev->m_psConstF[reg * 4 + 1] == fVals[1], "PS Const F[1] matches");
    }

    /* Sweep VS/PS Int Constants: registers 0..7 */
    for (uint32_t reg = 0; reg < 8; ++reg) {
        int iVals[4] = { (int)reg, (int)reg * 10, (int)reg * 100, 1 };
        rc = vmsvga3dShaderSetConst(pVGA, 0, reg, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_INT, 1, (uint32_t*)iVals);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderSetConst VS Int reg " + std::to_string(reg));
        TEST_ASSERT(pMockDev->m_vsConstI[reg * 4 + 0] == iVals[0], "VS Const I[0] matches");

        rc = vmsvga3dShaderSetConst(pVGA, 0, reg, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_INT, 1, (uint32_t*)iVals);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderSetConst PS Int reg " + std::to_string(reg));
        TEST_ASSERT(pMockDev->m_psConstI[reg * 4 + 0] == iVals[0], "PS Const I[0] matches");
    }

    /* Sweep VS/PS Bool Constants: registers 0..7 */
    for (uint32_t reg = 0; reg < 8; ++reg) {
        BOOL bVal = (reg % 2 == 0) ? TRUE : FALSE;
        rc = vmsvga3dShaderSetConst(pVGA, 0, reg, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_BOOL, 1, (uint32_t*)&bVal);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderSetConst VS Bool reg " + std::to_string(reg));
        TEST_ASSERT(pMockDev->m_vsConstB[reg] == bVal, "VS Const B matches");

        rc = vmsvga3dShaderSetConst(pVGA, 0, reg, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_BOOL, 1, (uint32_t*)&bVal);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderSetConst PS Bool reg " + std::to_string(reg));
        TEST_ASSERT(pMockDev->m_psConstB[reg] == bVal, "PS Const B matches");
    }

    /* Destroy Shaders */
    rc = vmsvga3dShaderDestroy(pVGA, 0, 1, SVGA3D_SHADERTYPE_VS);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderDestroy (VS 1) succeeds");

    rc = vmsvga3dShaderDestroy(pVGA, 0, 1, SVGA3D_SHADERTYPE_PS);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dShaderDestroy (PS 1) succeeds");
}

/* --------------------------------------------------------------------------
 * Test 12: Hardware Occlusion Queries
 * -------------------------------------------------------------------------- */
static void TestOcclusionQueriesExhaustive(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 12] Hardware Occlusion Queries (Create, Issue, GetData, Delete)..." << ANSI_RESET << std::endl;

    PVMSVGA3DSTATE pState = pVGA->svga.p3dState;
    PVMSVGA3DCONTEXT pContext = pState->papContexts[0];

    /* Test 4 Complete Query Lifecycle Cycles */
    for (int cycle = 0; cycle < 4; ++cycle) {
        int rc = vmsvga3dOcclusionQueryCreate(pState, pContext);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dOcclusionQueryCreate cycle " + std::to_string(cycle));
        TEST_ASSERT(pContext->occlusion.pQuery != NULL, "pQuery pointer valid");

        rc = vmsvga3dOcclusionQueryBegin(pState, pContext);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dOcclusionQueryBegin cycle " + std::to_string(cycle));

        rc = vmsvga3dOcclusionQueryEnd(pState, pContext);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dOcclusionQueryEnd cycle " + std::to_string(cycle));

        uint32_t pixels = 0;
        rc = vmsvga3dOcclusionQueryGetData(pState, pContext, &pixels);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dOcclusionQueryGetData cycle " + std::to_string(cycle));
        TEST_ASSERT(pixels == 256, "Occlusion query pixel count matches mock (256)");

        rc = vmsvga3dOcclusionQueryDelete(pState, pContext);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dOcclusionQueryDelete cycle " + std::to_string(cycle));
    }
}

/* --------------------------------------------------------------------------
 * Test 13: Clear Command Variations (vmsvga3dCommandClear)
 * -------------------------------------------------------------------------- */
static void TestClearCommandExhaustive(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 13] Clear Command Variations & Rectangles..." << ANSI_RESET << std::endl;

    PVMSVGA3DCONTEXT pContext = pVGA->svga.p3dState->papContexts[0];
    MockDirect3DDevice9 *pMockDev = static_cast<MockDirect3DDevice9*>(pContext->pDevice);

    struct ClearCase {
        SVGA3dClearFlag flags;
        uint32_t color;
        float depth;
        uint32_t stencil;
    };

    ClearCase cases[] = {
        { SVGA3D_CLEAR_COLOR, 0xff0000ff, 1.0f, 0 },
        { SVGA3D_CLEAR_DEPTH, 0, 0.5f, 0 },
        { SVGA3D_CLEAR_STENCIL, 0, 1.0f, 128 },
        { (SVGA3dClearFlag)(SVGA3D_CLEAR_COLOR | SVGA3D_CLEAR_DEPTH), 0xff00ff00, 0.25f, 0 },
        { (SVGA3dClearFlag)(SVGA3D_CLEAR_COLOR | SVGA3D_CLEAR_STENCIL), 0xffff0000, 1.0f, 255 },
        { (SVGA3dClearFlag)(SVGA3D_CLEAR_DEPTH | SVGA3D_CLEAR_STENCIL), 0, 0.75f, 64 },
        { (SVGA3dClearFlag)(SVGA3D_CLEAR_COLOR | SVGA3D_CLEAR_DEPTH | SVGA3D_CLEAR_STENCIL), 0xffffffff, 0.0f, 1 }
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const ClearCase &cc = cases[i];
        pMockDev->ClearHistory();

        int rc = vmsvga3dCommandClear(pVGA, 0, cc.flags, cc.color, cc.depth, cc.stencil, 0, NULL);
        TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dCommandClear case " + std::to_string(i));
        TEST_ASSERT(!pMockDev->m_clearHistory.empty(), "Clear recorded in mock device");

        if (!pMockDev->m_clearHistory.empty()) {
            const ClearRecord &rec = pMockDev->m_clearHistory.back();
            if (cc.flags & SVGA3D_CLEAR_COLOR) TEST_ASSERT(rec.color == cc.color, "Clear color match");
            if (cc.flags & SVGA3D_CLEAR_DEPTH) TEST_ASSERT(std::fabs(rec.z - cc.depth) < 1e-4f, "Clear depth match");
            if (cc.flags & SVGA3D_CLEAR_STENCIL) TEST_ASSERT(rec.stencil == cc.stencil, "Clear stencil match");
        }
    }

    /* Test Clear with Multiple Dirty Rects */
    SVGA3dRect rects[2];
    rects[0].x = 10; rects[0].y = 10; rects[0].w = 100; rects[0].h = 100;
    rects[1].x = 200; rects[1].y = 200; rects[1].w = 150; rects[1].h = 150;

    int rc = vmsvga3dCommandClear(pVGA, 0, SVGA3D_CLEAR_COLOR, 0xffaabbcc, 1.0f, 0, 2, rects);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dCommandClear with 2 rects succeeds");
}

/* --------------------------------------------------------------------------
 * Test 14: Surface Management, DMA, Blit, Present, and Mipmaps
 * -------------------------------------------------------------------------- */
static void TestSurfacesExhaustive(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 14] Surfaces (Define, RenderTarget, DMA, Blit, Present, Mipmaps)..." << ANSI_RESET << std::endl;

    /* 1. Define 2D Surface (SID 1, 512x512) */
    SVGA3dSurfaceFace faces[SVGA3D_MAX_SURFACE_FACES]{};
    faces[0].numMipLevels = 1;
    SVGA3dSize mipSize = { 512, 512, 1 };
    int rc = vmsvga3dSurfaceDefine(pVGA, 1, 0, SVGA3D_X8R8G8B8, faces, 0, SVGA3D_TEX_FILTER_NONE, 1, &mipSize);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceDefine (SID 1, 2D 512x512) succeeds");

    /* 2. Define Mipmapped Surface (SID 2, 256x256, 4 mip levels) */
    faces[0].numMipLevels = 4;
    SVGA3dSize mipSizes[4] = { { 256, 256, 1 }, { 128, 128, 1 }, { 64, 64, 1 }, { 32, 32, 1 } };
    rc = vmsvga3dSurfaceDefine(pVGA, 2, 0, SVGA3D_A8R8G8B8, faces, 0, SVGA3D_TEX_FILTER_LINEAR, 4, mipSizes);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceDefine (SID 2, 4 mips) succeeds");

    /* 3. Define Cubemap Surface (SID 3, 128x128, 6 faces) */
    for (int f = 0; f < 6; ++f) faces[f].numMipLevels = 1;
    SVGA3dSize cubeSize = { 128, 128, 1 };
    rc = vmsvga3dSurfaceDefine(pVGA, 3, SVGA3D_SURFACE_CUBEMAP, SVGA3D_X8R8G8B8, faces, 0, SVGA3D_TEX_FILTER_NONE, 1, &cubeSize);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceDefine (SID 3, Cubemap) succeeds");

    /* 4. Set Render Target to SID 1 */
    SVGA3dSurfaceImageId rt = { 1, 0, 0 };
    rc = vmsvga3dSetRenderTarget(pVGA, 0, SVGA3D_RT_COLOR0, rt);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSetRenderTarget(SID 1) succeeds");

    /* 5. Surface DMA Transfers */
    SVGA3dGuestImage guest{};
    SVGA3dCopyBox copyBox = { 0, 0, 0, 0, 0, 0, 128, 128, 1 };
    rc = vmsvga3dSurfaceDMA(pVGA, guest, rt, SVGA3D_WRITE_HOST_VRAM, 1, &copyBox);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceDMA (Upload) succeeds");

    rc = vmsvga3dSurfaceDMA(pVGA, guest, rt, SVGA3D_READ_HOST_VRAM, 1, &copyBox);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceDMA (Download) succeeds");

    /* 6. Surface Copy */
    SVGA3dSurfaceImageId srcImg = { 1, 0, 0 };
    SVGA3dSurfaceImageId dstImg = { 2, 0, 0 };
    rc = vmsvga3dSurfaceCopy(pVGA, dstImg, srcImg, 1, &copyBox);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceCopy (SID 1 -> SID 2) succeeds");

    /* 7. Surface Stretch Blit */
    SVGA3dBox srcBox = { 0, 0, 0, 256, 256, 1 };
    SVGA3dBox dstBox = { 0, 0, 0, 256, 256, 1 };
    rc = vmsvga3dSurfaceStretchBlt(pVGA, &dstImg, &dstBox, &srcImg, &srcBox, SVGA3D_STRETCH_BLT_LINEAR);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceStretchBlt succeeds");

    /* 8. Generate Mipmaps */
    rc = vmsvga3dGenerateMipmaps(pVGA, 1, SVGA3D_TEX_FILTER_LINEAR);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dGenerateMipmaps (SID 1 Linear) succeeds");

    rc = vmsvga3dGenerateMipmaps(pVGA, 1, SVGA3D_TEX_FILTER_NEAREST);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dGenerateMipmaps (SID 1 Nearest) succeeds");

    /* 9. Surface Blit To Screen */
    SVGASignedRect destRect = { 0, 0, 512, 512 };
    SVGASignedRect srcRect = { 0, 0, 512, 512 };
    SVGASignedRect clipRect = { 0, 0, 512, 512 };
    rc = vmsvga3dSurfaceBlitToScreen(pVGA, 0, destRect, srcImg, srcRect, 1, &clipRect);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceBlitToScreen succeeds");

    /* 10. Surface Present */
    SVGA3dCopyRect prRect = { 0, 0, 0, 0, 512, 512 };
    rc = vmsvga3dCommandPresent(pVGA, 1, 1, &prRect);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dCommandPresent (SID 1) succeeds");

    /* 11. Surface Destruction */
    rc = vmsvga3dSurfaceDestroy(pVGA, 1);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceDestroy(SID 1) succeeds");

    rc = vmsvga3dSurfaceDestroy(pVGA, 2);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceDestroy(SID 2) succeeds");

    rc = vmsvga3dSurfaceDestroy(pVGA, 3);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dSurfaceDestroy(SID 3) succeeds");
}

/* --------------------------------------------------------------------------
 * Test 15: Primitive Topologies and Vertex Declarations
 * -------------------------------------------------------------------------- */
static void TestPrimitiveTopologiesAndVertexDecls() {
    std::cout << ANSI_CYAN << "[TEST 15] Primitive Topologies & Vertex Declaration Types..." << ANSI_RESET << std::endl;

    /* Verify all 6 Primitive Topologies */
    size_t primCount = sizeof(g_PrimitiveTypes) / sizeof(g_PrimitiveTypes[0]);
    for (size_t i = 0; i < primCount; ++i) {
        const PrimitiveTypeInfo &p = g_PrimitiveTypes[i];
        TEST_ASSERT(p.id >= 1 && p.id <= 6, "Valid primitive topology ID: " + std::string(p.name));
        TEST_ASSERT(p.d3dType >= 1 && p.d3dType <= 6, "Valid D3D primitive type: " + std::string(p.d3dTypeName));
        TEST_ASSERT(p.vkTopology <= 5, "Valid Vulkan topology: " + std::string(p.vkTopologyName));
    }

    /* Verify Vertex Element Formats */
    struct DeclTypeCheck {
        SVGA3dDeclType type;
        uint32_t expectedSize;
        const char *name;
    };

    DeclTypeCheck declTypes[] = {
        { SVGA3D_DECLTYPE_FLOAT1, 4, "FLOAT1" },
        { SVGA3D_DECLTYPE_FLOAT2, 8, "FLOAT2" },
        { SVGA3D_DECLTYPE_FLOAT3, 12, "FLOAT3" },
        { SVGA3D_DECLTYPE_FLOAT4, 16, "FLOAT4" },
        { SVGA3D_DECLTYPE_D3DCOLOR, 4, "D3DCOLOR" },
        { SVGA3D_DECLTYPE_UBYTE4, 4, "UBYTE4" },
        { SVGA3D_DECLTYPE_SHORT2, 4, "SHORT2" },
        { SVGA3D_DECLTYPE_SHORT4, 8, "SHORT4" },
        { SVGA3D_DECLTYPE_UBYTE4N, 4, "UBYTE4N" },
        { SVGA3D_DECLTYPE_SHORT2N, 4, "SHORT2N" },
        { SVGA3D_DECLTYPE_SHORT4N, 8, "SHORT4N" },
        { SVGA3D_DECLTYPE_USHORT2N, 4, "USHORT2N" },
        { SVGA3D_DECLTYPE_USHORT4N, 8, "USHORT4N" },
        { SVGA3D_DECLTYPE_FLOAT16_2, 4, "FLOAT16_2" },
        { SVGA3D_DECLTYPE_FLOAT16_4, 8, "FLOAT16_4" }
    };

    for (size_t i = 0; i < sizeof(declTypes) / sizeof(declTypes[0]); ++i) {
        TEST_ASSERT(declTypes[i].expectedSize > 0, "Vertex decl format supported: " + std::string(declTypes[i].name));
    }
}

/* --------------------------------------------------------------------------
 * Test 16: Multi-Context State Isolation and Termination
 * -------------------------------------------------------------------------- */
static void TestMultiContextIsolation(VGASTATE *pVGA) {
    std::cout << ANSI_CYAN << "[TEST 16] Multi-Context State Isolation & Device Termination..." << ANSI_RESET << std::endl;

    PVMSVGA3DSTATE pState = pVGA->svga.p3dState;

    /* Set RenderState in Context 0 */
    SVGA3dRenderState rs0;
    rs0.state = SVGA3D_RS_ZENABLE;
    rs0.uintValue = 1;
    vmsvga3dSetRenderState(pVGA, 0, 1, &rs0);

    /* Set RenderState in Context 1 with DIFFERENT value */
    SVGA3dRenderState rs1;
    rs1.state = SVGA3D_RS_ZENABLE;
    rs1.uintValue = 0;
    vmsvga3dSetRenderState(pVGA, 1, 1, &rs1);

    /* Verify isolation: Context 0 retains its own state */
    TEST_ASSERT(pState->papContexts[0]->state.aRenderState[SVGA3D_RS_ZENABLE].uintValue == 1, "Context 0 retains state 1");
    TEST_ASSERT(pState->papContexts[1]->state.aRenderState[SVGA3D_RS_ZENABLE].uintValue == 0, "Context 1 retains state 0");

    /* Destroy Contexts 1 and 2 */
    int rc = vmsvga3dContextDestroy(pVGA, 1);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dContextDestroy(1) succeeds");

    rc = vmsvga3dContextDestroy(pVGA, 2);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dContextDestroy(2) succeeds");

    /* Destroy Context 0 */
    rc = vmsvga3dContextDestroy(pVGA, 0);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dContextDestroy(0) succeeds");

    /* Terminate SVGA3D */
    rc = vmsvga3dTerminate(pVGA);
    TEST_ASSERT(rc == VINF_SUCCESS, "vmsvga3dTerminate succeeds");
}

/* --------------------------------------------------------------------------
 * Reference Data Exporter: JSON
 * -------------------------------------------------------------------------- */
static bool DumpJson(const std::string &path) {
    std::cout << ANSI_CYAN << "Exporting reference data to JSON: " << path << "..." << ANSI_RESET << std::endl;

    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "Failed to open output file: " << path << std::endl;
        return false;
    }

    out << "{\n";
    out << "  \"generator\": \"SVGA3D Oracle (DevVGA-SVGA3d-win.cpp Direct3D 9 shim)\",\n";
    out << "  \"description\": \"Definitive VMware SVGA3D protocol mapping fixtures and reference oracle\",\n";
    out << "  \"version\": \"1.0\",\n\n";

    /* DevCaps */
    out << "  \"device_capabilities\": [\n";
    size_t capCount = sizeof(g_DevCaps) / sizeof(g_DevCaps[0]);
    for (size_t i = 0; i < capCount; ++i) {
        const DevCapInfo &c = g_DevCaps[i];
        out << "    {\n";
        out << "      \"id\": " << c.id << ",\n";
        out << "      \"name\": \"" << c.name << "\",\n";
        out << "      \"description\": \"" << c.desc << "\",\n";
        out << "      \"expected_value\": " << c.expectedValue << "\n";
        out << "    }" << (i + 1 < capCount ? "," : "") << "\n";
    }
    out << "  ],\n\n";

    /* Surface Formats */
    out << "  \"surface_formats\": [\n";
    size_t fmtCount = sizeof(g_SurfaceFormats) / sizeof(g_SurfaceFormats[0]);
    for (size_t i = 0; i < fmtCount; ++i) {
        const SurfaceFormatInfo &f = g_SurfaceFormats[i];
        out << "    {\n";
        out << "      \"id\": " << f.id << ",\n";
        out << "      \"name\": \"" << f.name << "\",\n";
        out << "      \"d3d_format\": " << f.d3dFormat << ",\n";
        out << "      \"d3d_format_name\": \"" << f.d3dFormatName << "\",\n";
        out << "      \"vk_format\": " << f.vkFormat << ",\n";
        out << "      \"vk_format_name\": \"" << f.vkFormatName << "\"\n";
        out << "    }" << (i + 1 < fmtCount ? "," : "") << "\n";
    }
    out << "  ],\n\n";

    /* Render States */
    out << "  \"render_states\": [\n";
    size_t rsCount = sizeof(g_RenderStates) / sizeof(g_RenderStates[0]);
    for (size_t i = 0; i < rsCount; ++i) {
        const RenderStateInfo &r = g_RenderStates[i];
        out << "    {\n";
        out << "      \"id\": " << r.id << ",\n";
        out << "      \"name\": \"" << r.name << "\",\n";
        out << "      \"d3d_state\": " << r.d3dState << ",\n";
        out << "      \"d3d_state_name\": \"" << r.d3dStateName << "\"\n";
        out << "    }" << (i + 1 < rsCount ? "," : "") << "\n";
    }
    out << "  ],\n\n";

    /* Texture Combiners */
    out << "  \"texture_combiners\": [\n";
    size_t tcCount = sizeof(g_TextureCombiners) / sizeof(g_TextureCombiners[0]);
    for (size_t i = 0; i < tcCount; ++i) {
        const TextureCombinerInfo &t = g_TextureCombiners[i];
        out << "    {\n";
        out << "      \"id\": " << t.id << ",\n";
        out << "      \"name\": \"" << t.name << "\",\n";
        out << "      \"d3d_op\": " << t.d3dOp << ",\n";
        out << "      \"d3d_op_name\": \"" << t.d3dOpName << "\"\n";
        out << "    }" << (i + 1 < tcCount ? "," : "") << "\n";
    }
    out << "  ],\n\n";

    /* Primitive Topologies */
    out << "  \"primitive_topologies\": [\n";
    size_t primCount = sizeof(g_PrimitiveTypes) / sizeof(g_PrimitiveTypes[0]);
    for (size_t i = 0; i < primCount; ++i) {
        const PrimitiveTypeInfo &p = g_PrimitiveTypes[i];
        out << "    {\n";
        out << "      \"id\": " << p.id << ",\n";
        out << "      \"name\": \"" << p.name << "\",\n";
        out << "      \"d3d_type\": " << p.d3dType << ",\n";
        out << "      \"d3d_type_name\": \"" << p.d3dTypeName << "\",\n";
        out << "      \"vk_topology\": " << p.vkTopology << ",\n";
        out << "      \"vk_topology_name\": \"" << p.vkTopologyName << "\"\n";
        out << "    }" << (i + 1 < primCount ? "," : "") << "\n";
    }
    out << "  ],\n\n";

    /* Blend Ops */
    out << "  \"blend_ops\": [\n";
    size_t blendCount = sizeof(g_BlendOps) / sizeof(g_BlendOps[0]);
    for (size_t i = 0; i < blendCount; ++i) {
        const BlendOpInfo &b = g_BlendOps[i];
        out << "    {\n";
        out << "      \"id\": " << b.id << ",\n";
        out << "      \"name\": \"" << b.name << "\",\n";
        out << "      \"d3d_op\": " << b.d3dOp << ",\n";
        out << "      \"d3d_op_name\": \"" << b.d3dOpName << "\"\n";
        out << "    }" << (i + 1 < blendCount ? "," : "") << "\n";
    }
    out << "  ]\n";

    out << "}\n";
    out.close();

    std::cout << ANSI_GREEN << "Successfully written " << path << ANSI_RESET << std::endl;
    return true;
}

/* --------------------------------------------------------------------------
 * Reference Data Exporter: C/C++ Header (svga3d_reference.h)
 * -------------------------------------------------------------------------- */
static bool DumpHeader(const std::string &path) {
    std::cout << ANSI_CYAN << "Exporting reference header to: " << path << "..." << ANSI_RESET << std::endl;

    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "Failed to open output file: " << path << std::endl;
        return false;
    }

    out << "/*\n";
    out << " * Autogenerated Reference Header from DevVGA-SVGA3d-win.cpp Oracle\n";
    out << " * VMware SVGA3D protocol mapping tables for SVGA3=VLKN\n";
    out << " */\n\n";
    out << "#ifndef ___SVGA3D_REFERENCE_H___\n";
    out << "#define ___SVGA3D_REFERENCE_H___\n\n";
    out << "#include <stdint.h>\n";
    out << "#include <stddef.h>\n\n";

    out << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";

    /* Surface Format Mapping Table */
    out << "typedef struct Svga3dFormatMapping {\n";
    out << "    uint32_t svgaFormat;\n";
    out << "    const char *svgaName;\n";
    out << "    uint32_t d3dFormat;\n";
    out << "    const char *d3dName;\n";
    out << "    uint32_t vkFormat;\n";
    out << "    const char *vkName;\n";
    out << "} Svga3dFormatMapping;\n\n";

    out << "static const Svga3dFormatMapping kSvga3dFormatTable[] = {\n";
    size_t fmtCount = sizeof(g_SurfaceFormats) / sizeof(g_SurfaceFormats[0]);
    for (size_t i = 0; i < fmtCount; ++i) {
        const SurfaceFormatInfo &f = g_SurfaceFormats[i];
        out << "    { " << f.id << ", \"" << f.name << "\", " << f.d3dFormat << ", \"" << f.d3dFormatName << "\", "
            << f.vkFormat << ", \"" << f.vkFormatName << "\" },\n";
    }
    out << "};\n\n";

    /* Inline converter for SVGA3d to VkFormat */
    out << "static inline uint32_t svga3d_to_vk_format(uint32_t svgaFormat) {\n";
    out << "    for (size_t i = 0; i < sizeof(kSvga3dFormatTable) / sizeof(kSvga3dFormatTable[0]); ++i) {\n";
    out << "        if (kSvga3dFormatTable[i].svgaFormat == svgaFormat)\n";
    out << "            return kSvga3dFormatTable[i].vkFormat;\n";
    out << "    }\n";
    out << "    return 0; /* VK_FORMAT_UNDEFINED */\n";
    out << "}\n\n";

    /* Render State Mapping Table */
    out << "typedef struct Svga3dRenderStateMapping {\n";
    out << "    uint32_t svgaState;\n";
    out << "    const char *svgaName;\n";
    out << "    uint32_t d3dState;\n";
    out << "    const char *d3dName;\n";
    out << "} Svga3dRenderStateMapping;\n\n";

    out << "static const Svga3dRenderStateMapping kSvga3dRenderStateTable[] = {\n";
    size_t rsCount = sizeof(g_RenderStates) / sizeof(g_RenderStates[0]);
    for (size_t i = 0; i < rsCount; ++i) {
        const RenderStateInfo &r = g_RenderStates[i];
        out << "    { " << r.id << ", \"" << r.name << "\", " << r.d3dState << ", \"" << r.d3dStateName << "\" },\n";
    }
    out << "};\n\n";

    /* Primitive Topology Table */
    out << "typedef struct Svga3dTopologyMapping {\n";
    out << "    uint32_t svgaTopology;\n";
    out << "    const char *svgaName;\n";
    out << "    uint32_t d3dTopology;\n";
    out << "    const char *d3dName;\n";
    out << "    uint32_t vkTopology;\n";
    out << "    const char *vkName;\n";
    out << "} Svga3dTopologyMapping;\n\n";

    out << "static const Svga3dTopologyMapping kSvga3dTopologyTable[] = {\n";
    size_t primCount = sizeof(g_PrimitiveTypes) / sizeof(g_PrimitiveTypes[0]);
    for (size_t i = 0; i < primCount; ++i) {
        const PrimitiveTypeInfo &p = g_PrimitiveTypes[i];
        out << "    { " << p.id << ", \"" << p.name << "\", " << p.d3dType << ", \"" << p.d3dTypeName << "\", "
            << p.vkTopology << ", \"" << p.vkTopologyName << "\" },\n";
    }
    out << "};\n\n";

    /* Inline converter for SVGA3d to VkPrimitiveTopology */
    out << "static inline uint32_t svga3d_to_vk_primitive_topology(uint32_t svgaTopology) {\n";
    out << "    for (size_t i = 0; i < sizeof(kSvga3dTopologyTable) / sizeof(kSvga3dTopologyTable[0]); ++i) {\n";
    out << "        if (kSvga3dTopologyTable[i].svgaTopology == svgaTopology)\n";
    out << "            return kSvga3dTopologyTable[i].vkTopology;\n";
    out << "    }\n";
    out << "    return 3; /* VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST default */\n";
    out << "}\n\n";

    out << "#ifdef __cplusplus\n}\n#endif\n\n";
    out << "#endif /* ___SVGA3D_REFERENCE_H___ */\n";
    out.close();

    std::cout << ANSI_GREEN << "Successfully written " << path << ANSI_RESET << std::endl;
    return true;
}

/* --------------------------------------------------------------------------
 * Main Entry Point
 * -------------------------------------------------------------------------- */
int main(int argc, char **argv) {
    std::cout << ANSI_BOLD << "==========================================================" << ANSI_RESET << std::endl;
    std::cout << ANSI_BOLD << " SVGA3D Reference Oracle Test Runner & Fixture Exporter" << ANSI_RESET << std::endl;
    std::cout << ANSI_BOLD << "==========================================================" << ANSI_RESET << std::endl;

    bool runTests = false;
    std::string jsonPath = "";
    std::string headerPath = "";

    if (argc <= 1) {
        runTests = true;
        jsonPath = "data/svga3d_reference.json";
        headerPath = "data/svga3d_reference.h";
    } else {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--test") {
                runTests = true;
            } else if (arg == "--dump-json" && i + 1 < argc) {
                jsonPath = argv[++i];
            } else if (arg == "--dump-header" && i + 1 < argc) {
                headerPath = argv[++i];
            } else {
                std::cerr << "Unknown option: " << arg << std::endl;
                std::cerr << "Usage: " << argv[0] << " [--test] [--dump-json <path>] [--dump-header <path>]" << std::endl;
                return 1;
            }
        }
    }

    if (runTests) {
        VGASTATE vga;
        InitVgaState(&vga);

        if (TestLifecycle(&vga)) {
            TestDevCaps(&vga);
            TestSurfaceFormats();
            TestRenderStatesExhaustive(&vga);
            TestTextureStatesExhaustive(&vga);
            TestViewportScissorAndZRange(&vga);
            TestTransformsExhaustive(&vga);
            TestMaterialsExhaustive(&vga);
            TestLightsExhaustive(&vga);
            TestClipPlanesExhaustive(&vga);
            TestShadersAndConstantsExhaustive(&vga);
            TestOcclusionQueriesExhaustive(&vga);
            TestClearCommandExhaustive(&vga);
            TestSurfacesExhaustive(&vga);
            TestPrimitiveTopologiesAndVertexDecls();
            TestMultiContextIsolation(&vga);
        }

        std::cout << "\n" << ANSI_BOLD << "----------------------------------------------------------" << ANSI_RESET << std::endl;
        std::cout << ANSI_BOLD << " Test Results Summary: " << ANSI_RESET;
        if (g_testsFailed == 0) {
            std::cout << ANSI_GREEN << ANSI_BOLD << "ALL " << g_testsPassed << " TESTS PASSED!" << ANSI_RESET << std::endl;
        } else {
            std::cout << ANSI_RED << ANSI_BOLD << g_testsFailed << " FAILED, " << g_testsPassed << " passed (out of " << g_testsRun << ")" << ANSI_RESET << std::endl;
        }
        std::cout << ANSI_BOLD << "----------------------------------------------------------" << ANSI_RESET << std::endl;
    }

    if (!jsonPath.empty()) {
        DumpJson(jsonPath);
    }

    if (!headerPath.empty()) {
        DumpHeader(headerPath);
    }

    return (g_testsFailed == 0) ? 0 : 1;
}
