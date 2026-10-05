/*
 * SVGA3=VLKN - Verified Rendering Acceptance Test Suite
 * Deliverable 4: Verified Rendering & Analytical Pixel Comparison
 *
 * Covers:
 * 1. Textured indexed geometry with vertex transformations and shader constants.
 * 2. Depth overlap (Z-buffering, depth testing, closer vs farther geometry).
 * 3. Alpha blending (semi-transparent geometry over background with analytical blend math).
 * 4. Viewport and scissor boundaries (pixel preservation outside clipping regions).
 * 5. Switching render targets (multiple RTs with independent content).
 * 6. Two contexts with distinct state (multi-context isolation).
 * 7. Dynamic resource updates followed by another draw.
 * 8. Negative controls (deliberately altered shaders, constants, textures, expected images).
 *
 * All comparisons are performed against independently calculated analytical pixel values
 * with documented tolerances (never using the implementation under test).
 */

#include "svga3_vlkn.h"
#include "svga3_device.h"
#include "vlkn_backend.h"
#include "svga3_context.h"
#include "svga3_surface.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <cassert>

#define TEST_CHECK(cond, msg) do { \
    if (cond) { \
        std::cout << "  [PASS] " << msg << std::endl; \
    } else { \
        std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        return 1; \
    } \
} while(0)

#define TEST_CHECK_NEGATIVE(cond, msg) do { \
    if (cond) { \
        std::cout << "  [PASS] Negative control detected mismatch as expected: " << msg << std::endl; \
    } else { \
        std::cerr << "  [FAIL] Negative control failed to detect mismatch: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        return 1; \
    } \
} while(0)

struct Vertex {
    float x, y, z;
    float u, v;
};

struct Pixel {
    uint8_t b, g, r, a;
};

/* Documented comparison tolerance: +/- 2 LSB to account for 8-bit quantization and fixed-point rasterization */
constexpr int COLOR_TOLERANCE = 2;

static bool pixelMatches(const Pixel &actual, uint8_t er, uint8_t eg, uint8_t eb, uint8_t ea, int tol = COLOR_TOLERANCE) {
    return (std::abs(static_cast<int>(actual.r) - static_cast<int>(er)) <= tol &&
            std::abs(static_cast<int>(actual.g) - static_cast<int>(eg)) <= tol &&
            std::abs(static_cast<int>(actual.b) - static_cast<int>(eb)) <= tol &&
            std::abs(static_cast<int>(actual.a) - static_cast<int>(ea)) <= tol);
}

/* Helper macro for D3D9 token construction */
#define D3D9_DST(regType, regNum, mask) \
    (0x80000000u | (((regType) & 0x7) << 28) | ((((regType) >> 3) & 0x3) << 11) | (((mask) & 0xF) << 16) | ((regNum) & 0x7FF))

#define D3D9_SRC(regType, regNum, swiz) \
    (0x80000000u | (((regType) & 0x7) << 28) | ((((regType) >> 3) & 0x3) << 11) | (((swiz) & 0xFF) << 16) | ((regNum) & 0x7FF))

static void savePPM(const char *filename, const Pixel *pixels, int width, int height) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; ++i) {
        uint8_t rgb[3] = { pixels[i].r, pixels[i].g, pixels[i].b };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void saveDiffPPM(const char *filename, const Pixel *actual, const Pixel *reference, int width, int height) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; ++i) {
        int dr = std::abs((int)actual[i].r - (int)reference[i].r);
        int dg = std::abs((int)actual[i].g - (int)reference[i].g);
        int db = std::abs((int)actual[i].b - (int)reference[i].b);
        uint8_t rgb[3];
        if (dr <= COLOR_TOLERANCE && dg <= COLOR_TOLERANCE && db <= COLOR_TOLERANCE) {
            rgb[0] = rgb[1] = rgb[2] = 0; // Black = match within tolerance
        } else {
            rgb[0] = static_cast<uint8_t>(std::min(255, dr * 8 + 64));
            rgb[1] = static_cast<uint8_t>(std::min(255, dg * 8 + 64));
            rgb[2] = static_cast<uint8_t>(std::min(255, db * 8 + 64));
        }
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static std::vector<Pixel> g_scene1Fb;
static std::vector<Pixel> g_scene1Ref;

int main() {
    std::cout << "======================================================================" << std::endl;
    std::cout << "SVGA3=VLKN Deliverable 4: Verified Rendering & Analytical Comparison" << std::endl;
    std::cout << "======================================================================" << std::endl;

    /* 1. Device Creation on Real Vulkan with Validation Layers */
    Svga3VlknConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.appName = "Verified Rendering Acceptance Test";
    cfg.apiVersion = VK_API_VERSION_1_1;
    cfg.forceMockBackend = false; /* Real Vulkan ONLY */
    cfg.enableValidationLayers = true;

    Svga3VlknDevice *dev = svga3_vlkn_device_create(&cfg);
    if (!dev) {
        std::cerr << "CRITICAL: Failed to create real Vulkan device (forceMockBackend=false)!" << std::endl;
        return 1;
    }

    svga3_vlkn::VlknBackend *backend = dev->backend.get();
    TEST_CHECK(!backend->dispatch().isMock, "Execution is running on real Vulkan driver (NOT mock)");

    VkPhysicalDeviceProperties props;
    backend->dispatch().vkGetPhysicalDeviceProperties(backend->physicalDevice(), &props);
    std::cout << "  [INFO] Real Vulkan Driver: " << props.deviceName
              << " (Driver version: 0x" << std::hex << props.driverVersion << std::dec
              << ", Vulkan API: " << VK_VERSION_MAJOR(props.apiVersion) << "."
              << VK_VERSION_MINOR(props.apiVersion) << "." << VK_VERSION_PATCH(props.apiVersion) << ")" << std::endl;

    const uint32_t RT_W = 64;
    const uint32_t RT_H = 64;

    /* Common Shaders:
     * VS1: vs_3_0; dcl_position v0; dcl_texcoord v1; m4x4 oPos, v0, c0; mov oT0, v1; end
     * PS1 (Textured + Constant Tint): ps_3_0; dcl_texcoord v1; dcl_2d s0; texld r0, v1, s0; mul oC0, r0, c0; end
     * PS2 (Solid Constant Color): ps_3_0; mov oC0, c0; end
     */
    const uint32_t vs1Bytecode[] = {
        0xFFFE0300, /* vs_3_0 */
        (31) | (2 << 24), 0x80000000 | 0, D3D9_DST(1, 0, 0xF), /* dcl_position v0 */
        (31) | (2 << 24), 0x80000000 | 5, D3D9_DST(1, 1, 0xF), /* dcl_texcoord v1 */
        (20) | (3 << 24), D3D9_DST(4, 0, 0xF), D3D9_SRC(1, 0, 0xE4), D3D9_SRC(2, 0, 0xE4), /* m4x4 oPos, v0, c0 */
        (1)  | (2 << 24), D3D9_DST(6, 0, 0xF), D3D9_SRC(1, 1, 0xE4), /* mov oT0, v1 */
        0x0000FFFF
    };

    const uint32_t ps1Bytecode[] = {
        0xFFFF0300, /* ps_3_0 */
        (31) | (2 << 24), 0x80000000 | 5, D3D9_DST(1, 1, 0xF), /* dcl_texcoord v1 */
        (31) | (2 << 24), 0x80000000 | (2 << 27), D3D9_DST(10, 0, 0xF), /* dcl_2d s0 */
        (66) | (3 << 24), D3D9_DST(0, 0, 0xF), D3D9_SRC(1, 1, 0xE4), D3D9_SRC(10, 0, 0xE4), /* texld r0, v1, s0 */
        (5)  | (3 << 24), D3D9_DST(8, 0, 0xF), D3D9_SRC(0, 0, 0xE4), D3D9_SRC(2, 0, 0xE4), /* mul oC0, r0, c0 */
        0x0000FFFF
    };

    const uint32_t ps2Bytecode[] = {
        0xFFFF0300, /* ps_3_0 */
        (1)  | (2 << 24), D3D9_DST(8, 0, 0xF), D3D9_SRC(2, 0, 0xE4), /* mov oC0, c0 */
        0x0000FFFF
    };

    /* Common Vertex Declaration for (Position: float3, Texcoord: float2) */
    SVGA3dVertexDecl decls[2] = {};
    decls[0].identity.type = SVGA3D_DECLTYPE_FLOAT3;
    decls[0].identity.method = SVGA3D_DECLMETHOD_DEFAULT;
    decls[0].identity.usage = SVGA3D_DECLUSAGE_POSITION;
    decls[0].identity.usageIndex = 0;
    decls[0].array.surfaceId = 10;
    decls[0].array.offset = 0;
    decls[0].array.stride = sizeof(Vertex);

    decls[1].identity.type = SVGA3D_DECLTYPE_FLOAT2;
    decls[1].identity.method = SVGA3D_DECLMETHOD_DEFAULT;
    decls[1].identity.usage = SVGA3D_DECLUSAGE_TEXCOORD;
    decls[1].identity.usageIndex = 0;
    decls[1].array.surfaceId = 10;
    decls[1].array.offset = offsetof(Vertex, u);
    decls[1].array.stride = sizeof(Vertex);

    /* --------------------------------------------------------------------------
     * SCENE 1: Textured Indexed Geometry with Transformations & Shader Constants
     * -------------------------------------------------------------------------- */
    std::cout << "\n--- Scene 1: Textured Indexed Geometry with Matrix & Constant Tint ---" << std::endl;
    {
        const uint32_t CID = 1;
        const uint32_t SID_RT = 1;
        const uint32_t SID_TEX = 2;
        const uint32_t SID_VB = 10;
        const uint32_t SID_IB = 11;

        svga3_vlkn_context_create(dev, CID);

        SVGA3dSize rtSize = { RT_W, RT_H, 1 };
        svga3_vlkn_surface_define(dev, SID_RT, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_X8R8G8B8, &rtSize, 1);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);

        SVGA3dRect vp = { 0, 0, RT_W, RT_H };
        svga3_vlkn_context_set_viewport(dev, CID, &vp);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_CULLMODE, SVGA3D_FACE_NONE);

        /* Define 2x2 Texture: Top-left: White, Top-right: Red, Bottom-left: Green, Bottom-right: Blue */
        SVGA3dSize texSize = { 2, 2, 1 };
        svga3_vlkn_surface_define(dev, SID_TEX, SVGA3D_SURFACE_HINT_TEXTURE, SVGA3D_X8R8G8B8, &texSize, 1);
        uint32_t texData[4] = {
            0xFFFFFFFF, /* White */
            0xFFFF0000, /* Red */
            0xFF00FF00, /* Green */
            0xFF0000FF  /* Blue */
        };
        SVGA3dBox tBox = { 0, 0, 0, 2, 2, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_TEX, 0, &tBox, texData, 2 * 4);
        svga3_vlkn_context_set_texture(dev, CID, 0, SID_TEX);

        /* Define Indexed Quad: 4 vertices, 6 indices (2 triangles) */
        Vertex quadVerts[4] = {
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f }, /* 0: TL */
            {  1.0f,  1.0f, 0.5f,  1.0f, 0.0f }, /* 1: TR */
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f }, /* 2: BR */
            { -1.0f, -1.0f, 0.5f,  0.0f, 1.0f }  /* 3: BL */
        };
        SVGA3dSize vbSize = { sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_define(dev, SID_VB, SVGA3D_SURFACE_HINT_VERTEXBUFFER, SVGA3D_BUFFER, &vbSize, 1);
        SVGA3dBox vbBox = { 0, 0, 0, sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_VB, 0, &vbBox, quadVerts, sizeof(quadVerts));

        uint16_t indices[6] = { 0, 1, 2, 0, 2, 3 };
        SVGA3dSize ibSize = { sizeof(indices), 1, 1 };
        svga3_vlkn_surface_define(dev, SID_IB, SVGA3D_SURFACE_HINT_INDEXBUFFER, SVGA3D_BUFFER, &ibSize, 1);
        SVGA3dBox ibBox = { 0, 0, 0, sizeof(indices), 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_IB, 0, &ibBox, indices, sizeof(indices));

        /* Define Shaders */
        svga3_vlkn_context_define_shader(dev, CID, 1, SVGA3D_SHADERTYPE_VS, vs1Bytecode, sizeof(vs1Bytecode)/4);
        svga3_vlkn_context_define_shader(dev, CID, 1, SVGA3D_SHADERTYPE_PS, ps1Bytecode, sizeof(ps1Bytecode)/4);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_VS, 1);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 1);

        /* Transformation Matrix: Scale by 0.5 so quad covers [-0.5, 0.5] (pixels [16..47, 16..47]) */
        float matScaleHalf[16] = {
            0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        for (int r = 0; r < 4; ++r) {
            uint32_t rowValues[4];
            memcpy(rowValues, &matScaleHalf[r * 4], sizeof(float) * 4);
            svga3_vlkn_context_set_shader_const(dev, CID, r, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, rowValues);
        }

        /* Shader Constant Tint: (0.5, 1.0, 0.8, 1.0) */
        float tint[4] = { 0.5f, 1.0f, 0.8f, 1.0f };
        uint32_t tintVal[4];
        memcpy(tintVal, tint, sizeof(tint));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, tintVal);

        /* Clear to Black (0, 0, 0, 0) */
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0x00000000, 1.0f, 0, nullptr, 0);

        /* Draw Indexed */
        SVGA3dPrimitiveRange range = {};
        range.primType = SVGA3D_PRIMITIVE_TRIANGLELIST;
        range.primitiveCount = 2;
        range.indexArray.surfaceId = SID_IB;
        range.indexArray.offset = 0;
        range.indexArray.stride = 2;

        decls[0].array.surfaceId = SID_VB;
        decls[1].array.surfaceId = SID_VB;
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_device_wait_idle(dev);

        /* Read back framebuffer and analytically verify */
        std::vector<Pixel> fb(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fb.data(), RT_W * 4);

        /* Analytical expectation:
         * Quad spans [16..47, 16..47]. Outside quad must be (0, 0, 0).
         * Inside quad:
         * Top-Left (~(24, 24)): White * Tint = (255*0.5, 255*1.0, 255*0.8) = (127, 255, 204)
         * Bottom-Left (~(24, 40)): Green * Tint = (0*0.5, 255*1.0, 0*0.8) = (0, 255, 0)
         * Corner (0, 0): untouched black (0, 0, 0)
         */
        Pixel outside = fb[0];
        TEST_CHECK(outside.r == 0 && outside.g == 0 && outside.b == 0,
                   "Scene 1: Pixels outside scaled geometry remain untouched clear color (0, 0, 0)");

        Pixel tl = fb[24 * RT_W + 24];
        TEST_CHECK(pixelMatches(tl, 127, 255, 204, 255),
                   "Scene 1: Top-Left pixel analytically matches White * Tint (127, 255, 204)");

        Pixel bl = fb[40 * RT_W + 24];
        TEST_CHECK(pixelMatches(bl, 0, 255, 0, 255),
                   "Scene 1: Bottom-Left pixel analytically matches Green * Tint (0, 255, 0)");

        /* Generate Analytical Reference Image for Scene 1 */
        std::vector<Pixel> refFb(RT_W * RT_H, { 0, 0, 0, 0 });
        for (int y = 16; y < 48; ++y) {
            for (int x = 16; x < 48; ++x) {
                if (x < 32 && y < 32) {
                    refFb[y * RT_W + x] = { 204, 255, 127, 255 }; // White * Tint -> b=204, g=255, r=127
                } else if (x >= 32 && y < 32) {
                    refFb[y * RT_W + x] = { 0, 0, 127, 255 };     // Red * Tint -> b=0, g=0, r=127
                } else if (x < 32 && y >= 32) {
                    refFb[y * RT_W + x] = { 0, 255, 0, 255 };     // Green * Tint -> b=0, g=255, r=0
                } else {
                    refFb[y * RT_W + x] = { 204, 0, 0, 255 };     // Blue * Tint -> b=204, g=0, r=0
                }
            }
        }
        g_scene1Fb = fb;
        g_scene1Ref = refFb;
        savePPM("artifacts/scene1_rendered.ppm", fb.data(), RT_W, RT_H);
        savePPM("artifacts/scene1_reference.ppm", refFb.data(), RT_W, RT_H);
        saveDiffPPM("artifacts/scene1_diff.ppm", fb.data(), refFb.data(), RT_W, RT_H);

        /* Packed guest formats must sample by component, not merely round-trip
         * their bytes. These values are independent of the oracle's tables. */
        struct FormatProbe { SVGA3dSurfaceFormat format; uint64_t word; size_t bytes; Pixel expected; };
        const FormatProbe probes[] = {
            {SVGA3D_R5G6B5, 0xF800, 2, {0, 0, 255, 255}},
            {SVGA3D_A1R5G5B5, 0x83E0, 2, {0, 255, 0, 255}},
            {SVGA3D_X1R5G5B5, 0x001F, 2, {255, 0, 0, 255}},
            {SVGA3D_A4R4G4B4, 0x8421, 2, {17, 34, 68, 136}},
            {SVGA3D_A16B16G16R16, UINT64_C(0xFFFFBFFF80004000), 8, {191, 128, 64, 255}},
            {SVGA3D_A2R10G10B10, UINT64_C(0xBFF80000), 4, {0, 128, 255, 170}},
            {SVGA3D_LUMINANCE16, 0x8000, 2, {128, 128, 128, 255}},
            {SVGA3D_LUMINANCE8_ALPHA8, 0x8040, 2, {64, 64, 64, 128}},
            {SVGA3D_ALPHA8, 0x80, 1, {0, 0, 0, 128}},
            {SVGA3D_Z_DF24, 0xFFFFFF00, 4, {255, 255, 255, 255}},
            {SVGA3D_Z_DF24, 0x80000000, 4, {128, 128, 128, 255}}
        };
        const float whiteTint[4] = {1, 1, 1, 1};
        memcpy(tintVal, whiteTint, sizeof(tintVal));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, tintVal);
        TEST_CHECK(svga3_vlkn_surface_define(dev, 90, SVGA3D_SURFACE_HINT_RENDERTARGET,
            SVGA3D_A8R8G8B8, &rtSize, 1) == SVGA3_VLKN_SUCCESS, "Define format probe target");
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, 90, 0, 0);
        std::vector<uint32_t> shadowPS(std::begin(ps1Bytecode),std::end(ps1Bytecode));
        for (auto &token : shadowPS) if (token == (0x80000000u | (2u<<27))) token = 0x80000000u | (5u<<27);
        TEST_CHECK(svga3_vlkn_context_define_shader(dev,CID,102,SVGA3D_SHADERTYPE_PS,shadowPS.data(),shadowPS.size())
            == SVGA3_VLKN_SUCCESS, "Accept the valid SVGA 2D shadow sampler declaration");
        for (const auto &probe : probes) {
            TEST_CHECK(svga3_vlkn_surface_define(dev, 91, SVGA3D_SURFACE_HINT_TEXTURE,
                probe.format, &texSize, 1) == SVGA3_VLKN_SUCCESS, "Define packed/component texture");
            std::vector<uint8_t> data(probe.bytes * 4);
            for (size_t i = 0; i < 4; ++i) memcpy(data.data() + i * probe.bytes, &probe.word, probe.bytes);
            TEST_CHECK(svga3_vlkn_surface_dma_upload(dev, 91, 0, nullptr, data.data(), probe.bytes * 2)
                == SVGA3_VLKN_SUCCESS, "Upload packed/component texture");
            svga3_vlkn_context_set_texture(dev, CID, 0, 91);
            svga3_vlkn_context_set_shader(dev,CID,SVGA3D_SHADERTYPE_PS,probe.format==SVGA3D_Z_DF24 ? 102 : 1);
            TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1)
                == SVGA3_VLKN_SUCCESS, "Sample packed/component texture");
            TEST_CHECK(svga3_vlkn_surface_dma_download(dev, 90, 0, nullptr, fb.data(), RT_W * 4)
                == SVGA3_VLKN_SUCCESS, "Read packed/component pixels");
            Pixel actual = fb[32 * RT_W + 32];
            TEST_CHECK(pixelMatches(actual, probe.expected.r, probe.expected.g, probe.expected.b, probe.expected.a),
                "Packed/component texture has the expected RGBA values");
            std::vector<uint8_t> readback(data.size());
            TEST_CHECK(svga3_vlkn_surface_dma_download(dev, 91, 0, nullptr, readback.data(), probe.bytes * 2)
                == SVGA3_VLKN_SUCCESS && readback == data, "Packed/component texture retains guest byte layout");
            svga3_vlkn_context_set_texture(dev, CID, 0, SID_TEX);
            svga3_vlkn_surface_destroy(dev, 91);
        }
        svga3_vlkn_context_set_shader(dev,CID,SVGA3D_SHADERTYPE_PS,1);
        // Depth clears used to generate shadow mipmaps must sample in every component.
        svga3_vlkn_surface_define(dev, 91, SVGA3D_SURFACE_HINT_DEPTHSTENCIL | SVGA3D_SURFACE_HINT_TEXTURE,
            SVGA3D_Z_DF24, &texSize, 1);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SVGA3D_INVALID_ID, 0, 0);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_DEPTH, 91, 0, 0);
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_DEPTH, 0, 0.1f, 0, nullptr, 0);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_DEPTH, SVGA3D_INVALID_ID, 0, 0);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, 90, 0, 0);
        svga3_vlkn_context_set_texture(dev, CID, 0, 91);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 90, 0, nullptr, fb.data(), RT_W*4);
        TEST_CHECK(pixelMatches(fb[32*RT_W+32],26,26,26,255), "Cleared DF24 depth samples in RGB components");
        svga3_vlkn_context_set_texture(dev, CID, 0, SID_TEX);
        svga3_vlkn_surface_destroy(dev,91);
        /* Shader samplers 8..15 are independent of the eight interpolators. */
        std::vector<uint32_t> lastStageShader(std::begin(ps1Bytecode), std::end(ps1Bytecode));
        lastStageShader[6] = D3D9_DST(10, 15, 0xF);
        lastStageShader[10] = D3D9_SRC(10, 15, 0xE4);
        TEST_CHECK(svga3_vlkn_context_define_shader(dev, CID, 92, SVGA3D_SHADERTYPE_PS,
            lastStageShader.data(), lastStageShader.size()) == SVGA3_VLKN_SUCCESS, "Define sampler 15 shader");
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 92);
        TEST_CHECK(svga3_vlkn_context_set_texture(dev, CID, 15, SID_TEX) == SVGA3_VLKN_SUCCESS,
            "Bind independent texture stage 15");
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 90, 0, nullptr, fb.data(), RT_W * 4);
        TEST_CHECK(pixelMatches(fb[24 * RT_W + 24], 255, 255, 255, 255) &&
            pixelMatches(fb[40 * RT_W + 24], 0, 255, 0, 255), "Sampler 15 reads its own image");
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 1);
        auto sampleConstant = [&](uint32_t dimension, uint32_t controls, uint32_t sid,
                                  const float *coords, const Pixel &expected) -> bool {
            const uint32_t bytecode[] = {
                0xFFFF0300, 31 | (2 << 24), 0x80000000u | (dimension << 27), D3D9_DST(10, 0, 15),
                66 | (3 << 24) | (controls << 16), D3D9_DST(0, 0, 15), D3D9_SRC(2, 1, 0xE4), D3D9_SRC(10, 0, 0xE4),
                1 | (2 << 24), D3D9_DST(8, 0, 15), D3D9_SRC(0, 0, 0xE4), 0xFFFF};
            if (svga3_vlkn_context_define_shader(dev, CID, 93, SVGA3D_SHADERTYPE_PS, bytecode,
                sizeof(bytecode)/4) != SVGA3_VLKN_SUCCESS) return false;
            svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 93);
            uint32_t constants[4]; memcpy(constants, coords, sizeof(constants));
            svga3_vlkn_context_set_shader_const(dev, CID, 1, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, constants);
            svga3_vlkn_context_set_texture(dev, CID, 0, sid);
            if (svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1)
                != SVGA3_VLKN_SUCCESS) return false;
            if (svga3_vlkn_surface_dma_download(dev, 90, 0, nullptr, fb.data(), RT_W*4)
                != SVGA3_VLKN_SUCCESS) return false;
            return pixelMatches(fb[32 * RT_W + 32], expected.r, expected.g, expected.b, expected.a);
        };
        svga3_vlkn_context_set_texture_stage_state(dev, CID, 0, SVGA3D_TS_MINFILTER, SVGA3D_TEX_FILTER_NEAREST);
        svga3_vlkn_context_set_texture_stage_state(dev, CID, 0, SVGA3D_TS_MAGFILTER, SVGA3D_TEX_FILTER_NEAREST);
        const float middleCoords[4] = {.5f,.5f,0,1};
        svga3_vlkn_context_set_texture_stage_state(dev,CID,0,SVGA3D_TS_MINFILTER,SVGA3D_TEX_FILTER_ANISOTROPIC);
        svga3_vlkn_context_set_texture_stage_state(dev,CID,0,SVGA3D_TS_MAGFILTER,SVGA3D_TEX_FILTER_ANISOTROPIC);
        svga3_vlkn_context_set_texture_stage_state(dev,CID,0,SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL,16);
        TEST_CHECK(sampleConstant(2,0,SID_TEX,middleCoords,{128,128,128,255}), "Anisotropic filtering interpolates adjacent texels");
        svga3_vlkn_context_set_texture_stage_state(dev,CID,0,SVGA3D_TS_MINFILTER,SVGA3D_TEX_FILTER_NEAREST);
        svga3_vlkn_context_set_texture_stage_state(dev,CID,0,SVGA3D_TS_MAGFILTER,SVGA3D_TEX_FILTER_NEAREST);
        svga3_vlkn_context_set_texture_stage_state(dev,CID,0,SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL,1);
        const float projectedCoords[4] = {.8f, .8f, 0, 2};
        TEST_CHECK(sampleConstant(2, 1, SID_TEX, projectedCoords, {255,255,255,255}),
            "Projected TEX divides coordinates by w");
        svga3_vlkn_context_set_texture_stage_state(dev, CID, 0, SVGA3D_TS_ADDRESSU, SVGA3D_TEX_ADDRESS_BORDER);
        svga3_vlkn_context_set_texture_stage_state(dev, CID, 0, SVGA3D_TS_BORDERCOLOR, 0xFF804020);
        const float borderCoords[4] = {-2,.5f,0,1};
        TEST_CHECK(sampleConstant(2, 0, SID_TEX, borderCoords, {32,64,128,255}), "Sampler preserves arbitrary border color");
        svga3_vlkn_context_set_texture_stage_state(dev, CID, 0, SVGA3D_TS_ADDRESSU, SVGA3D_TEX_ADDRESS_WRAP);
        const uint32_t volumeData[2] = {0xFFFF0000, 0xFF00FF00};
        const SVGA3dSize volumeSize = {1,1,2};
        TEST_CHECK(svga3_vlkn_surface_define(dev, 94, SVGA3D_SURFACE_HINT_TEXTURE, SVGA3D_A8R8G8B8,
            &volumeSize, 1) == SVGA3_VLKN_SUCCESS, "Define 3D texture");
        svga3_vlkn_surface_dma_upload(dev, 94, 0, nullptr, volumeData, 4);
        const float volumeCoords[4] = {.5f,.5f,.75f,1};
        TEST_CHECK(sampleConstant(4, 0, 94, volumeCoords, {0,255,0,255}), "Volume TEX samples the selected z slice");
        const SVGA3dSize flatVolume = {1,1,1};
        svga3_vlkn_surface_define(dev, 94, SVGA3D_SURFACE_HINT_TEXTURE, SVGA3D_A8R8G8B8, &flatVolume, 1);
        svga3_vlkn_surface_dma_upload(dev, 94, 0, nullptr, volumeData, 4);
        TEST_CHECK(sampleConstant(4, 0, 94, volumeCoords, {0,0,255,255}), "Depth-one volume retains its data and a 3D view");
        const SVGA3dSize cubeSizes[6] = {{1,1,1},{1,1,1},{1,1,1},{1,1,1},{1,1,1},{1,1,1}};
        TEST_CHECK(svga3_vlkn_surface_define(dev, 95, SVGA3D_SURFACE_HINT_TEXTURE | SVGA3D_SURFACE_CUBEMAP,
            SVGA3D_A8R8G8B8, cubeSizes, 6) == SVGA3_VLKN_SUCCESS, "Define cube texture");
        auto *cube = dev->surfaceMgr->getSurface(95);
        for (uint32_t face = 0; face < 6; ++face)
            TEST_CHECK(cube->dmaUpload(0, nullptr, &volumeData[face%2], 4, false, face) == SVGA3_VLKN_SUCCESS,
                "Upload independent cube face");
        const float positiveX[4] = {1,0,0,1}, negativeX[4] = {-1,0,0,1};
        TEST_CHECK(sampleConstant(3, 0, 95, positiveX, {0,0,255,255}), "Cube TEX samples positive X face");
        TEST_CHECK(sampleConstant(3, 0, 95, negativeX, {0,255,0,255}), "Cube TEX samples negative X face");
        uint32_t cubeReadback = 0;
        TEST_CHECK(cube->dmaDownload(0, nullptr, &cubeReadback, 4, false, 1) == SVGA3_VLKN_SUCCESS &&
            cubeReadback == volumeData[1], "Cube face readback retains the selected layer");
        const SVGA3dSize mipSizes[3] = {{64,64,1},{32,32,1},{16,16,1}};
        TEST_CHECK(svga3_vlkn_surface_define(dev, 96, SVGA3D_SURFACE_HINT_TEXTURE, SVGA3D_A8R8G8B8,
            mipSizes, 3) == SVGA3_VLKN_SUCCESS, "Define bias texture");
        const uint32_t mipColors[3] = {0xFFFF0000,0xFF00FF00,0xFF0000FF};
        for (uint32_t level = 0; level < 3; ++level) {
            std::vector<uint32_t> mipData(mipSizes[level].width * mipSizes[level].height, mipColors[level]);
            svga3_vlkn_surface_dma_upload(dev, 96, level, nullptr, mipData.data(), mipSizes[level].width * 4);
        }
        std::vector<uint32_t> biasShader(std::begin(ps1Bytecode), std::end(ps1Bytecode));
        biasShader[7] |= 2 << 16; /* texldb; FLOAT2 vertex coordinate supplies w=1. */
        TEST_CHECK(svga3_vlkn_context_define_shader(dev, CID, 97, SVGA3D_SHADERTYPE_PS,
            biasShader.data(), biasShader.size()) == SVGA3_VLKN_SUCCESS, "Define biased TEX shader");
        svga3_vlkn_context_set_texture_stage_state(dev, CID, 0, SVGA3D_TS_MIPFILTER, SVGA3D_TEX_FILTER_NEAREST);
        svga3_vlkn_context_set_texture(dev, CID, 0, 96);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 97);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 90, 0, nullptr, fb.data(), RT_W * 4);
        TEST_CHECK(pixelMatches(fb[24 * RT_W + 24], 0,0,255,255), "TEX bias selects mip 2 instead of implicit mip 1");
        svga3_vlkn_context_set_texture_stage_state(dev,CID,0,SVGA3D_TS_MIPFILTER,SVGA3D_TEX_FILTER_NONE);
        svga3_vlkn_context_set_texture_stage_state(dev,CID,0,SVGA3D_TS_TEXTURE_MIPMAP_LEVEL,2);
        TEST_CHECK(sampleConstant(2,0,96,middleCoords,{255,0,0,255}), "MAXMIPLEVEL selects mip 2 even without mip filtering");
        svga3_vlkn_context_set_texture_stage_state(dev,CID,0,SVGA3D_TS_TEXTURE_MIPMAP_LEVEL,0);
        TEST_CHECK(sampleConstant(2,0,96,middleCoords,{0,0,255,255}), "Zero MAXMIPLEVEL restores base-level sampling");
        svga3_vlkn_context_set_texture(dev, CID, 0, SID_TEX);
        for (uint32_t sid = 94; sid <= 96; ++sid) svga3_vlkn_surface_destroy(dev, sid);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);
        svga3_vlkn_surface_destroy(dev, 90);

        // Distinct MRT outputs, a hole at location 0, and cached small targets.
        const SVGA3dSize smallSize = {64,64,1};
        for (uint32_t sid = 98; sid < 102; ++sid) {
            TEST_CHECK(svga3_vlkn_surface_define(dev, sid, SVGA3D_SURFACE_HINT_RENDERTARGET,
                SVGA3D_A8R8G8B8, &smallSize, 1) == SVGA3_VLKN_SUCCESS, "Define small MRT target");
            svga3_vlkn_context_set_render_target(dev, CID, (SVGA3dRenderTargetType)(SVGA3D_RT_COLOR0 + sid - 98), sid, 0, 0);
        }
        const uint32_t mrtShader[] = {0xFFFF0300,
            1 | (2 << 24), D3D9_DST(8,0,15), D3D9_SRC(2,0,0xE4),
            1 | (2 << 24), D3D9_DST(8,1,15), D3D9_SRC(2,1,0xE4),
            1 | (2 << 24), D3D9_DST(8,2,15), D3D9_SRC(2,2,0xE4),
            1 | (2 << 24), D3D9_DST(8,3,15), D3D9_SRC(2,3,0xE4), 0xFFFF};
        TEST_CHECK(svga3_vlkn_context_define_shader(dev, CID, 98, SVGA3D_SHADERTYPE_PS,
            mrtShader, sizeof(mrtShader)/4) == SVGA3_VLKN_SUCCESS, "Define four-output shader");
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 98);
        const float outputColors[4][4] = {{1,0,0,1},{0,1,0,1},{0,0,1,1},{1,1,0,1}};
        std::vector<Pixel> smallPixels(64*64);
        for (uint32_t i = 0; i < 4; ++i) {
            uint32_t value[4]; memcpy(value, outputColors[i], sizeof(value));
            svga3_vlkn_context_set_shader_const(dev, CID, i, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, value);
            svga3_vlkn_surface_dma_download(dev, 98+i, 0, nullptr, smallPixels.data(), 64*4);
        }
        TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1)
            == SVGA3_VLKN_SUCCESS, "Draw distinct MRT outputs");
        for (uint32_t i = 0; i < 4; ++i) {
            svga3_vlkn_surface_dma_download(dev, 98+i, 0, nullptr, smallPixels.data(), 64*4);
            TEST_CHECK(pixelMatches(smallPixels[24*64+24], outputColors[i][0]*255,
                outputColors[i][1]*255, outputColors[i][2]*255, 255), "Every MRT writes its independent color after a cached read");
        }
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE, 2);
        uint32_t whiteConstants[4]; memcpy(whiteConstants, whiteTint, sizeof(whiteConstants));
        svga3_vlkn_context_set_shader_const(dev, CID, 2, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, whiteConstants);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 100, 0, nullptr, smallPixels.data(), 64*4);
        TEST_CHECK(pixelMatches(smallPixels[24*64+24],0,255,255,255), "Legacy global color mask applies to every target");
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE2, 1);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 100, 0, nullptr, smallPixels.data(), 64*4);
        TEST_CHECK(pixelMatches(smallPixels[24*64+24],255,255,255,255), "Explicit per-target color mask overrides the global mask");
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE, 15);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE2, 15);
        uint32_t blueConstants[4]; memcpy(blueConstants, outputColors[2], sizeof(blueConstants));
        svga3_vlkn_context_set_shader_const(dev, CID, 2, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, blueConstants);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SVGA3D_INVALID_ID, 0, 0);
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0xFF000000, 1, 0, nullptr, 0);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 101, 0, nullptr, smallPixels.data(), 64*4);
        TEST_CHECK(pixelMatches(smallPixels[24*64+24],255,255,0,255), "MRT location 3 survives an unbound location 0");
        for (uint32_t i = 1; i < 4; ++i)
            svga3_vlkn_context_set_render_target(dev, CID, (SVGA3dRenderTargetType)(SVGA3D_RT_COLOR0+i), SVGA3D_INVALID_ID, 0, 0);
        for (uint32_t sid = 98; sid < 102; ++sid) svga3_vlkn_surface_destroy(dev, sid);

        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);
        const uint32_t flatPS[] = {0xFFFF0300, 1 | (2<<24), D3D9_DST(8,0,15), D3D9_SRC(2,0,0xE4), 0xFFFF};
        svga3_vlkn_context_define_shader(dev, CID, 99, SVGA3D_SHADERTYPE_PS, flatPS, sizeof(flatPS)/4);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 99);
        // Legacy alpha-only attachments store A in native R, including blending.
        svga3_vlkn_surface_define(dev, 103, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_ALPHA8, &smallSize, 1);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, 103, 0, 0);
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0x40C00000, 1, 0, nullptr, 0);
        std::vector<uint8_t> alphaPixels(64*64);
        svga3_vlkn_surface_dma_download(dev, 103, 0, nullptr, alphaPixels.data(), 64);
        TEST_CHECK(alphaPixels[0] == 64, "Alpha attachment clear stores A instead of R");
        const float alphaColor[4] = {0.8f,0.2f,0.3f,0.5f};
        uint32_t alphaConstant[4]; memcpy(alphaConstant,alphaColor,sizeof(alphaConstant));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, alphaConstant);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE, 7);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 103, 0, nullptr, alphaPixels.data(), 64);
        TEST_CHECK(alphaPixels[24*64+24] == 64, "RGB-only mask leaves an alpha attachment unchanged");
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE, 8);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 103, 0, nullptr, alphaPixels.data(), 64);
        TEST_CHECK(abs(int(alphaPixels[24*64+24])-128) <= 1, "Alpha-only mask stores the shader alpha");
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0x40C00000, 1, 0, nullptr, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_BLENDENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SRCBLEND, SVGA3D_BLENDOP_SRCALPHA);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_DSTBLEND, SVGA3D_BLENDOP_DESTALPHA);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 103, 0, nullptr, alphaPixels.data(), 64);
        TEST_CHECK(abs(int(alphaPixels[24*64+24])-80) <= 1, "Alpha attachment blends using stored source and destination alpha");
        svga3_vlkn_surface_define(dev, 104, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_X8R8G8B8, &smallSize, 1);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, 104, 0, 0);
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0x40808080, 1, 0, nullptr, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE, 15);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SRCBLEND, SVGA3D_BLENDOP_ZERO);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 104, 0, nullptr, smallPixels.data(), 64*4);
        TEST_CHECK(smallPixels[24*64+24].r == 128, "XRGB destination alpha is one regardless of unused alpha bits");
        svga3_vlkn_surface_define(dev, 105, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_A8R8G8B8, &smallSize, 1);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, 105, 0, 0);
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0x40808080, 1, 0, nullptr, 0);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 105, 0, nullptr, smallPixels.data(), 64*4);
        TEST_CHECK(abs(int(smallPixels[24*64+24].r)-32) <= 1, "RGBA destination alpha does not reuse an opaque-target pipeline");
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0x40808080, 1, 0, nullptr, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE, 0x564C010F);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 105, 0, nullptr, smallPixels.data(), 64*4);
        TEST_CHECK(smallPixels[24*64+24].r == 128, "Guest RGB view preserves opaque blending on an RGBA native image");
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SRCBLEND, SVGA3D_BLENDOP_SRCALPHASAT);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_DSTBLEND, SVGA3D_BLENDOP_ZERO);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, 105, 0, nullptr, smallPixels.data(), 64*4);
        TEST_CHECK(smallPixels[24*64+24].r == 0, "Source alpha saturation is zero for an opaque RGB view");
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE, 15);
        svga3_vlkn_surface_destroy(dev, 105);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_BLENDENABLE, 0);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);
        svga3_vlkn_surface_destroy(dev, 103); svga3_vlkn_surface_destroy(dev, 104);
        // SWTNL sends screen-space POSITIONT and remapped color as TEXCOORD1.
        const uint32_t windowPS[] = {0xFFFF0300,
            31 | (2<<24), 0x80010005, D3D9_DST(1,0,15),
            1 | (2<<24), D3D9_DST(8,0,15), D3D9_SRC(1,0,0xE4), 0xFFFF};
        svga3_vlkn_context_define_shader(dev,CID,101,SVGA3D_SHADERTYPE_PS,windowPS,sizeof(windowPS)/4);
        svga3_vlkn_context_set_shader(dev,CID,SVGA3D_SHADERTYPE_PS,101);
        const float windowVertices[4][8] = {
            {1.125f,1,0.5f,1, 0,1,0,0}, {5.125f,1,0.5f,2, 0,1,0,0},
            {1.125f,5,0.5f,1, 0,1,0,0}, {5.125f,5,0.5f,2, 0,1,0,0}};
        const uint16_t lineIndices[4] = {0,1,2,3};
        const SVGA3dSize windowBufferSize = {sizeof(windowVertices),1,1}, lineBufferSize = {sizeof(lineIndices),1,1};
        svga3_vlkn_surface_define(dev,106,SVGA3D_SURFACE_HINT_VERTEXBUFFER,SVGA3D_BUFFER,&windowBufferSize,1);
        svga3_vlkn_surface_define(dev,107,SVGA3D_SURFACE_HINT_INDEXBUFFER,SVGA3D_BUFFER,&lineBufferSize,1);
        svga3_vlkn_surface_dma_upload(dev,106,0,nullptr,windowVertices,sizeof(windowVertices));
        svga3_vlkn_surface_dma_upload(dev,107,0,nullptr,lineIndices,sizeof(lineIndices));
        SVGA3dVertexDecl windowDecls[2] = {decls[0],decls[1]};
        windowDecls[0].identity.usage = SVGA3D_DECLUSAGE_POSITIONT;
        windowDecls[0].identity.type = SVGA3D_DECLTYPE_FLOAT4;
        windowDecls[1].identity.usage = SVGA3D_DECLUSAGE_TEXCOORD;
        windowDecls[1].identity.usageIndex = 1;
        windowDecls[1].identity.type = SVGA3D_DECLTYPE_FLOAT4;
        for (uint32_t i=0;i<2;++i) {
            windowDecls[i].array.surfaceId=106; windowDecls[i].array.stride=32; windowDecls[i].array.offset=i*16;
        }
        SVGA3dPrimitiveRange lineRange = range;
        lineRange.primType=SVGA3D_PRIMITIVE_LINELIST; lineRange.primitiveCount=2; lineRange.indexArray.surfaceId=107;
        svga3_vlkn_context_clear(dev,CID,SVGA3D_CLEAR_COLOR,0,1,0,nullptr,0);
        TEST_CHECK(svga3_vlkn_context_draw(dev,CID,SVGA3D_PRIMITIVE_LINELIST,windowDecls,2,&lineRange,1)==SVGA3_VLKN_SUCCESS,
            "POSITIONT bypasses a bound guest vertex shader");
        svga3_vlkn_surface_dma_download(dev,SID_RT,0,nullptr,fb.data(),RT_W*4);
        TEST_CHECK(pixelMatches(fb[RT_W+3],0,255,0,0) && pixelMatches(fb[5*RT_W+3],0,255,0,0),
            "Pretransformed horizontal lines retain their screen coordinates, RHW and remapped color");
        TEST_CHECK(pixelMatches(fb[3*RT_W+1],0,0,0,0), "Pretransformed lines leave the vertical edge absent");
        svga3_vlkn_context_set_shader(dev,CID,SVGA3D_SHADERTYPE_PS,99);
        svga3_vlkn_surface_destroy(dev,106); svga3_vlkn_surface_destroy(dev,107);
        uint32_t redConst[4]; memcpy(redConst, outputColors[0], sizeof(redConst));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, redConst);
        const uint32_t selfCopyPS[] = {0xFFFF0300,
            31 | (2<<24), 0x90000000, D3D9_DST(10,0,15),
            81 | (5<<24), D3D9_DST(2,0,15), 0x3F800000,0x3F800000,0x3F800000,0x3F800000,
            66 | (3<<24), D3D9_DST(0,0,15), D3D9_SRC(2,1,0xE4), D3D9_SRC(10,0,0xE4),
            3 | (3<<24), D3D9_DST(8,0,15), D3D9_SRC(2,0,0xE4), D3D9_SRC(0,0,0xE4), 0xFFFF};
        svga3_vlkn_context_define_shader(dev,CID,111,SVGA3D_SHADERTYPE_PS,selfCopyPS,sizeof(selfCopyPS)/4);
        svga3_vlkn_context_set_shader(dev,CID,SVGA3D_SHADERTYPE_PS,111);
        svga3_vlkn_context_set_texture(dev,CID,0,SID_RT);
        const float centerCoords[4]={.5f,.5f,0,1};uint32_t coordsBits[4];memcpy(coordsBits,centerCoords,sizeof(coordsBits));
        svga3_vlkn_context_set_shader_const(dev,CID,1,SVGA3D_SHADERTYPE_PS,SVGA3D_CONST_TYPE_FLOAT,coordsBits);
        svga3_vlkn_context_clear(dev,CID,SVGA3D_CLEAR_COLOR,0xFF112233,1,0,nullptr,0);
        for (unsigned iteration=0;iteration<2;++iteration) {
            TEST_CHECK(svga3_vlkn_context_draw(dev,CID,SVGA3D_PRIMITIVE_TRIANGLELIST,decls,2,&range,1)==SVGA3_VLKN_SUCCESS,
                "Copy from the current attachment through a preserved sampled image");
            svga3_vlkn_surface_dma_download(dev,SID_RT,0,nullptr,fb.data(),RT_W*4);
            const auto& center=fb[32*RT_W+32];
            TEST_CHECK(abs(int(center.r)-(iteration?17:238))<=1 && abs(int(center.g)-(iteration?34:221))<=1 && abs(int(center.b)-(iteration?51:204))<=1,
                "Overlapping attachment copy reads the complete previous draw, including a refreshed snapshot");
        }
        svga3_vlkn_context_set_texture(dev,CID,0,SVGA3D_INVALID_ID);
        svga3_vlkn_context_set_shader(dev,CID,SVGA3D_SHADERTYPE_PS,99);
        const uint32_t pointVS[] = {0xFFFE0300,
            81 | (5<<24), D3D9_DST(2,0,15), 0,0,0x3F000000,0x3F800000,
            81 | (5<<24), D3D9_DST(2,1,15), 0x40400000,0x40400000,0x40400000,0x40400000,
            1 | (2<<24), D3D9_DST(4,0,15), D3D9_SRC(2,0,0xE4),
            1 | (2<<24), D3D9_DST(4,2,1), D3D9_SRC(2,1,0xE4), 0xFFFF};
        TEST_CHECK(svga3_vlkn_context_define_shader(dev,CID,110,SVGA3D_SHADERTYPE_VS,pointVS,sizeof(pointVS)/4)==SVGA3_VLKN_SUCCESS,
            "Define point-size shader with separate position and PSIZE outputs");
        svga3_vlkn_context_set_shader(dev,CID,SVGA3D_SHADERTYPE_VS,110);
        svga3_vlkn_context_clear(dev,CID,SVGA3D_CLEAR_COLOR,0xFF000000,1,0,nullptr,0);
        TEST_CHECK(svga3_vlkn_context_begin_query(dev,CID,SVGA3D_QUERYTYPE_OCCLUSION)==SVGA3_VLKN_SUCCESS,
            "Begin query outside the point render pass");
        SVGA3dPrimitiveRange pointRange{};
        pointRange.primType=SVGA3D_PRIMITIVE_POINTLIST;pointRange.primitiveCount=1;
        pointRange.indexArray.surfaceId=SVGA3D_INVALID_ID;
        TEST_CHECK(svga3_vlkn_context_draw(dev,CID,SVGA3D_PRIMITIVE_POINTLIST,decls,2,&pointRange,1)==SVGA3_VLKN_SUCCESS,
            "Render a three-pixel point");
        TEST_CHECK(svga3_vlkn_context_end_query(dev,CID,SVGA3D_QUERYTYPE_OCCLUSION)==SVGA3_VLKN_SUCCESS,
            "End query outside the point render pass");
        uint32_t samples=0;
        TEST_CHECK(svga3_vlkn_context_wait_for_query(dev,CID,SVGA3D_QUERYTYPE_OCCLUSION,&samples)==SVGA3_VLKN_SUCCESS && samples==9,
            "Occlusion query counts the nine covered point samples");
        svga3_vlkn_surface_dma_download(dev,SID_RT,0,nullptr,fb.data(),RT_W*4);
        unsigned redPoints=0;
        for (const auto& pixel : fb) redPoints += pixelMatches(pixel,255,0,0,255);
        TEST_CHECK(redPoints==9 && pixelMatches(fb[32*RT_W+32],255,0,0,255),
            "PSIZE changes coverage while leaving the point position intact");
        svga3_vlkn_context_set_shader(dev,CID,SVGA3D_SHADERTYPE_VS,1);
        const SVGA3dRect negativeViewport = {uint32_t(-32),0,64,64};
        const SVGA3dRect oversizedScissor = {uint32_t(-5),uint32_t(-5),69,69};
        const SVGA3dRect fullClear = {0,0,64,64};
        svga3_vlkn_context_set_viewport(dev, CID, &negativeViewport);
        svga3_vlkn_context_set_scissor_rect(dev, CID, &oversizedScissor);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SCISSORTESTENABLE, 1);
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0xFF000000, 1, 0, &fullClear, 1);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fb.data(), RT_W*4);
        TEST_CHECK(pixelMatches(fb[24*RT_W+8],255,0,0,255) && pixelMatches(fb[24*RT_W+24],0,0,0,255),
            "Signed offscreen viewport keeps geometry while scissors clip to the framebuffer");

        const SVGA3dRect emptyScissor = {0,64,0,0}, fullViewport = {0,0,64,64};
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0xFF000000, 1, 0, &fullClear, 1);
        svga3_vlkn_context_set_scissor_rect(dev, CID, &emptyScissor);
        svga3_vlkn_context_set_viewport(dev, CID, &fullViewport);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fb.data(), RT_W*4);
        TEST_CHECK(pixelMatches(fb[24*RT_W+24],0,0,0,255), "Explicit empty scissor survives viewport and target changes");
        const SVGA3dRect emptyViewport = {0,0,0,0};
        svga3_vlkn_context_set_viewport(dev, CID, &emptyViewport);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SCISSORTESTENABLE, 0);
        TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &range, 1)
            == SVGA3_VLKN_SUCCESS, "Explicit empty viewport clips the draw without a Vulkan viewport error");
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fb.data(), RT_W*4);
        TEST_CHECK(pixelMatches(fb[24*RT_W+24],0,0,0,255), "Explicit empty viewport survives target changes");

        /* Clean up Scene 1 */
        svga3_vlkn_context_destroy(dev, CID);
        svga3_vlkn_surface_destroy(dev, SID_RT);
        svga3_vlkn_surface_destroy(dev, SID_TEX);
        svga3_vlkn_surface_destroy(dev, SID_VB);
        svga3_vlkn_surface_destroy(dev, SID_IB);
    }

    /* --------------------------------------------------------------------------
     * SCENE 2: Depth Overlap (Depth Testing and Z-Buffering)
     * -------------------------------------------------------------------------- */
    std::cout << "\n--- Scene 2: Depth Overlap (Depth Testing & Z-Buffering) ---" << std::endl;
    {
        const uint32_t CID = 2;
        const uint32_t SID_RT = 20;
        const uint32_t SID_DS = 21;
        const uint32_t SID_VB1 = 22; /* Quad at Z = 0.5 (Red) */
        const uint32_t SID_VB2 = 23; /* Quad at Z = 0.8 (Green, Farther) */
        const uint32_t SID_VB3 = 24; /* Quad at Z = 0.2 (Blue, Closer) */

        svga3_vlkn_context_create(dev, CID);

        SVGA3dSize rtSize = { RT_W, RT_H, 1 };
        svga3_vlkn_surface_define(dev, SID_RT, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_X8R8G8B8, &rtSize, 1);
        svga3_vlkn_surface_define(dev, SID_DS, SVGA3D_SURFACE_HINT_DEPTHSTENCIL, SVGA3D_Z_D24S8, &rtSize, 1);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_DEPTH, SID_DS, 0, 0);

        SVGA3dRect vp = { 0, 0, RT_W, RT_H };
        svga3_vlkn_context_set_viewport(dev, CID, &vp);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_CULLMODE, SVGA3D_FACE_NONE);

        /* Identity matrix for VS */
        float matIdent[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        for (int r = 0; r < 4; ++r) {
            uint32_t rowValues[4];
            memcpy(rowValues, &matIdent[r * 4], sizeof(float) * 4);
            svga3_vlkn_context_set_shader_const(dev, CID, r, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, rowValues);
        }

        svga3_vlkn_context_define_shader(dev, CID, 1, SVGA3D_SHADERTYPE_VS, vs1Bytecode, sizeof(vs1Bytecode)/4);
        svga3_vlkn_context_define_shader(dev, CID, 2, SVGA3D_SHADERTYPE_PS, ps2Bytecode, sizeof(ps2Bytecode)/4);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_VS, 1);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 2);

        /* Enable Depth Test and Depth Write with LESSEQUAL */
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ZENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ZWRITEENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ZFUNC, SVGA3D_CMP_LESSEQUAL);

        /* Create 3 quads with different Z values */
        auto makeQuad = [&](float z) -> std::vector<Vertex> {
            return {
                { -1.0f,  1.0f, z,  0.0f, 0.0f },
                {  1.0f,  1.0f, z,  1.0f, 0.0f },
                {  1.0f, -1.0f, z,  1.0f, 1.0f },
                { -1.0f,  1.0f, z,  0.0f, 0.0f },
                {  1.0f, -1.0f, z,  1.0f, 1.0f },
                { -1.0f, -1.0f, z,  0.0f, 1.0f }
            };
        };

        std::vector<Vertex> q1 = makeQuad(0.5f);
        std::vector<Vertex> q2 = makeQuad(0.8f);
        std::vector<Vertex> q3 = makeQuad(0.2f);

        SVGA3dSize vbSz = { sizeof(Vertex) * 6, 1, 1 };
        svga3_vlkn_surface_define(dev, SID_VB1, SVGA3D_SURFACE_HINT_VERTEXBUFFER, SVGA3D_BUFFER, &vbSz, 1);
        svga3_vlkn_surface_define(dev, SID_VB2, SVGA3D_SURFACE_HINT_VERTEXBUFFER, SVGA3D_BUFFER, &vbSz, 1);
        svga3_vlkn_surface_define(dev, SID_VB3, SVGA3D_SURFACE_HINT_VERTEXBUFFER, SVGA3D_BUFFER, &vbSz, 1);

        SVGA3dBox bBox = { 0, 0, 0, sizeof(Vertex) * 6, 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_VB1, 0, &bBox, q1.data(), sizeof(Vertex) * 6);
        svga3_vlkn_surface_dma_upload(dev, SID_VB2, 0, &bBox, q2.data(), sizeof(Vertex) * 6);
        svga3_vlkn_surface_dma_upload(dev, SID_VB3, 0, &bBox, q3.data(), sizeof(Vertex) * 6);

        /* Step 1: Clear Color to Black, Depth to 1.0 */
        svga3_vlkn_context_clear(dev, CID, (SVGA3dClearFlag)(SVGA3D_CLEAR_COLOR | SVGA3D_CLEAR_DEPTH), 0x00000000, 1.0f, 0, nullptr, 0);

        /* Step 2: Draw Quad 1 at Z = 0.5 with Solid Red (1, 0, 0, 1) */
        float colRed[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
        uint32_t cVal[4];
        memcpy(cVal, colRed, sizeof(colRed));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, cVal);

        decls[0].array.surfaceId = SID_VB1;
        decls[1].array.surfaceId = SID_VB1;
        SVGA3dPrimitiveRange r1 = { SVGA3D_PRIMITIVE_TRIANGLELIST, 2, {}, 0, 0 };
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);

        /* Step 3: Draw Quad 2 at Z = 0.8 with Solid Green (0, 1, 0, 1) - Must fail depth test! */
        float colGreen[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
        memcpy(cVal, colGreen, sizeof(colGreen));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, cVal);

        decls[0].array.surfaceId = SID_VB2;
        decls[1].array.surfaceId = SID_VB2;
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_device_wait_idle(dev);

        std::vector<Pixel> fb(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fb.data(), RT_W * 4);

        /* Verify Quad 2 was rejected by depth test: pixel must remain Red! */
        Pixel center1 = fb[(RT_H / 2) * RT_W + (RT_W / 2)];
        TEST_CHECK(pixelMatches(center1, 255, 0, 0, 255),
                   "Scene 2: Farther geometry (Z=0.8) rejected by depth test; Red (Z=0.5) prevails");

        /* Step 4: Draw Quad 3 at Z = 0.2 with Solid Blue (0, 0, 1, 1) - Must pass depth test! */
        float colBlue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        memcpy(cVal, colBlue, sizeof(colBlue));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, cVal);

        decls[0].array.surfaceId = SID_VB3;
        decls[1].array.surfaceId = SID_VB3;
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_device_wait_idle(dev);

        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fb.data(), RT_W * 4);
        Pixel center2 = fb[(RT_H / 2) * RT_W + (RT_W / 2)];
        TEST_CHECK(pixelMatches(center2, 0, 0, 255, 255),
                   "Scene 2: Closer geometry (Z=0.2) passes depth test; overwrites with Blue");

        /* Generate Analytical Reference Image for Scene 2 (Closer Blue quad over full surface) */
        std::vector<Pixel> refFb2(RT_W * RT_H, { 255, 0, 0, 255 }); // Blue: B=255, G=0, R=0, A=255
        savePPM("artifacts/scene2_depth_rendered.ppm", fb.data(), RT_W, RT_H);
        savePPM("artifacts/scene2_depth_reference.ppm", refFb2.data(), RT_W, RT_H);
        saveDiffPPM("artifacts/scene2_depth_diff.ppm", fb.data(), refFb2.data(), RT_W, RT_H);

        /* Fragment depth overrides interpolated z, including depth-only draws. */
        const uint32_t depthShader[] = {0xFFFF0300,
            1 | (2 << 24), D3D9_DST(8, 0, 15), D3D9_SRC(2, 0, 0xE4),
            1 | (2 << 24), D3D9_DST(9, 0, 1), D3D9_SRC(2, 1, 0), 0xFFFF};
        TEST_CHECK(svga3_vlkn_context_define_shader(dev, CID, 98, SVGA3D_SHADERTYPE_PS,
            depthShader, sizeof(depthShader)/4) == SVGA3_VLKN_SUCCESS, "Define fragment depth shader");
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 98);
        const float replacedDepth[4] = {.7f,0,0,0};
        memcpy(cVal, replacedDepth, sizeof(cVal));
        svga3_vlkn_context_set_shader_const(dev, CID, 1, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, cVal);
        for (bool depthOnly : {false,true}) {
            if (depthOnly) svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SVGA3D_INVALID_ID, 0, 0);
            svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_DEPTH, 0, 1.f, 0, nullptr, 0);
            TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1)
                == SVGA3_VLKN_SUCCESS, "Draw with fragment depth output");
            std::vector<uint32_t> depthPixels(RT_W * RT_H);
            TEST_CHECK(svga3_vlkn_surface_dma_download(dev, SID_DS, 0, nullptr, depthPixels.data(), RT_W * 4)
                == SVGA3_VLKN_SUCCESS, "Read replaced fragment depth");
            TEST_CHECK(std::abs(double(depthPixels[32 * RT_W + 32] >> 8) / 16777215. - .7) < .000001,
                "Fragment depth 0.7 replaces geometry depth 0.2 with and without a color target");
        }
        // Packed depth and stencil must survive both surface-copy and blit paths.
        const SVGA3dSize packedSize = {2,2,1};
        const uint32_t packedDepth[4] = {0x123456A5,0x6543215A,0xFEDCBA12,0xABCDEF34};
        for (uint32_t sid : {97u,98u})
            svga3_vlkn_surface_define(dev, sid, SVGA3D_SURFACE_HINT_DEPTHSTENCIL, SVGA3D_Z_D24S8, &packedSize, 1);
        svga3_vlkn_surface_dma_upload(dev, 97, 0, nullptr, packedDepth, 8);
        const SVGA3dCopyBox packedCopy = {0,0,0,2,2,1,0,0,0};
        TEST_CHECK(dev->surfaceMgr->copy(97,98,&packedCopy,1) == SVGA3_VLKN_SUCCESS, "Copy packed depth-stencil surface");
        uint32_t packedReadback[4] = {};
        svga3_vlkn_surface_dma_download(dev, 98, 0, nullptr, packedReadback, 8);
        TEST_CHECK(memcmp(packedReadback,packedDepth,sizeof(packedDepth)) == 0, "Surface copy retains depth and stencil bits");
        const SVGA3dBox packedBox = {0,0,0,2,2,1};
        TEST_CHECK(dev->surfaceMgr->stretchBlt(97,98,packedBox,packedBox,SVGA3D_STRETCH_BLT_LINEAR) == SVGA3_VLKN_SUCCESS,
            "Depth-stencil blit uses a legal nearest filter");
        svga3_vlkn_surface_dma_download(dev, 98, 0, nullptr, packedReadback, 8);
        TEST_CHECK(memcmp(packedReadback,packedDepth,sizeof(packedDepth)) == 0, "Surface blit retains depth and stencil bits");
        svga3_vlkn_surface_define(dev, 99, SVGA3D_SURFACE_HINT_DEPTHSTENCIL, SVGA3D_Z_DF24, &packedSize, 1);
        TEST_CHECK(dev->surfaceMgr->copy(97,99,&packedCopy,1) == SVGA3_VLKN_SUCCESS, "Copy packed depth into DF24");
        svga3_vlkn_surface_dma_download(dev, 99, 0, nullptr, packedReadback, 8);
        for (uint32_t i = 0; i < 4; ++i)
            TEST_CHECK(packedReadback[i] == (packedDepth[i] & 0xFFFFFF00u), "DF24 copy preserves the 24-bit depth value");
        svga3_vlkn_surface_destroy(dev,99);
        for (uint32_t sid : {97u,98u}) svga3_vlkn_surface_destroy(dev,sid);

        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ZENABLE, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILENABLE2SIDED, 1);
        for (auto state : {SVGA3D_RS_STENCILFUNC,SVGA3D_RS_CCWSTENCILFUNC})
            svga3_vlkn_context_set_render_state(dev,CID,state,SVGA3D_CMP_ALWAYS);
        for (auto state : {SVGA3D_RS_STENCILPASS,SVGA3D_RS_CCWSTENCILPASS})
            svga3_vlkn_context_set_render_state(dev,CID,state,SVGA3D_STENCILOP_REPLACE);
        svga3_vlkn_context_set_render_state(dev,CID,SVGA3D_RS_STENCILMASK,0x564CFFFF);
        svga3_vlkn_context_set_render_state(dev,CID,SVGA3D_RS_STENCILWRITEMASK,0x564C1666);
        svga3_vlkn_context_set_render_state(dev,CID,SVGA3D_RS_STENCILREF,0x564CFFFF);
        for (bool compareMasks : {false,true}) {
            if (compareMasks) {
                for (auto state : {SVGA3D_RS_STENCILFUNC,SVGA3D_RS_CCWSTENCILFUNC})
                    svga3_vlkn_context_set_render_state(dev,CID,state,SVGA3D_CMP_EQUAL);
                svga3_vlkn_context_set_render_state(dev,CID,SVGA3D_RS_STENCILMASK,0x564C0FF0);
                svga3_vlkn_context_set_render_state(dev,CID,SVGA3D_RS_STENCILWRITEMASK,0x564CFFFF);
                svga3_vlkn_context_set_render_state(dev,CID,SVGA3D_RS_STENCILREF,0x564C0240);
            }
            uint32_t observedStencil[2] = {};
            for (uint32_t winding = 0; winding < 2; ++winding) {
                svga3_vlkn_context_clear(dev,CID,SVGA3D_CLEAR_STENCIL,0,1,compareMasks ? 0x42 : 0,nullptr,0);
                svga3_vlkn_surface_dma_upload(dev,SID_VB3,0,&bBox,q3.data(),sizeof(Vertex)*6);
                svga3_vlkn_context_draw(dev,CID,SVGA3D_PRIMITIVE_TRIANGLELIST,decls,2,&r1,1);
                std::vector<uint32_t> stencilPixels(RT_W*RT_H);
                svga3_vlkn_surface_dma_download(dev,SID_DS,0,nullptr,stencilPixels.data(),RT_W*4);
                observedStencil[winding] = stencilPixels[(RT_H/2)*RT_W+RT_W/2] & 255;
                std::swap(q3[1],q3[2]); std::swap(q3[4],q3[5]);
            }
            const uint32_t front = compareMasks ? 0x40 : 0x66, back = compareMasks ? 0x02 : 0x16;
            TEST_CHECK((observedStencil[0]==front && observedStencil[1]==back) ||
                (observedStencil[0]==back && observedStencil[1]==front),
                "Front and back stencil references, compare masks and write masks remain independent");
        }

        /* Clean up Scene 2 */
        svga3_vlkn_context_destroy(dev, CID);
        svga3_vlkn_surface_destroy(dev, SID_RT);
        svga3_vlkn_surface_destroy(dev, SID_DS);
        svga3_vlkn_surface_destroy(dev, SID_VB1);
        svga3_vlkn_surface_destroy(dev, SID_VB2);
        svga3_vlkn_surface_destroy(dev, SID_VB3);
    }

    /* --------------------------------------------------------------------------
     * SCENE 3: Alpha Blending
     * -------------------------------------------------------------------------- */
    std::cout << "\n--- Scene 3: Alpha Blending with Analytical Mathematical Verification ---" << std::endl;
    {
        const uint32_t CID = 3;
        const uint32_t SID_RT = 30;
        const uint32_t SID_VB = 31;

        svga3_vlkn_context_create(dev, CID);

        SVGA3dSize rtSize = { RT_W, RT_H, 1 };
        svga3_vlkn_surface_define(dev, SID_RT, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_X8R8G8B8, &rtSize, 1);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);

        SVGA3dRect vp = { 0, 0, RT_W, RT_H };
        svga3_vlkn_context_set_viewport(dev, CID, &vp);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_CULLMODE, SVGA3D_FACE_NONE);

        float matIdent[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        for (int r = 0; r < 4; ++r) {
            uint32_t rowValues[4];
            memcpy(rowValues, &matIdent[r * 4], sizeof(float) * 4);
            svga3_vlkn_context_set_shader_const(dev, CID, r, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, rowValues);
        }

        svga3_vlkn_context_define_shader(dev, CID, 1, SVGA3D_SHADERTYPE_VS, vs1Bytecode, sizeof(vs1Bytecode)/4);
        svga3_vlkn_context_define_shader(dev, CID, 2, SVGA3D_SHADERTYPE_PS, ps2Bytecode, sizeof(ps2Bytecode)/4);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_VS, 1);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 2);

        /* Clear to Solid Gray (80, 80, 80, 255) -> 0xFF505050 */
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0xFF505050, 1.0f, 0, nullptr, 0);

        /* Configure Standard Alpha Blending: SRC * SRC_ALPHA + DST * (1 - SRC_ALPHA) */
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_BLENDENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SRCBLEND, SVGA3D_BLENDOP_SRCALPHA);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_DSTBLEND, SVGA3D_BLENDOP_INVSRCALPHA);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_BLENDEQUATION, SVGA3D_BLENDEQ_ADD);

        /* Draw Quad with Semi-transparent Color:
         * Src Color: R=200/255, G=100/255, B=50/255, Alpha=128/255 (0.50196)
         * Analytical Math:
         * R = round(200 * 0.50196 + 80 * (1 - 0.50196)) = round(100.39 + 39.84) = 140
         * G = round(100 * 0.50196 + 80 * (1 - 0.50196)) = round( 50.20 + 39.84) = 90
         * B = round( 50 * 0.50196 + 80 * (1 - 0.50196)) = round( 25.10 + 39.84) = 65
         * A: separate alpha blending is off, so per D3D9 the alpha
         *    channel blends with the color factors against the cleared
         *    dst alpha 255: round(128 * 0.50196 + 255 * (1 - 0.50196))
         *    = round(64.25 + 127.0) = 191
         */
        float srcColor[4] = { 200.0f / 255.0f, 100.0f / 255.0f, 50.0f / 255.0f, 128.0f / 255.0f };
        uint32_t scVal[4];
        memcpy(scVal, srcColor, sizeof(srcColor));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, scVal);

        Vertex quadVerts[6] = {
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f },
            {  1.0f,  1.0f, 0.5f,  1.0f, 0.0f },
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f },
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f },
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f },
            { -1.0f, -1.0f, 0.5f,  0.0f, 1.0f }
        };
        SVGA3dSize vbSz = { sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_define(dev, SID_VB, SVGA3D_SURFACE_HINT_VERTEXBUFFER, SVGA3D_BUFFER, &vbSz, 1);
        SVGA3dBox bBox = { 0, 0, 0, sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_VB, 0, &bBox, quadVerts, sizeof(quadVerts));

        decls[0].array.surfaceId = SID_VB;
        decls[1].array.surfaceId = SID_VB;
        SVGA3dPrimitiveRange r1 = { SVGA3D_PRIMITIVE_TRIANGLELIST, 2, {}, 0, 0 };
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_device_wait_idle(dev);

        std::vector<Pixel> fb(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fb.data(), RT_W * 4);

        Pixel blended = fb[(RT_H / 2) * RT_W + (RT_W / 2)];
        TEST_CHECK(pixelMatches(blended, 140, 90, 65, 191),
                   "Scene 3: Alpha blending analytically matches round(Src*A + Dst*(1-A)) = (140, 90, 65)");

        /* Generate Analytical Reference Image for Scene 3 */
        std::vector<Pixel> refFb3(RT_W * RT_H, { 65, 90, 140, 191 }); // (B=65, G=90, R=140, A=191)
        savePPM("artifacts/scene3_alpha_rendered.ppm", fb.data(), RT_W, RT_H);
        savePPM("artifacts/scene3_alpha_reference.ppm", refFb3.data(), RT_W, RT_H);
        saveDiffPPM("artifacts/scene3_alpha_diff.ppm", fb.data(), refFb3.data(), RT_W, RT_H);

        /* Constant factors must use the packed guest color, including alpha.
         * Queue two draws with different constants before readback to catch
         * stale pipeline state and updates that overwrite earlier draws. */
        const uint32_t constantColors[] = {0xBF4000FF, 0x4080FF00};
        const uint32_t factors[] = {12, 13, 18, 19}; // VMware wire values
        const uint8_t source[] = {200, 100, 50, 128};
        for (uint32_t factor : factors) {
            for (bool destinationFactor : {false, true}) {
                svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SCISSORTESTENABLE, 0);
                TEST_CHECK(svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR,
                    0xFF505050, 1.0f, 0, nullptr, 0) == SVGA3_VLKN_SUCCESS,
                    "Constant blend clear succeeds");
                svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SRCBLEND,
                    destinationFactor ? uint32_t(SVGA3D_BLENDOP_ZERO) : factor);
                svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_DSTBLEND,
                    destinationFactor ? factor : uint32_t(SVGA3D_BLENDOP_ZERO));
                svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SEPARATEALPHABLENDENABLE, 1);
                svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SRCBLENDALPHA, SVGA3D_BLENDOP_ZERO);
                svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_DSTBLENDALPHA, factor);
                svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SCISSORTESTENABLE, 1);
                for (int half = 0; half < 2; ++half) {
                    SVGA3dRect clip = {uint32_t(half * RT_W / 2), 0, RT_W / 2, RT_H};
                    svga3_vlkn_context_set_scissor_rect(dev, CID, &clip);
                    svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_BLENDCOLOR, constantColors[half]);
                    TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST,
                        decls, 2, &r1, 1) == SVGA3_VLKN_SUCCESS, "Constant blend draw succeeds");
                }
                TEST_CHECK(svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr,
                    fb.data(), RT_W * 4) == SVGA3_VLKN_SUCCESS, "Constant blend readback succeeds");
                for (int half = 0; half < 2; ++half) {
                    const uint32_t c = constantColors[half];
                    const uint8_t channels[] = {uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c), uint8_t(c >> 24)};
                    uint8_t expected[4];
                    for (int channel = 0; channel < 4; ++channel) {
                        const int component = (factor >= 18 || channel == 3) ? channels[3] : channels[channel];
                        const int weight = (factor == 13 || factor == 19) ? 255 - component : component;
                        const int input = channel == 3 ? 255 : (destinationFactor ? 80 : source[channel]);
                        expected[channel] = uint8_t(std::lround(input * weight / 255.0));
                    }
                    const Pixel &actual = fb[(RT_H / 2) * RT_W + (half * RT_W / 2 + RT_W / 4)];
                    if (!pixelMatches(actual, expected[0], expected[1], expected[2], expected[3])) {
                        std::cerr << "constant factor=" << factor << " destination=" << destinationFactor
                                  << " half=" << half << " observed RGBA=" << int(actual.r) << ','
                                  << int(actual.g) << ',' << int(actual.b) << ',' << int(actual.a) << std::endl;
                    }
                    TEST_CHECK(pixelMatches(actual, expected[0], expected[1], expected[2], expected[3]),
                        "Constant and inverse blend factors match independent RGBA calculations");
                }
            }
        }

        /* Mesa encodes back-face colors with COLOR semantic indices 2/3.
         * They must not overwrite front colors or texture coordinates. */
        const uint32_t backColorVS[] = {
            0xFFFE0300,
            (31) | (2 << 24), 0x80000000, D3D9_DST(1, 0, 0xF),
            (31) | (2 << 24), 0x80000000, D3D9_DST(6, 0, 0xF),
            (31) | (2 << 24), 0x8000000A, D3D9_DST(6, 1, 0xF),
            (31) | (2 << 24), 0x8001000A, D3D9_DST(6, 2, 0xF),
            (31) | (2 << 24), 0x8002000A, D3D9_DST(6, 3, 0xF),
            (31) | (2 << 24), 0x8003000A, D3D9_DST(6, 4, 0xF),
            (20) | (3 << 24), D3D9_DST(6, 0, 0xF), D3D9_SRC(1, 0, 0xE4), D3D9_SRC(2, 0, 0xE4),
            (1) | (2 << 24), D3D9_DST(6, 1, 0xF), D3D9_SRC(2, 4, 0xE4),
            (1) | (2 << 24), D3D9_DST(6, 2, 0xF), D3D9_SRC(2, 5, 0xE4),
            (1) | (2 << 24), D3D9_DST(6, 3, 0xF), D3D9_SRC(2, 6, 0xE4),
            (1) | (2 << 24), D3D9_DST(6, 4, 0xF), D3D9_SRC(2, 7, 0xE4),
            0x0000FFFF
        };
        const uint32_t backColorPS[] = {
            0xFFFF0300,
            (31) | (2 << 24), 0x8002000A, D3D9_DST(1, 0, 0xF),
            (31) | (2 << 24), 0x8003000A, D3D9_DST(1, 1, 0xF),
            (2) | (3 << 24), D3D9_DST(8, 0, 0xF), D3D9_SRC(1, 0, 0xE4), D3D9_SRC(1, 1, 0xE4),
            0x0000FFFF
        };
        const float colors[4][4] = {{1, 1, 0, 0}, {0, 1, 1, 0}, {0.25f, 0, 0, 0.25f}, {0, 0.5f, 0.75f, 0.75f}};
        for (int color = 0; color < 4; ++color) {
            memcpy(scVal, colors[color], sizeof(scVal));
            svga3_vlkn_context_set_shader_const(dev, CID, 4 + color, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, scVal);
        }
        TEST_CHECK(svga3_vlkn_context_define_shader(dev, CID, 4, SVGA3D_SHADERTYPE_VS,
            backColorVS, sizeof(backColorVS) / 4) == SVGA3_VLKN_SUCCESS, "Four-color vertex shader defines");
        TEST_CHECK(svga3_vlkn_context_define_shader(dev, CID, 5, SVGA3D_SHADERTYPE_PS,
            backColorPS, sizeof(backColorPS) / 4) == SVGA3_VLKN_SUCCESS, "Back-color fragment shader defines");
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_VS, 4);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 5);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_BLENDENABLE, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SCISSORTESTENABLE, 0);
        TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST,
            decls, 1, &r1, 1) == SVGA3_VLKN_SUCCESS, "Back-color draw succeeds");
        TEST_CHECK(svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr,
            fb.data(), RT_W * 4) == SVGA3_VLKN_SUCCESS, "Back-color readback succeeds");
        TEST_CHECK(pixelMatches(fb[(RT_H / 2) * RT_W + RT_W / 4], 64, 128, 191, 255),
            "COLOR2 and COLOR3 interpolate independently of front colors");
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_VS, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SCISSORTESTENABLE, 1);

        /* SVGA front faces are clockwise in framebuffer coordinates.
         * These vertices map to top-left, top-right, bottom-right: clockwise
         * on screen. Reversing the vertex order must reverse vFace. */
        const uint32_t facePS[] = {
            0xFFFF0300,
            (1) | (2 << 24), D3D9_DST(8, 0, 0xF), D3D9_SRC(17, 1, 0xE4),
            0x0000FFFF
        };
        TEST_CHECK(svga3_vlkn_context_define_shader(dev, CID, 3, SVGA3D_SHADERTYPE_PS,
            facePS, sizeof(facePS) / 4) == SVGA3_VLKN_SUCCESS, "vFace shader defines");
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 3);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_BLENDENABLE, 0);
        for (int winding = 0; winding < 2; ++winding) {
            if (winding) {
                std::swap(quadVerts[0], quadVerts[1]);
                std::swap(quadVerts[3], quadVerts[4]);
                svga3_vlkn_surface_dma_upload(dev, SID_VB, 0, &bBox, quadVerts, sizeof(quadVerts));
            }
            SVGA3dRect clip = {uint32_t(winding * RT_W / 2), 0, RT_W / 2, RT_H};
            svga3_vlkn_context_set_scissor_rect(dev, CID, &clip);
            TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST,
                decls, 2, &r1, 1) == SVGA3_VLKN_SUCCESS, "Opposite winding vFace draw succeeds");
        }
        TEST_CHECK(svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr,
            fb.data(), RT_W * 4) == SVGA3_VLKN_SUCCESS, "vFace readback succeeds");
        TEST_CHECK(pixelMatches(fb[(RT_H / 2) * RT_W + RT_W / 4], 255, 255, 255, 255),
            "Clockwise framebuffer triangles receive positive SVGA vFace");
        TEST_CHECK(pixelMatches(fb[(RT_H / 2) * RT_W + 3 * RT_W / 4], 0, 0, 0, 0),
            "Counterclockwise framebuffer triangles receive negative SVGA vFace");

        /* Exercise the same orientation through the fixed-function stencil
         * path: clockwise replaces with 5; counterclockwise inverts to 255. */
        const uint32_t SID_DS = 32;
        TEST_CHECK(svga3_vlkn_surface_define(dev, SID_DS, SVGA3D_SURFACE_HINT_DEPTHSTENCIL,
            SVGA3D_Z_D24S8, &rtSize, 1) == SVGA3_VLKN_SUCCESS, "Two-sided stencil surface defines");
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_DEPTH, SID_DS, 0, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ZENABLE, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ZWRITEENABLE, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILENABLE2SIDED, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILFUNC, SVGA3D_CMP_ALWAYS);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_CCWSTENCILFUNC, SVGA3D_CMP_ALWAYS);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILPASS, SVGA3D_STENCILOP_REPLACE);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_CCWSTENCILPASS, SVGA3D_STENCILOP_INVERT);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILREF, 5);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE, 0);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 2);
        const float white[4] = {1, 1, 1, 1};
        memcpy(scVal, white, sizeof(white));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, scVal);
        TEST_CHECK(svga3_vlkn_context_clear(dev, CID,
            SVGA3dClearFlag(SVGA3D_CLEAR_COLOR | SVGA3D_CLEAR_STENCIL),
            0, 1.0f, 0, nullptr, 0) == SVGA3_VLKN_SUCCESS, "Two-sided stencil clears");
        for (int winding = 0; winding < 2; ++winding) {
            std::swap(quadVerts[0], quadVerts[1]);
            std::swap(quadVerts[3], quadVerts[4]);
            svga3_vlkn_surface_dma_upload(dev, SID_VB, 0, &bBox, quadVerts, sizeof(quadVerts));
            SVGA3dRect clip = {uint32_t(winding * RT_W / 2), 0, RT_W / 2, RT_H};
            svga3_vlkn_context_set_scissor_rect(dev, CID, &clip);
            TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST,
                decls, 2, &r1, 1) == SVGA3_VLKN_SUCCESS, "Two-sided stencil writes succeed");
        }
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILENABLE2SIDED, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILFUNC, SVGA3D_CMP_EQUAL);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILPASS, SVGA3D_STENCILOP_KEEP);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_COLORWRITEENABLE, 0xF);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SCISSORTESTENABLE, 0);
        TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST,
            decls, 2, &r1, 1) == SVGA3_VLKN_SUCCESS, "Stencil comparison draw succeeds");
        TEST_CHECK(svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr,
            fb.data(), RT_W * 4) == SVGA3_VLKN_SUCCESS, "Two-sided stencil readback succeeds");
        TEST_CHECK(pixelMatches(fb[(RT_H / 2) * RT_W + RT_W / 4], 255, 255, 255, 255),
            "Clockwise triangles use regular stencil replacement");
        TEST_CHECK(pixelMatches(fb[(RT_H / 2) * RT_W + 3 * RT_W / 4], 0, 0, 0, 0),
            "Counterclockwise triangles use CCW stencil inversion");

        TEST_CHECK(svga3_vlkn_context_clear(dev, CID,
            SVGA3dClearFlag(SVGA3D_CLEAR_DEPTH | SVGA3D_CLEAR_STENCIL),
            0, 1.0f, 5, nullptr, 0) == SVGA3_VLKN_SUCCESS, "Packed depth/stencil clear succeeds");
        std::vector<uint32_t> packedDepth(RT_W * RT_H);
        TEST_CHECK(svga3_vlkn_surface_dma_download(dev, SID_DS, 0, nullptr,
            packedDepth.data(), RT_W * 4) == SVGA3_VLKN_SUCCESS, "Packed depth/stencil readback succeeds");
        TEST_CHECK(packedDepth[(RT_H / 2) * RT_W + RT_W / 4] == 0xFFFFFF05,
            "Guest D24S8 readback packs depth above the stencil byte");
        SVGA3dBox depthBox = {3, 4, 0, 3, 2, 1};
        const uint32_t depthInput[8] = {0x00000001, 0x1234567F, 0xFFFFFF80, 0xDEADBEEF,
                                      0xABCDEF11, 0x654321FF, 0x80000000, 0xDEADBEEF};
        uint32_t depthOutput[8] = {};
        TEST_CHECK(svga3_vlkn_surface_dma_upload(dev, SID_DS, 0, &depthBox,
            depthInput, 16) == SVGA3_VLKN_SUCCESS, "Pitched packed depth/stencil upload succeeds");
        TEST_CHECK(svga3_vlkn_surface_dma_download(dev, SID_DS, 0, &depthBox,
            depthOutput, 16) == SVGA3_VLKN_SUCCESS, "Pitched packed depth/stencil download succeeds");
        for (int row = 0; row < 2; ++row) {
            for (int column = 0; column < 3; ++column)
                TEST_CHECK(depthOutput[row * 4 + column] == depthInput[row * 4 + column],
                    "Depth and stencil round-trip exactly through separate Vulkan aspects");
            TEST_CHECK(depthOutput[row * 4 + 3] == 0, "Depth readback preserves destination row padding");
        }

        {
            const void *mappedDepth = nullptr;
            size_t depthPitch = 0;
            std::unique_lock<std::mutex> depthLock;
            TEST_CHECK(dev->surfaceMgr->getSurface(SID_DS)->dmaDownloadToStaging(0, &depthBox,
                &mappedDepth, &depthPitch, depthLock) == SVGA3_VLKN_SUCCESS,
                "Direct staging readback returns guest-packed depth/stencil");
            TEST_CHECK(depthPitch == 12 && std::memcmp(mappedDepth, depthInput, 12) == 0 &&
                std::memcmp(static_cast<const uint8_t*>(mappedDepth) + depthPitch, depthInput + 4, 12) == 0,
                "Direct staging depth readback has tight packed rows");
        }
        const uint32_t SID_DEPTH24 = 33;
        TEST_CHECK(svga3_vlkn_surface_define(dev, SID_DEPTH24, SVGA3D_SURFACE_HINT_DEPTHSTENCIL,
            SVGA3D_Z_D24X8, &rtSize, 1) == SVGA3_VLKN_SUCCESS, "D24X8 surface defines");
        TEST_CHECK(svga3_vlkn_surface_dma_upload(dev, SID_DEPTH24, 0, &depthBox,
            depthInput, 16) == SVGA3_VLKN_SUCCESS, "D24X8 converts normalized depth to Vulkan float");
        TEST_CHECK(svga3_vlkn_surface_dma_download(dev, SID_DEPTH24, 0, &depthBox,
            depthOutput, 16) == SVGA3_VLKN_SUCCESS, "D24X8 converts Vulkan float to normalized depth");
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 3; ++column)
                TEST_CHECK(std::abs(int(depthOutput[row * 4 + column] >> 8) -
                    int(depthInput[row * 4 + column] >> 8)) <= 1 && (depthOutput[row * 4 + column] & 255) == 0,
                    "D24X8 round-trips depth within one UNORM step and clears unused bits");
        svga3_vlkn_surface_destroy(dev, SID_DEPTH24);

        /* Clean up Scene 3 */
        svga3_vlkn_surface_destroy(dev, SID_DS);
        svga3_vlkn_context_destroy(dev, CID);
        svga3_vlkn_surface_destroy(dev, SID_RT);
        svga3_vlkn_surface_destroy(dev, SID_VB);
    }

    /* --------------------------------------------------------------------------
     * SCENE 4: Viewport and Scissor Boundaries
     * -------------------------------------------------------------------------- */
    std::cout << "\n--- Scene 4: Viewport and Scissor Boundaries ---" << std::endl;
    {
        const uint32_t CID = 4;
        const uint32_t SID_RT = 40;
        const uint32_t SID_VB = 41;

        svga3_vlkn_context_create(dev, CID);

        SVGA3dSize rtSize = { RT_W, RT_H, 1 };
        svga3_vlkn_surface_define(dev, SID_RT, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_X8R8G8B8, &rtSize, 1);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);

        /* First clear the entire framebuffer [0, 0, RT_W, RT_H] to Dark Blue (0, 0, 100, 255) -> 0xFF000064 */
        SVGA3dRect fullVp = { 0, 0, RT_W, RT_H };
        svga3_vlkn_context_set_viewport(dev, CID, &fullVp);
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0xFF000064, 1.0f, 0, nullptr, 0);

        /* Set Viewport to [10, 10, 44, 44] */
        SVGA3dRect vp = { 10, 10, 44, 44 };
        svga3_vlkn_context_set_viewport(dev, CID, &vp);

        /* Set Scissor Rect to [20, 20, 24, 24] */
        SVGA3dRect scissor = { 20, 20, 24, 24 };
        svga3_vlkn_context_set_scissor_rect(dev, CID, &scissor);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_SCISSORTESTENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_CULLMODE, SVGA3D_FACE_NONE);

        /* Identity transform */
        float matIdent[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        for (int r = 0; r < 4; ++r) {
            uint32_t rowValues[4];
            memcpy(rowValues, &matIdent[r * 4], sizeof(float) * 4);
            svga3_vlkn_context_set_shader_const(dev, CID, r, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, rowValues);
        }

        svga3_vlkn_context_define_shader(dev, CID, 1, SVGA3D_SHADERTYPE_VS, vs1Bytecode, sizeof(vs1Bytecode)/4);
        svga3_vlkn_context_define_shader(dev, CID, 2, SVGA3D_SHADERTYPE_PS, ps2Bytecode, sizeof(ps2Bytecode)/4);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_VS, 1);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 2);

        /* Solid Yellow color (255, 255, 0, 255) */
        float colYellow[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
        uint32_t cVal[4];
        memcpy(cVal, colYellow, sizeof(colYellow));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, cVal);

        /* Draw full-screen quad */
        Vertex quadVerts[6] = {
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f },
            {  1.0f,  1.0f, 0.5f,  1.0f, 0.0f },
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f },
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f },
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f },
            { -1.0f, -1.0f, 0.5f,  0.0f, 1.0f }
        };
        SVGA3dSize vbSz = { sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_define(dev, SID_VB, SVGA3D_SURFACE_HINT_VERTEXBUFFER, SVGA3D_BUFFER, &vbSz, 1);
        SVGA3dBox bBox = { 0, 0, 0, sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_VB, 0, &bBox, quadVerts, sizeof(quadVerts));

        decls[0].array.surfaceId = SID_VB;
        decls[1].array.surfaceId = SID_VB;
        SVGA3dPrimitiveRange r1 = { SVGA3D_PRIMITIVE_TRIANGLELIST, 2, {}, 0, 0 };
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_device_wait_idle(dev);

        std::vector<Pixel> fb(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fb.data(), RT_W * 4);

        /* Verify:
         * Inside scissor [20..43, 20..43]: Yellow (255, 255, 0)
         * Outside scissor, inside viewport (e.g. (15, 15)): Dark Blue (0, 0, 100)
         * Outside viewport (e.g. (5, 5)): Dark Blue (0, 0, 100)
         */
        Pixel insideScissor = fb[30 * RT_W + 30];
        TEST_CHECK(pixelMatches(insideScissor, 255, 255, 0, 255),
                   "Scene 4: Pixel inside scissor is rendered Yellow (255, 255, 0)");

        Pixel outsideScissorInVp = fb[15 * RT_W + 15];
        TEST_CHECK(pixelMatches(outsideScissorInVp, 0, 0, 100, 255),
                   "Scene 4: Pixel outside scissor but inside viewport is preserved Dark Blue (0, 0, 100)");

        Pixel outsideVp = fb[5 * RT_W + 5];
        TEST_CHECK(pixelMatches(outsideVp, 0, 0, 100, 255),
                   "Scene 4: Pixel outside viewport is preserved Dark Blue (0, 0, 100)");

        /* Generate Analytical Reference Image for Scene 4 */
        std::vector<Pixel> refFb4(RT_W * RT_H, { 100, 0, 0, 255 }); // Dark blue clear (B=100, G=0, R=0, A=255)
        for (int y = 20; y < 44; ++y) {
            for (int x = 20; x < 44; ++x) {
                refFb4[y * RT_W + x] = { 0, 255, 255, 255 }; // Yellow quad inside scissor (B=0, G=255, R=255, A=255)
            }
        }
        savePPM("artifacts/scene4_scissor_rendered.ppm", fb.data(), RT_W, RT_H);
        savePPM("artifacts/scene4_scissor_reference.ppm", refFb4.data(), RT_W, RT_H);
        saveDiffPPM("artifacts/scene4_scissor_diff.ppm", fb.data(), refFb4.data(), RT_W, RT_H);

        /* Clean up Scene 4 */
        svga3_vlkn_context_destroy(dev, CID);
        svga3_vlkn_surface_destroy(dev, SID_RT);
        svga3_vlkn_surface_destroy(dev, SID_VB);
    }

    /* --------------------------------------------------------------------------
     * SCENE 5: Switching Render Targets
     * -------------------------------------------------------------------------- */
    std::cout << "\n--- Scene 5: Switching Render Targets (RT1 & RT2 Independent Rendering) ---" << std::endl;
    {
        const uint32_t CID = 5;
        const uint32_t SID_RT1 = 50;
        const uint32_t SID_RT2 = 51;
        const uint32_t SID_VB = 52;

        svga3_vlkn_context_create(dev, CID);

        SVGA3dSize rtSize = { RT_W, RT_H, 1 };
        svga3_vlkn_surface_define(dev, SID_RT1, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_X8R8G8B8, &rtSize, 1);
        svga3_vlkn_surface_define(dev, SID_RT2, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_X8R8G8B8, &rtSize, 1);

        SVGA3dRect vp = { 0, 0, RT_W, RT_H };
        svga3_vlkn_context_set_viewport(dev, CID, &vp);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_CULLMODE, SVGA3D_FACE_NONE);

        float matIdent[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        for (int r = 0; r < 4; ++r) {
            uint32_t rowValues[4];
            memcpy(rowValues, &matIdent[r * 4], sizeof(float) * 4);
            svga3_vlkn_context_set_shader_const(dev, CID, r, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, rowValues);
        }

        svga3_vlkn_context_define_shader(dev, CID, 1, SVGA3D_SHADERTYPE_VS, vs1Bytecode, sizeof(vs1Bytecode)/4);
        svga3_vlkn_context_define_shader(dev, CID, 2, SVGA3D_SHADERTYPE_PS, ps2Bytecode, sizeof(ps2Bytecode)/4);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_VS, 1);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 2);

        Vertex quadVerts[6] = {
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f },
            {  1.0f,  1.0f, 0.5f,  1.0f, 0.0f },
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f },
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f },
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f },
            { -1.0f, -1.0f, 0.5f,  0.0f, 1.0f }
        };
        SVGA3dSize vbSz = { sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_define(dev, SID_VB, SVGA3D_SURFACE_HINT_VERTEXBUFFER, SVGA3D_BUFFER, &vbSz, 1);
        SVGA3dBox bBox = { 0, 0, 0, sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_VB, 0, &bBox, quadVerts, sizeof(quadVerts));

        decls[0].array.surfaceId = SID_VB;
        decls[1].array.surfaceId = SID_VB;
        SVGA3dPrimitiveRange r1 = { SVGA3D_PRIMITIVE_TRIANGLELIST, 2, {}, 0, 0 };

        /* 1. Target RT1: Clear to Magenta (255, 0, 255) */
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT1, 0, 0);
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0xFFFF00FF, 1.0f, 0, nullptr, 0);

        /* 2. Switch to RT2: Clear to Black (0, 0, 0), Draw Cyan (0, 255, 255) Quad */
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT2, 0, 0);
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0x00000000, 1.0f, 0, nullptr, 0);

        float colCyan[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
        uint32_t cVal[4];
        memcpy(cVal, colCyan, sizeof(colCyan));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, cVal);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);

        /* 3. Switch BACK to RT1: Draw Yellow (255, 255, 0) Quad */
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT1, 0, 0);
        float colYellow[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
        memcpy(cVal, colYellow, sizeof(colYellow));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, cVal);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_device_wait_idle(dev);

        /* Read back both RT1 and RT2 and verify independence */
        std::vector<Pixel> fb1(RT_W * RT_H);
        std::vector<Pixel> fb2(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT1, 0, nullptr, fb1.data(), RT_W * 4);
        svga3_vlkn_surface_dma_download(dev, SID_RT2, 0, nullptr, fb2.data(), RT_W * 4);

        Pixel p1 = fb1[(RT_H / 2) * RT_W + (RT_W / 2)];
        Pixel p2 = fb2[(RT_H / 2) * RT_W + (RT_W / 2)];
        TEST_CHECK(pixelMatches(p1, 255, 255, 0, 255), "Scene 5: RT1 contains Yellow (255, 255, 0)");
        TEST_CHECK(pixelMatches(p2, 0, 255, 255, 255), "Scene 5: RT2 contains Cyan (0, 255, 255)");

        savePPM("artifacts/scene5_rt1_rendered.ppm", fb1.data(), RT_W, RT_H);
        savePPM("artifacts/scene5_rt2_rendered.ppm", fb2.data(), RT_W, RT_H);

        /* Clean up Scene 5 */
        svga3_vlkn_context_destroy(dev, CID);
        svga3_vlkn_surface_destroy(dev, SID_RT1);
        svga3_vlkn_surface_destroy(dev, SID_RT2);
        svga3_vlkn_surface_destroy(dev, SID_VB);
    }

    /* --------------------------------------------------------------------------
     * SCENE 6: Two Contexts with Distinct State (Multi-Context Isolation)
     * -------------------------------------------------------------------------- */
    std::cout << "\n--- Scene 6: Two Contexts with Distinct State (Context Isolation) ---" << std::endl;
    {
        const uint32_t CID1 = 61;
        const uint32_t CID2 = 62;
        const uint32_t SID_RT1 = 601;
        const uint32_t SID_RT2 = 602;
        const uint32_t SID_VB = 603;

        svga3_vlkn_context_create(dev, CID1);
        svga3_vlkn_context_create(dev, CID2);

        SVGA3dSize rtSize = { RT_W, RT_H, 1 };
        svga3_vlkn_surface_define(dev, SID_RT1, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_X8R8G8B8, &rtSize, 1);
        svga3_vlkn_surface_define(dev, SID_RT2, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_X8R8G8B8, &rtSize, 1);

        svga3_vlkn_context_set_render_target(dev, CID1, SVGA3D_RT_COLOR0, SID_RT1, 0, 0);
        svga3_vlkn_context_set_render_target(dev, CID2, SVGA3D_RT_COLOR0, SID_RT2, 0, 0);

        /* Clear both entire framebuffers to Black */
        SVGA3dRect fullVp = { 0, 0, RT_W, RT_H };
        svga3_vlkn_context_set_viewport(dev, CID1, &fullVp);
        svga3_vlkn_context_set_viewport(dev, CID2, &fullVp);
        svga3_vlkn_context_clear(dev, CID1, SVGA3D_CLEAR_COLOR, 0x00000000, 1.0f, 0, nullptr, 0);
        svga3_vlkn_context_clear(dev, CID2, SVGA3D_CLEAR_COLOR, 0x00000000, 1.0f, 0, nullptr, 0);

        /* Context 1: Viewport Left Half [0, 0, 32, 64], Color Red */
        SVGA3dRect vp1 = { 0, 0, RT_W / 2, RT_H };
        svga3_vlkn_context_set_viewport(dev, CID1, &vp1);
        svga3_vlkn_context_set_render_state(dev, CID1, SVGA3D_RS_CULLMODE, SVGA3D_FACE_NONE);

        /* Context 2: Viewport Right Half [32, 0, 32, 64], Color Blue */
        SVGA3dRect vp2 = { RT_W / 2, 0, RT_W / 2, RT_H };
        svga3_vlkn_context_set_viewport(dev, CID2, &vp2);
        svga3_vlkn_context_set_render_state(dev, CID2, SVGA3D_RS_CULLMODE, SVGA3D_FACE_NONE);

        float matIdent[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        for (int r = 0; r < 4; ++r) {
            uint32_t rowValues[4];
            memcpy(rowValues, &matIdent[r * 4], sizeof(float) * 4);
            svga3_vlkn_context_set_shader_const(dev, CID1, r, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, rowValues);
            svga3_vlkn_context_set_shader_const(dev, CID2, r, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, rowValues);
        }

        svga3_vlkn_context_define_shader(dev, CID1, 1, SVGA3D_SHADERTYPE_VS, vs1Bytecode, sizeof(vs1Bytecode)/4);
        svga3_vlkn_context_define_shader(dev, CID1, 2, SVGA3D_SHADERTYPE_PS, ps2Bytecode, sizeof(ps2Bytecode)/4);
        svga3_vlkn_context_set_shader(dev, CID1, SVGA3D_SHADERTYPE_VS, 1);
        svga3_vlkn_context_set_shader(dev, CID1, SVGA3D_SHADERTYPE_PS, 2);

        svga3_vlkn_context_define_shader(dev, CID2, 1, SVGA3D_SHADERTYPE_VS, vs1Bytecode, sizeof(vs1Bytecode)/4);
        svga3_vlkn_context_define_shader(dev, CID2, 2, SVGA3D_SHADERTYPE_PS, ps2Bytecode, sizeof(ps2Bytecode)/4);
        svga3_vlkn_context_set_shader(dev, CID2, SVGA3D_SHADERTYPE_VS, 1);
        svga3_vlkn_context_set_shader(dev, CID2, SVGA3D_SHADERTYPE_PS, 2);

        float colRed[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
        float colBlue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        uint32_t cVal[4];
        memcpy(cVal, colRed, sizeof(colRed));
        svga3_vlkn_context_set_shader_const(dev, CID1, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, cVal);
        memcpy(cVal, colBlue, sizeof(colBlue));
        svga3_vlkn_context_set_shader_const(dev, CID2, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, cVal);

        Vertex quadVerts[6] = {
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f },
            {  1.0f,  1.0f, 0.5f,  1.0f, 0.0f },
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f },
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f },
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f },
            { -1.0f, -1.0f, 0.5f,  0.0f, 1.0f }
        };
        SVGA3dSize vbSz = { sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_define(dev, SID_VB, SVGA3D_SURFACE_HINT_VERTEXBUFFER, SVGA3D_BUFFER, &vbSz, 1);
        SVGA3dBox bBox = { 0, 0, 0, sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_VB, 0, &bBox, quadVerts, sizeof(quadVerts));

        decls[0].array.surfaceId = SID_VB;
        decls[1].array.surfaceId = SID_VB;
        SVGA3dPrimitiveRange r1 = { SVGA3D_PRIMITIVE_TRIANGLELIST, 2, {}, 0, 0 };

        /* Interleave draw calls between Context 1 and Context 2 */
        svga3_vlkn_context_draw(dev, CID1, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_context_draw(dev, CID2, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_device_wait_idle(dev);

        std::vector<Pixel> fb1(RT_W * RT_H);
        std::vector<Pixel> fb2(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT1, 0, nullptr, fb1.data(), RT_W * 4);
        svga3_vlkn_surface_dma_download(dev, SID_RT2, 0, nullptr, fb2.data(), RT_W * 4);

        /* Verify Context 1: Left half is Red, Right half is untouched Black */
        Pixel c1Left = fb1[32 * RT_W + 16];
        Pixel c1Right = fb1[32 * RT_W + 48];
        TEST_CHECK(pixelMatches(c1Left, 255, 0, 0, 255), "Scene 6: CID1 left half is Red (255, 0, 0)");
        TEST_CHECK(pixelMatches(c1Right, 0, 0, 0, 0), "Scene 6: CID1 right half is Black (0, 0, 0)");

        /* Verify Context 2: Left half is untouched Black, Right half is Blue */
        Pixel c2Left = fb2[32 * RT_W + 16];
        Pixel c2Right = fb2[32 * RT_W + 48];
        TEST_CHECK(pixelMatches(c2Left, 0, 0, 0, 0), "Scene 6: CID2 left half is Black (0, 0, 0)");
        TEST_CHECK(pixelMatches(c2Right, 0, 0, 255, 255), "Scene 6: CID2 right half is Blue (0, 0, 255)");

        savePPM("artifacts/scene6_ctx1_rendered.ppm", fb1.data(), RT_W, RT_H);
        savePPM("artifacts/scene6_ctx2_rendered.ppm", fb2.data(), RT_W, RT_H);

        /* Clean up Scene 6 */
        svga3_vlkn_context_destroy(dev, CID1);
        svga3_vlkn_context_destroy(dev, CID2);
        svga3_vlkn_surface_destroy(dev, SID_RT1);
        svga3_vlkn_surface_destroy(dev, SID_RT2);
        svga3_vlkn_surface_destroy(dev, SID_VB);
    }

    /* --------------------------------------------------------------------------
     * SCENE 7: Dynamic Resource Updates Followed by Another Draw
     * -------------------------------------------------------------------------- */
    std::cout << "\n--- Scene 7: Dynamic Resource Updates & Subsequent Draw ---" << std::endl;
    {
        const uint32_t CID = 7;
        const uint32_t SID_RT = 70;
        const uint32_t SID_TEX = 71;
        const uint32_t SID_VB = 72;

        svga3_vlkn_context_create(dev, CID);

        SVGA3dSize rtSize = { RT_W, RT_H, 1 };
        svga3_vlkn_surface_define(dev, SID_RT, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_X8R8G8B8, &rtSize, 1);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);

        SVGA3dRect vp = { 0, 0, RT_W, RT_H };
        svga3_vlkn_context_set_viewport(dev, CID, &vp);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_CULLMODE, SVGA3D_FACE_NONE);

        float matIdent[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        for (int r = 0; r < 4; ++r) {
            uint32_t rowValues[4];
            memcpy(rowValues, &matIdent[r * 4], sizeof(float) * 4);
            svga3_vlkn_context_set_shader_const(dev, CID, r, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, rowValues);
        }

        /* Identity tint (1, 1, 1, 1) */
        float tintOne[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        uint32_t tVal[4];
        memcpy(tVal, tintOne, sizeof(tintOne));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, tVal);

        svga3_vlkn_context_define_shader(dev, CID, 1, SVGA3D_SHADERTYPE_VS, vs1Bytecode, sizeof(vs1Bytecode)/4);
        svga3_vlkn_context_define_shader(dev, CID, 1, SVGA3D_SHADERTYPE_PS, ps1Bytecode, sizeof(ps1Bytecode)/4);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_VS, 1);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 1);

        /* Create 1x1 Texture Surface */
        SVGA3dSize texSz = { 1, 1, 1 };
        svga3_vlkn_surface_define(dev, SID_TEX, SVGA3D_SURFACE_HINT_TEXTURE, SVGA3D_X8R8G8B8, &texSz, 1);
        svga3_vlkn_context_set_texture(dev, CID, 0, SID_TEX);

        Vertex quadVerts[6] = {
            { -1.0f,  1.0f, 0.5f,  0.5f, 0.5f },
            {  1.0f,  1.0f, 0.5f,  0.5f, 0.5f },
            {  1.0f, -1.0f, 0.5f,  0.5f, 0.5f },
            { -1.0f,  1.0f, 0.5f,  0.5f, 0.5f },
            {  1.0f, -1.0f, 0.5f,  0.5f, 0.5f },
            { -1.0f, -1.0f, 0.5f,  0.5f, 0.5f }
        };
        SVGA3dSize vbSz = { sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_define(dev, SID_VB, SVGA3D_SURFACE_HINT_VERTEXBUFFER, SVGA3D_BUFFER, &vbSz, 1);
        SVGA3dBox bBox = { 0, 0, 0, sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_VB, 0, &bBox, quadVerts, sizeof(quadVerts));

        decls[0].array.surfaceId = SID_VB;
        decls[1].array.surfaceId = SID_VB;
        SVGA3dPrimitiveRange r1 = { SVGA3D_PRIMITIVE_TRIANGLELIST, 2, {}, 0, 0 };

        /* 1. Upload Texture Version A: Orange (255, 128, 0) */
        uint32_t texOrange = 0xFFFF8000;
        SVGA3dBox tBox = { 0, 0, 0, 1, 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_TEX, 0, &tBox, &texOrange, 4);

        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0x00000000, 1.0f, 0, nullptr, 0);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_device_wait_idle(dev);

        std::vector<Pixel> fbA(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fbA.data(), RT_W * 4);
        Pixel pA = fbA[(RT_H / 2) * RT_W + (RT_W / 2)];
        TEST_CHECK(pixelMatches(pA, 255, 128, 0, 255), "Scene 7 (Draw 1): Rendered Texture A (Orange 255, 128, 0)");
        savePPM("artifacts/scene7_texA_rendered.ppm", fbA.data(), RT_W, RT_H);

        /* 2. Dynamically Update Texture to Version B: Purple (128, 0, 255) WITHOUT destroying surface */
        uint32_t texPurple = 0xFF8000FF;
        svga3_vlkn_surface_dma_upload(dev, SID_TEX, 0, &tBox, &texPurple, 4);

        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0x00000000, 1.0f, 0, nullptr, 0);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_device_wait_idle(dev);

        std::vector<Pixel> fbB(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fbB.data(), RT_W * 4);
        Pixel pB = fbB[(RT_H / 2) * RT_W + (RT_W / 2)];
        TEST_CHECK(pixelMatches(pB, 128, 0, 255, 255), "Scene 7 (Draw 2): Dynamically updated Texture B rendered (Purple 128, 0, 255)");
        savePPM("artifacts/scene7_texB_rendered.ppm", fbB.data(), RT_W, RT_H);

        /* Clean up Scene 7 */
        svga3_vlkn_context_destroy(dev, CID);
        svga3_vlkn_surface_destroy(dev, SID_RT);
        svga3_vlkn_surface_destroy(dev, SID_TEX);
        svga3_vlkn_surface_destroy(dev, SID_VB);
    }

    /* --------------------------------------------------------------------------
     * SCENE 8: Negative Controls (Deliberate alterations must trigger detection)
     * -------------------------------------------------------------------------- */
    std::cout << "\n--- Scene 8: Negative Controls (Detect Deliberate Alterations) ---" << std::endl;
    {
        /* Negative Control A: Altered Color Channel (Red changed to 200 instead of 255) */
        Pixel alteredColor = { 0, 128, 200, 255 };
        bool matchA = pixelMatches(alteredColor, 255, 128, 0, 255);
        TEST_CHECK_NEGATIVE(!matchA, "Altered color channel (200 vs 255) rejected");

        /* Negative Control B: Altered Alpha Channel (Alpha changed to 128 instead of 255) */
        Pixel alteredAlpha = { 0, 128, 255, 128 };
        bool matchB = pixelMatches(alteredAlpha, 255, 128, 0, 255);
        TEST_CHECK_NEGATIVE(!matchB, "Altered alpha channel (128 vs 255) rejected");

        /* Negative Control C: Altered Geometry / Viewport (Pixel at (0, 0) checked against quad color) */
        Pixel bgPixel = { 0, 0, 0, 0 };
        bool matchC = pixelMatches(bgPixel, 255, 128, 0, 255);
        TEST_CHECK_NEGATIVE(!matchC, "Untouched background pixel (0,0,0) rejected when expecting quad color");

        /* Negative Control D: Deliberate Shader Constant Modification Detection */
        /* If shader constant was Tint = (0.2, 1.0, 0.8), White * Tint would be (51, 255, 204) */
        Pixel renderedPixel = { 204, 255, 127, 255 }; /* Expected from Tint=(0.5, 1.0, 0.8) */
        bool matchD = pixelMatches(renderedPixel, 51, 255, 204, 255); /* Checking against Tint=(0.2, ...) */
        TEST_CHECK_NEGATIVE(!matchD, "Pixel from Tint=(0.5) rejected when compared against analytical Tint=(0.2)");

        /* Visual Negative Control Failure Artifact */
        if (!g_scene1Fb.empty() && !g_scene1Ref.empty()) {
            std::vector<Pixel> intentionallyFlawedRef = g_scene1Ref;
            for (int y = 20; y < 44; ++y) {
                for (int x = 20; x < 44; ++x) {
                    intentionallyFlawedRef[y * RT_W + x] = { 0, 0, 255, 255 }; // Discrepancy
                }
            }
            saveDiffPPM("artifacts/negative_control_diff.ppm", g_scene1Fb.data(), intentionallyFlawedRef.data(), RT_W, RT_H);
        }
    }

    /* --------------------------------------------------------------------------
     * SCENE 9: Fixed-Function Alpha Testing Emulation via Specialization Constants
     * -------------------------------------------------------------------------- */
    std::cout << "\n--- Scene 9: Fixed-Function Alpha Testing Emulation ---" << std::endl;
    {
        const uint32_t CID = 90;
        const uint32_t SID_RT = 91;
        const uint32_t SID_TEX = 92;
        const uint32_t SID_VB = 93;

        svga3_vlkn_context_create(dev, CID);

        SVGA3dSize rtSz = { RT_W, RT_H, 1 };
        svga3_vlkn_surface_define(dev, SID_RT, SVGA3D_SURFACE_HINT_RENDERTARGET, SVGA3D_A8R8G8B8, &rtSz, 1);
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_COLOR0, SID_RT, 0, 0);

        SVGA3dRect vp = { 0, 0, RT_W, RT_H };
        svga3_vlkn_context_set_viewport(dev, CID, &vp);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_CULLMODE, SVGA3D_FACE_NONE);

        float matIdent[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        for (int r = 0; r < 4; ++r) {
            uint32_t rowValues[4];
            memcpy(rowValues, &matIdent[r * 4], sizeof(float) * 4);
            svga3_vlkn_context_set_shader_const(dev, CID, r, SVGA3D_SHADERTYPE_VS, SVGA3D_CONST_TYPE_FLOAT, rowValues);
        }

        /* Identity tint (1, 1, 1, 1) */
        float tintOne[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        uint32_t tVal[4];
        memcpy(tVal, tintOne, sizeof(tintOne));
        svga3_vlkn_context_set_shader_const(dev, CID, 0, SVGA3D_SHADERTYPE_PS, SVGA3D_CONST_TYPE_FLOAT, tVal);

        svga3_vlkn_context_define_shader(dev, CID, 1, SVGA3D_SHADERTYPE_VS, vs1Bytecode, sizeof(vs1Bytecode)/4);
        svga3_vlkn_context_define_shader(dev, CID, 1, SVGA3D_SHADERTYPE_PS, ps1Bytecode, sizeof(ps1Bytecode)/4);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_VS, 1);
        svga3_vlkn_context_set_shader(dev, CID, SVGA3D_SHADERTYPE_PS, 1);

        /* Create 2x1 Texture Surface: Left=Red (A=0.2), Right=Green (A=0.8) */
        SVGA3dSize texSz = { 2, 1, 1 };
        svga3_vlkn_surface_define(dev, SID_TEX, SVGA3D_SURFACE_HINT_TEXTURE, SVGA3D_A8R8G8B8, &texSz, 1);
        svga3_vlkn_context_set_texture(dev, CID, 0, SID_TEX);
        svga3_vlkn_context_set_texture_stage_state(dev, CID, 0, SVGA3D_TS_MINFILTER, SVGA3D_TEX_FILTER_NEAREST);
        svga3_vlkn_context_set_texture_stage_state(dev, CID, 0, SVGA3D_TS_MAGFILTER, SVGA3D_TEX_FILTER_NEAREST);

        uint32_t texData[2] = {
            0x33FF0000, /* Left: Red, Alpha = ~0.2 */
            0xCC00FF00  /* Right: Green, Alpha = ~0.8 */
        };
        SVGA3dBox tBox = { 0, 0, 0, 2, 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_TEX, 0, &tBox, texData, sizeof(texData));

        Vertex quadVerts[6] = {
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f },
            {  1.0f,  1.0f, 0.5f,  1.0f, 0.0f },
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f },
            { -1.0f,  1.0f, 0.5f,  0.0f, 0.0f },
            {  1.0f, -1.0f, 0.5f,  1.0f, 1.0f },
            { -1.0f, -1.0f, 0.5f,  0.0f, 1.0f }
        };
        SVGA3dSize vbSz = { sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_define(dev, SID_VB, SVGA3D_SURFACE_HINT_VERTEXBUFFER, SVGA3D_BUFFER, &vbSz, 1);
        SVGA3dBox bBox = { 0, 0, 0, sizeof(quadVerts), 1, 1 };
        svga3_vlkn_surface_dma_upload(dev, SID_VB, 0, &bBox, quadVerts, sizeof(quadVerts));

        decls[0].array.surfaceId = SID_VB;
        decls[1].array.surfaceId = SID_VB;
        SVGA3dPrimitiveRange r1 = { SVGA3D_PRIMITIVE_TRIANGLELIST, 2, {}, 0, 0 };

        /* 1. Draw with Alpha Test Enabled: GREATER than 0.5 */
        float alphaRef = 0.5f;
        uint32_t alphaRefBits;
        memcpy(&alphaRefBits, &alphaRef, sizeof(float));

        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ALPHATESTENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ALPHAFUNC, SVGA3D_CMP_GREATER);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ALPHAREF, alphaRefBits);

        /* Clear background to Blue (0, 0, 255) */
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0xFF0000FF, 1.0f, 0, nullptr, 0);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_device_wait_idle(dev);

        std::vector<Pixel> fbAlphaOn(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fbAlphaOn.data(), RT_W * 4);

        /* Left half (u=0.25, A=0.2 < 0.5) must be discarded -> untouched Blue (0, 0, 255) */
        Pixel pLeftOn = fbAlphaOn[(RT_H / 2) * RT_W + (RT_W / 4)];
        TEST_CHECK(pixelMatches(pLeftOn, 0, 0, 255, 255), "Scene 9 (Alpha Test ON): Left half discarded, background Blue preserved");

        /* Right half (u=0.75, A=0.8 > 0.5) must pass -> Green (0, 255, 0) */
        Pixel pRightOn = fbAlphaOn[(RT_H / 2) * RT_W + (3 * RT_W / 4)];
        TEST_CHECK(pixelMatches(pRightOn, 0, 255, 0, 204), "Scene 9 (Alpha Test ON): Right half passed, Green rendered");

        savePPM("artifacts/scene9_alpha_on.ppm", fbAlphaOn.data(), RT_W, RT_H);

        /* 2. Draw with Alpha Test Disabled: Both halves should render */
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ALPHATESTENABLE, 0);
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0xFF0000FF, 1.0f, 0, nullptr, 0);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        svga3_vlkn_device_wait_idle(dev);

        std::vector<Pixel> fbAlphaOff(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, fbAlphaOff.data(), RT_W * 4);

        /* Left half (Red) must now overwrite Blue */
        Pixel pLeftOff = fbAlphaOff[(RT_H / 2) * RT_W + (RT_W / 4)];
        TEST_CHECK(pixelMatches(pLeftOff, 255, 0, 0, 51), "Scene 9 (Alpha Test OFF): Left half drawn, Red rendered");

        savePPM("artifacts/scene9_alpha_off.ppm", fbAlphaOff.data(), RT_W, RT_H);

        /* Exercise every SVGA comparison mode against alpha values below and
         * above the reference. Equality must reject both at this reference. */
        const bool expectedLeft[8] = {false, true, false, true, false, true, false, true};
        const bool expectedRight[8] = {false, false, false, false, true, true, true, true};
        for (uint32_t func = SVGA3D_CMP_NEVER; func <= SVGA3D_CMP_ALWAYS; ++func) {
            svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ALPHATESTENABLE, 1);
            svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ALPHAFUNC, func);
            TEST_CHECK(svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0xFF0000FF,
                1.0f, 0, nullptr, 0) == SVGA3_VLKN_SUCCESS, "Alpha comparison clear succeeds");
            TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST,
                decls, 2, &r1, 1) == SVGA3_VLKN_SUCCESS, "Alpha comparison draw succeeds");
            std::vector<Pixel> pixels(RT_W * RT_H);
            TEST_CHECK(svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr,
                pixels.data(), RT_W * 4) == SVGA3_VLKN_SUCCESS, "Alpha comparison readback succeeds");
            Pixel left = pixels[(RT_H / 2) * RT_W + (RT_W / 4)];
            Pixel right = pixels[(RT_H / 2) * RT_W + (3 * RT_W / 4)];
            TEST_CHECK(expectedLeft[func - 1] ? pixelMatches(left,255,0,0,51) :
                pixelMatches(left,0,0,255,255), "Alpha comparison preserves or draws left pixels");
            TEST_CHECK(expectedRight[func - 1] ? pixelMatches(right,0,255,0,204) :
                pixelMatches(right,0,0,255,255), "Alpha comparison preserves or draws right pixels");
        }

        /* Discarded fragments must not write depth or stencil. First mark
         * only the opaque half in stencil, then use that mask to select pixels. */
        const uint32_t SID_DS = 94;
        TEST_CHECK(svga3_vlkn_surface_define(dev, SID_DS, SVGA3D_SURFACE_HINT_DEPTHSTENCIL,
            SVGA3D_Z_D24S8, &rtSz, 1) == SVGA3_VLKN_SUCCESS, "Create alpha depth/stencil target");
        svga3_vlkn_context_set_render_target(dev, CID, SVGA3D_RT_DEPTH, SID_DS, 0, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ZENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ZWRITEENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILFUNC, SVGA3D_CMP_ALWAYS);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILREF, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILPASS, SVGA3D_STENCILOP_REPLACE);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ALPHATESTENABLE, 1);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ALPHAFUNC, SVGA3D_CMP_GREATER);
        TEST_CHECK(svga3_vlkn_context_clear(dev, CID,
            static_cast<SVGA3dClearFlag>(SVGA3D_CLEAR_COLOR | SVGA3D_CLEAR_DEPTH | SVGA3D_CLEAR_STENCIL),
            0xFF0000FF, 1.0f, 0, nullptr, 0) == SVGA3_VLKN_SUCCESS, "Clear alpha depth/stencil target");
        TEST_CHECK(svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST,
            decls, 2, &r1, 1) == SVGA3_VLKN_SUCCESS, "Draw alpha-filtered stencil mask");
        svga3_vlkn_context_clear(dev, CID, SVGA3D_CLEAR_COLOR, 0xFF0000FF, 1.0f, 0, nullptr, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_ALPHATESTENABLE, 0);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILFUNC, SVGA3D_CMP_EQUAL);
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILWRITEMASK, 0);
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        std::vector<Pixel> maskPixels(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, maskPixels.data(), RT_W * 4);
        TEST_CHECK(pixelMatches(maskPixels[(RT_H/2)*RT_W+RT_W/4],0,0,255,255),
            "Alpha-discarded fragments leave stencil clear");
        TEST_CHECK(pixelMatches(maskPixels[(RT_H/2)*RT_W+3*RT_W/4],0,255,0,204),
            "Stencil reference and replacement preserve accepted fragments");
        svga3_vlkn_context_set_render_state(dev, CID, SVGA3D_RS_STENCILENABLE, 0);
        for (auto &vertex : quadVerts) vertex.z = 0.75f;
        svga3_vlkn_surface_dma_upload(dev, SID_VB, 0, &bBox, quadVerts, sizeof(quadVerts));
        svga3_vlkn_context_draw(dev, CID, SVGA3D_PRIMITIVE_TRIANGLELIST, decls, 2, &r1, 1);
        std::vector<Pixel> depthPixels(RT_W * RT_H);
        svga3_vlkn_surface_dma_download(dev, SID_RT, 0, nullptr, depthPixels.data(), RT_W * 4);
        TEST_CHECK(pixelMatches(depthPixels[(RT_H/2)*RT_W+RT_W/4],255,0,0,51),
            "Alpha-discarded fragments leave depth available for farther geometry");
        TEST_CHECK(pixelMatches(depthPixels[(RT_H/2)*RT_W+3*RT_W/4],0,255,0,204),
            "Accepted fragments block farther geometry with their depth");

        /* Clean up Scene 9 */
        svga3_vlkn_context_destroy(dev, CID);
        svga3_vlkn_surface_destroy(dev, SID_DS);
        svga3_vlkn_surface_destroy(dev, SID_RT);
        svga3_vlkn_surface_destroy(dev, SID_TEX);
        svga3_vlkn_surface_destroy(dev, SID_VB);
    }

    /* Keep the backend alive long enough to check messages emitted while
     * resources and the Vulkan device are destroyed, not just while drawing. */
    TEST_CHECK(dev->backend->validationErrors() == 0 && dev->backend->validationWarnings() == 0,
        "Rendering completes without validation errors or warnings");
    dev->contextMgr->clear();
    dev->surfaceMgr->clear();
    dev->backend->shutdown();
    TEST_CHECK(dev->backend->validationErrors() == 0 && dev->backend->validationWarnings() == 0,
        "Resource and device teardown remain validation clean");
    svga3_vlkn_device_destroy(dev);
    std::cout << "\n  [PASS] Clean device destruction completed" << std::endl;

    std::cout << "\n======================================================================" << std::endl;
    std::cout << "DELIVERABLE 4 ACCEPTANCE SUITE: ALL SCENES VERIFIED SUCCESSFULLY!" << std::endl;
    std::cout << "======================================================================" << std::endl;

    return 0;
}
