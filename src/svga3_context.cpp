/*
 * SVGA3=VLKN - SVGA3D Context & Vulkan Pipeline State Tracker Implementation
 */

#include "svga3_context.h"
#include "svga3_shader_translator.h"
#include "../data/svga3d_reference.h"
#include <cstring>
#include <algorithm>
#include <iostream>

extern "C" void log_msg(const char *fmt, ...);

/* Extern "C" conversion functions from svga3_vlkn.h */
extern "C" {

VkFormat svga3_format_to_vk(SVGA3dSurfaceFormat svgaFormat) {
    return (VkFormat)svga3d_to_vk_format((uint32_t)svgaFormat);
}

VkPrimitiveTopology svga3_primitive_to_vk(SVGA3dPrimitiveType svgaTopology) {
    return (VkPrimitiveTopology)svga3d_to_vk_primitive_topology((uint32_t)svgaTopology);
}

VkCompareOp svga3_cmp_func_to_vk(SVGA3dCmpFunc cmp) {
    switch (cmp) {
        case SVGA3D_CMP_NEVER:        return VK_COMPARE_OP_NEVER;
        case SVGA3D_CMP_LESS:         return VK_COMPARE_OP_LESS;
        case SVGA3D_CMP_EQUAL:        return VK_COMPARE_OP_EQUAL;
        case SVGA3D_CMP_LESSEQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case SVGA3D_CMP_GREATER:      return VK_COMPARE_OP_GREATER;
        case SVGA3D_CMP_NOTEQUAL:     return VK_COMPARE_OP_NOT_EQUAL;
        case SVGA3D_CMP_GREATEREQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case SVGA3D_CMP_ALWAYS:       return VK_COMPARE_OP_ALWAYS;
        default:                      return VK_COMPARE_OP_ALWAYS;
    }
}

VkBlendOp svga3_blend_eq_to_vk(SVGA3dBlendEquation eq) {
    switch (eq) {
        case SVGA3D_BLENDEQ_ADD:         return VK_BLEND_OP_ADD;
        case SVGA3D_BLENDEQ_SUBTRACT:    return VK_BLEND_OP_SUBTRACT;
        case SVGA3D_BLENDEQ_REVSUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case SVGA3D_BLENDEQ_MINIMUM:     return VK_BLEND_OP_MIN;
        case SVGA3D_BLENDEQ_MAXIMUM:     return VK_BLEND_OP_MAX;
        default:                         return VK_BLEND_OP_ADD;
    }
}

VkBlendFactor svga3_blend_factor_to_vk(SVGA3dBlendOp factor) {
    switch (factor) {
        case SVGA3D_BLENDOP_ZERO:            return VK_BLEND_FACTOR_ZERO;
        case SVGA3D_BLENDOP_ONE:             return VK_BLEND_FACTOR_ONE;
        case SVGA3D_BLENDOP_SRCCOLOR:        return VK_BLEND_FACTOR_SRC_COLOR;
        case SVGA3D_BLENDOP_INVSRCCOLOR:     return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case SVGA3D_BLENDOP_SRCALPHA:        return VK_BLEND_FACTOR_SRC_ALPHA;
        case SVGA3D_BLENDOP_INVSRCALPHA:     return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case SVGA3D_BLENDOP_DESTALPHA:       return VK_BLEND_FACTOR_DST_ALPHA;
        case SVGA3D_BLENDOP_INVDESTALPHA:    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case SVGA3D_BLENDOP_DESTCOLOR:       return VK_BLEND_FACTOR_DST_COLOR;
        case SVGA3D_BLENDOP_INVDESTCOLOR:    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case SVGA3D_BLENDOP_SRCALPHASAT:     return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case SVGA3D_BLENDOP_BLENDFACTOR:     return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case SVGA3D_BLENDOP_INVBLENDFACTOR:  return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        default:                             return VK_BLEND_FACTOR_ONE;
    }
}

VkStencilOp svga3_stencil_op_to_vk(SVGA3dStencilOp op) {
    switch (op) {
        case SVGA3D_STENCILOP_KEEP:    return VK_STENCIL_OP_KEEP;
        case SVGA3D_STENCILOP_ZERO:    return VK_STENCIL_OP_ZERO;
        case SVGA3D_STENCILOP_REPLACE: return VK_STENCIL_OP_REPLACE;
        case SVGA3D_STENCILOP_INCRSAT: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case SVGA3D_STENCILOP_DECRSAT: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case SVGA3D_STENCILOP_INVERT:  return VK_STENCIL_OP_INVERT;
        case SVGA3D_STENCILOP_INCR:    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case SVGA3D_STENCILOP_DECR:    return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default:                       return VK_STENCIL_OP_KEEP;
    }
}

VkSamplerAddressMode svga3_texture_address_to_vk(SVGA3dTextureAddress addr) {
    switch (addr) {
        case SVGA3D_TEX_ADDRESS_WRAP:       return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SVGA3D_TEX_ADDRESS_MIRROR:     return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SVGA3D_TEX_ADDRESS_CLAMP:      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SVGA3D_TEX_ADDRESS_BORDER:     return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case SVGA3D_TEX_ADDRESS_MIRRORONCE: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        case SVGA3D_TEX_ADDRESS_EDGE:       return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        default:                            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

} // extern "C"

namespace svga3_vlkn {

static VkFormat svga3_decl_type_to_vk(SVGA3dDeclType type) {
    switch (type) {
        case SVGA3D_DECLTYPE_FLOAT1:    return VK_FORMAT_R32_SFLOAT;
        case SVGA3D_DECLTYPE_FLOAT2:    return VK_FORMAT_R32G32_SFLOAT;
        case SVGA3D_DECLTYPE_FLOAT3:    return VK_FORMAT_R32G32B32_SFLOAT;
        case SVGA3D_DECLTYPE_FLOAT4:    return VK_FORMAT_R32G32B32A32_SFLOAT;
        case SVGA3D_DECLTYPE_D3DCOLOR:  return VK_FORMAT_B8G8R8A8_UNORM;
        case SVGA3D_DECLTYPE_UBYTE4:    return VK_FORMAT_R8G8B8A8_USCALED;
        case SVGA3D_DECLTYPE_SHORT2:    return VK_FORMAT_R16G16_SSCALED;
        case SVGA3D_DECLTYPE_SHORT4:    return VK_FORMAT_R16G16B16A16_SSCALED;
        case SVGA3D_DECLTYPE_UBYTE4N:   return VK_FORMAT_R8G8B8A8_UNORM;
        case SVGA3D_DECLTYPE_SHORT2N:   return VK_FORMAT_R16G16_SNORM;
        case SVGA3D_DECLTYPE_SHORT4N:   return VK_FORMAT_R16G16B16A16_SNORM;
        case SVGA3D_DECLTYPE_USHORT2N:  return VK_FORMAT_R16G16_UNORM;
        case SVGA3D_DECLTYPE_USHORT4N:  return VK_FORMAT_R16G16B16A16_UNORM;
        case SVGA3D_DECLTYPE_FLOAT16_2: return VK_FORMAT_R16G16_SFLOAT;
        case SVGA3D_DECLTYPE_FLOAT16_4: return VK_FORMAT_R16G16B16A16_SFLOAT;
        default:                        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}

/* VlknContext implementation */

VlknContext::VlknContext(VlknBackend *backend, VlknSurfaceManager *surfaceMgr, uint32_t cid)
    : m_backend(backend)
    , m_surfaceMgr(surfaceMgr)
    , m_cid(cid)
    , m_activeFramebuffer(VK_NULL_HANDLE)
    , m_activeRenderPass(VK_NULL_HANDLE)
    , m_inRenderPass(false)
    , m_defaultPipelineLayout(VK_NULL_HANDLE)
    , m_defaultVS(VK_NULL_HANDLE)
    , m_defaultFS(VK_NULL_HANDLE)
    , m_defaultFSTex(VK_NULL_HANDLE)
    , m_descriptorSetInitialized(false)
    , m_descriptorSetDirty(true)
    , m_constantsDirty(true)
    , m_drawCount(0)
    , m_vertexCount(0)
    , m_clearCount(0)
    , m_lastDrawnWindowSid(SVGA3D_INVALID_ID)
    , m_hasDrawnToWindow(false)
    , m_dummyVb(VK_NULL_HANDLE)
    , m_dummyVbMemory(VK_NULL_HANDLE)
    , m_defaultVsInputMask(0)
{
    memset(m_renderTargets, 0, sizeof(m_renderTargets));
    memset(&m_depthStencilTarget, 0, sizeof(m_depthStencilTarget));
    memset(m_stages, 0, sizeof(m_stages));
    for (uint32_t i = 0; i < SVGA3_MAX_TEXTURE_STAGES; ++i) {
        m_stages[i].sid = SVGA3D_INVALID_ID;
    }

    m_viewport.x = 0.0f;
    m_viewport.y = 0.0f;
    m_viewport.width = 1.0f;
    m_viewport.height = 1.0f;
    m_viewport.minDepth = 0.0f;
    m_viewport.maxDepth = 1.0f;

    m_scissor.offset.x = 0;
    m_scissor.offset.y = 0;
    m_scissor.extent.width = 1;
    m_scissor.extent.height = 1;

    m_zRange.min = 0.0f;
    m_zRange.max = 1.0f;
    memset(m_clipPlanes, 0, sizeof(m_clipPlanes));
    memset(m_materials, 0, sizeof(m_materials));
    memset(m_lights, 0, sizeof(m_lights));

    m_boundVS = SVGA3D_INVALID_ID;
    m_boundPS = SVGA3D_INVALID_ID;
    memset(&m_vsConsts, 0, sizeof(m_vsConsts));
    memset(&m_psConsts, 0, sizeof(m_psConsts));

    m_queryPool = VK_NULL_HANDLE;
    m_queryActive = false;
    m_queryEnded = false;
    m_lastQueryResult = 0;

    initDefaultRenderStates();

    /* Identity MVP matrix in VS constant c0..c3 */
    m_vsConsts.floatConsts[0][0] = 1.0f;
    m_vsConsts.floatConsts[1][1] = 1.0f;
    m_vsConsts.floatConsts[2][2] = 1.0f;
    m_vsConsts.floatConsts[3][3] = 1.0f;

    /* Create descriptor set layout for UBOs (bindings 0, 1) and samplers (bindings 2..9) */
    VkDescriptorSetLayoutBinding descBindings[10] = {};
    descBindings[0].binding = 0;
    descBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descBindings[0].descriptorCount = 1;
    descBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    descBindings[1].binding = 1;
    descBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descBindings[1].descriptorCount = 1;
    descBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    for (int i = 0; i < 8; ++i) {
        descBindings[2 + i].binding = 2 + i;
        descBindings[2 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descBindings[2 + i].descriptorCount = 1;
        descBindings[2 + i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo dslInfo = {};
    dslInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslInfo.bindingCount = 10;
    dslInfo.pBindings = descBindings;
    m_backend->dispatch().vkCreateDescriptorSetLayout(m_backend->device(), &dslInfo, nullptr, &m_descriptorSetLayout);

    /* Create pipeline layout */
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    m_backend->dispatch().vkCreatePipelineLayout(m_backend->device(), &layoutInfo, nullptr, &m_defaultPipelineLayout);

    /* Allocate descriptor set */
    VkDescriptorSetAllocateInfo dsAlloc = {};
    dsAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAlloc.descriptorPool = m_backend->descriptorPool();
    dsAlloc.descriptorSetCount = 1;
    dsAlloc.pSetLayouts = &m_descriptorSetLayout;
    m_backend->dispatch().vkAllocateDescriptorSets(m_backend->device(), &dsAlloc, &m_descriptorSet);

    /* Create VS and PS constant buffers (4096 bytes each for 256 vec4 constants) */
    m_backend->createBuffer(
        4096,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &m_vsConstBuffer,
        &m_vsConstMemory
    );
    m_backend->createBuffer(
        4096,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &m_psConstBuffer,
        &m_psConstMemory
    );

    /* Create 1x1 dummy texture and sampler for unbound sampler stages */
    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent.width = 1;
    imgInfo.extent.height = 1;
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_backend->dispatch().vkCreateImage(m_backend->device(), &imgInfo, nullptr, &m_dummyImage);

    VkMemoryRequirements memReqs;
    m_backend->dispatch().vkGetImageMemoryRequirements(m_backend->device(), m_dummyImage, &memReqs);
    int memType = m_backend->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_backend->allocateMemory(memReqs.size, memType, &m_dummyMemory);
    m_backend->dispatch().vkBindImageMemory(m_backend->device(), m_dummyImage, m_dummyMemory, 0);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_dummyImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    m_backend->dispatch().vkCreateImageView(m_backend->device(), &viewInfo, nullptr, &m_dummyView);

    VkSamplerCreateInfo sampInfo = {};
    sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampInfo.magFilter = VK_FILTER_LINEAR;
    sampInfo.minFilter = VK_FILTER_LINEAR;
    sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    m_backend->dispatch().vkCreateSampler(m_backend->device(), &sampInfo, nullptr, &m_dummySampler);

    /* Initialize m_dummyImage to SHADER_READ_ONLY_OPTIMAL layout with a cleared black pixel */
    VkCommandBuffer initCb = m_backend->getActiveCommandBuffer();
    VkImageMemoryBarrier dummyBarrier = {};
    dummyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    dummyBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dummyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dummyBarrier.srcAccessMask = 0;
    dummyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dummyBarrier.image = m_dummyImage;
    dummyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    dummyBarrier.subresourceRange.baseMipLevel = 0;
    dummyBarrier.subresourceRange.levelCount = 1;
    dummyBarrier.subresourceRange.baseArrayLayer = 0;
    dummyBarrier.subresourceRange.layerCount = 1;

    m_backend->dispatch().vkCmdPipelineBarrier(
        initCb,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &dummyBarrier
    );

    if (m_backend->stagingBuffer() && m_backend->stagingMapped()) {
        std::lock_guard<std::mutex> lock(m_backend->stagingMutex());
        uint32_t black = 0;
        memcpy(m_backend->stagingMapped(), &black, sizeof(black));
        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = { 1, 1, 1 };
        m_backend->dispatch().vkCmdCopyBufferToImage(
            initCb, m_backend->stagingBuffer(), m_dummyImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region
        );
    }

    dummyBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dummyBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dummyBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dummyBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    m_backend->dispatch().vkCmdPipelineBarrier(
        initCb,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &dummyBarrier
    );
    m_backend->flushCommandBuffer();

    /* Create occlusion query pool */
    VkQueryPoolCreateInfo qpInfo = {};
    qpInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qpInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
    qpInfo.queryCount = 16;
    m_backend->dispatch().vkCreateQueryPool(m_backend->device(), &qpInfo, nullptr, &m_queryPool);

    /* Create dummy vertex buffer initialized to (1.0, 1.0, 1.0, 1.0) for missing vertex attributes */
    m_backend->createBuffer(
        256,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &m_dummyVb,
        &m_dummyVbMemory
    );
    if (m_dummyVb && m_dummyVbMemory) {
        void *mapped = nullptr;
        if (m_backend->dispatch().vkMapMemory(m_backend->device(), m_dummyVbMemory, 0, 256, 0, &mapped) == VK_SUCCESS) {
            float dummyVal[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            memcpy(mapped, dummyVal, sizeof(dummyVal));
            m_backend->dispatch().vkUnmapMemory(m_backend->device(), m_dummyVbMemory);
        }
    }

    /* Create valid default VS & FS using D3D9 translator into standard SPIR-V */
#define D3D9_REG(regType, regNum) \
    (0x80000000u | (((regType) & 0x7) << 28) | ((((regType) >> 3) & 0x3) << 11) | ((regNum) & 0x7FF))
#define D3D9_DST(regType, regNum, mask) \
    (D3D9_REG(regType, regNum) | (((mask) & 0xF) << 16))
#define D3D9_SRC(regType, regNum, swiz) \
    (D3D9_REG(regType, regNum) | (((swiz) & 0xFF) << 16))

    static const uint32_t defVsTokens[] = {
        0xFFFE0300, /* vs_3_0 */
        (31) | (2 << 24), 0x80000000 | 0,  D3D9_DST(1, 0, 0xF), /* dcl_position v0 */
        (31) | (2 << 24), 0x80000000 | 10, D3D9_DST(1, 1, 0xF), /* dcl_color v1 */
        (31) | (2 << 24), 0x80000000 | 5,  D3D9_DST(1, 2, 0xF), /* dcl_texcoord0 v2 */
        (31) | (2 << 24), 0x80000000 | (1 << 16) | 5, D3D9_DST(1, 3, 0xF), /* dcl_texcoord1 v3 */
        (20) | (3 << 24), D3D9_DST(4, 0, 0xF), D3D9_SRC(1, 0, 0xE4), D3D9_SRC(2, 0, 0xE4), /* m4x4 oPos, v0, c0 */
        (1)  | (2 << 24), D3D9_DST(5, 0, 0xF), D3D9_SRC(1, 1, 0xE4), /* mov oD0, v1 */
        (1)  | (2 << 24), D3D9_DST(6, 0, 0xF), D3D9_SRC(1, 2, 0xE4), /* mov oT0, v2 */
        (1)  | (2 << 24), D3D9_DST(6, 1, 0xF), D3D9_SRC(1, 3, 0xE4), /* mov oT1, v3 */
        0x0000FFFF
    };
    std::vector<uint32_t> defVsSpirv;
    std::string defErr;
    svga3_translate_shader_d3d9(SVGA3D_SHADERTYPE_VS, defVsTokens, sizeof(defVsTokens)/sizeof(uint32_t), defVsSpirv, defErr, &m_defaultVsInputMask);
    VkShaderModuleCreateInfo smInfo = {};
    smInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smInfo.codeSize = defVsSpirv.size() * sizeof(uint32_t);
    smInfo.pCode = defVsSpirv.data();
    m_backend->dispatch().vkCreateShaderModule(m_backend->device(), &smInfo, nullptr, &m_defaultVS);

    static const uint32_t defPsTokens[] = {
        0xFFFF0300, /* ps_3_0 */
        (31) | (2 << 24), 0x80000000 | 10, D3D9_DST(1, 0, 0xF), /* dcl_color v0 */
        (1)  | (2 << 24), D3D9_DST(8, 0, 0xF), D3D9_SRC(1, 0, 0xE4), /* mov oC0, v0 */
        0x0000FFFF
    };
    std::vector<uint32_t> defPsSpirv;
    svga3_translate_shader_d3d9(SVGA3D_SHADERTYPE_PS, defPsTokens, sizeof(defPsTokens)/sizeof(uint32_t), defPsSpirv, defErr);
    smInfo.codeSize = defPsSpirv.size() * sizeof(uint32_t);
    smInfo.pCode = defPsSpirv.data();
    m_backend->dispatch().vkCreateShaderModule(m_backend->device(), &smInfo, nullptr, &m_defaultFS);

    /* Create textured default FS: samples texture stage 0 and modulates with vertex color */
    static const uint32_t defPsTexTokens[] = {
        0xFFFF0300, /* ps_3_0 */
        (31) | (2 << 24), 0x80000000 | 10, D3D9_DST(1, 0, 0xF), /* dcl_color v0 */
        (31) | (2 << 24), 0x80000000 | 5,  D3D9_DST(1, 1, 0xF), /* dcl_texcoord v1 */
        (31) | (2 << 24), 0x80000000 | (2 << 28), D3D9_DST(10, 0, 0xF), /* dcl_2d s0 */
        (66) | (3 << 24), D3D9_DST(0, 0, 0xF), D3D9_SRC(1, 1, 0xE4), D3D9_SRC(10, 0, 0xE4), /* texld r0, v1, s0 */
        (5)  | (3 << 24), D3D9_DST(8, 0, 0xF), D3D9_SRC(0, 0, 0xE4), D3D9_SRC(1, 0, 0xE4), /* mul oC0, r0, v0 */
        0x0000FFFF
    };
    std::vector<uint32_t> defPsTexSpirv;
    svga3_translate_shader_d3d9(SVGA3D_SHADERTYPE_PS, defPsTexTokens, sizeof(defPsTexTokens)/sizeof(uint32_t), defPsTexSpirv, defErr);
    smInfo.codeSize = defPsTexSpirv.size() * sizeof(uint32_t);
    smInfo.pCode = defPsTexSpirv.data();
    m_backend->dispatch().vkCreateShaderModule(m_backend->device(), &smInfo, nullptr, &m_defaultFSTex);
#undef D3D9_REG
#undef D3D9_DST
#undef D3D9_SRC
}

VlknContext::~VlknContext() {
    endRenderPassIfActive();

    for (auto &pair : m_pipelineCache) {
        if (pair.second) {
            m_backend->dispatch().vkDestroyPipeline(m_backend->device(), pair.second, nullptr);
        }
    }
    m_pipelineCache.clear();

    for (uint32_t i = 0; i < SVGA3_MAX_TEXTURE_STAGES; ++i) {
        if (m_stages[i].sampler) {
            m_backend->dispatch().vkDestroySampler(m_backend->device(), m_stages[i].sampler, nullptr);
            m_stages[i].sampler = VK_NULL_HANDLE;
        }
    }

    for (auto &pair : m_vertexShaders) {
        if (pair.second.module && pair.second.module != m_defaultVS) {
            m_backend->dispatch().vkDestroyShaderModule(m_backend->device(), pair.second.module, nullptr);
        }
    }
    m_vertexShaders.clear();

    for (auto &pair : m_pixelShaders) {
        if (pair.second.module && pair.second.module != m_defaultFS && pair.second.module != m_defaultFSTex) {
            m_backend->dispatch().vkDestroyShaderModule(m_backend->device(), pair.second.module, nullptr);
        }
    }
    m_pixelShaders.clear();

    if (m_defaultVS) {
        m_backend->dispatch().vkDestroyShaderModule(m_backend->device(), m_defaultVS, nullptr);
        m_defaultVS = VK_NULL_HANDLE;
    }
    if (m_defaultFS) {
        m_backend->dispatch().vkDestroyShaderModule(m_backend->device(), m_defaultFS, nullptr);
        m_defaultFS = VK_NULL_HANDLE;
    }
    if (m_defaultFSTex) {
        m_backend->dispatch().vkDestroyShaderModule(m_backend->device(), m_defaultFSTex, nullptr);
        m_defaultFSTex = VK_NULL_HANDLE;
    }
    if (m_dummySampler) {
        m_backend->dispatch().vkDestroySampler(m_backend->device(), m_dummySampler, nullptr);
        m_dummySampler = VK_NULL_HANDLE;
    }
    if (m_dummyView) {
        m_backend->dispatch().vkDestroyImageView(m_backend->device(), m_dummyView, nullptr);
        m_dummyView = VK_NULL_HANDLE;
    }
    if (m_dummyImage) {
        m_backend->dispatch().vkDestroyImage(m_backend->device(), m_dummyImage, nullptr);
        m_dummyImage = VK_NULL_HANDLE;
    }
    if (m_dummyMemory) {
        m_backend->freeMemory(m_dummyMemory);
        m_dummyMemory = VK_NULL_HANDLE;
    }
    if (m_dummyVb) {
        m_backend->dispatch().vkDestroyBuffer(m_backend->device(), m_dummyVb, nullptr);
        m_dummyVb = VK_NULL_HANDLE;
    }
    if (m_dummyVbMemory) {
        m_backend->freeMemory(m_dummyVbMemory);
        m_dummyVbMemory = VK_NULL_HANDLE;
    }
    if (m_vsConstBuffer) {
        m_backend->dispatch().vkDestroyBuffer(m_backend->device(), m_vsConstBuffer, nullptr);
        m_vsConstBuffer = VK_NULL_HANDLE;
    }
    if (m_vsConstMemory) {
        m_backend->freeMemory(m_vsConstMemory);
        m_vsConstMemory = VK_NULL_HANDLE;
    }
    if (m_psConstBuffer) {
        m_backend->dispatch().vkDestroyBuffer(m_backend->device(), m_psConstBuffer, nullptr);
        m_psConstBuffer = VK_NULL_HANDLE;
    }
    if (m_psConstMemory) {
        m_backend->freeMemory(m_psConstMemory);
        m_psConstMemory = VK_NULL_HANDLE;
    }
    if (m_defaultPipelineLayout) {
        m_backend->dispatch().vkDestroyPipelineLayout(m_backend->device(), m_defaultPipelineLayout, nullptr);
        m_defaultPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout) {
        m_backend->dispatch().vkDestroyDescriptorSetLayout(m_backend->device(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (m_activeFramebuffer) {
        m_backend->dispatch().vkDestroyFramebuffer(m_backend->device(), m_activeFramebuffer, nullptr);
        m_activeFramebuffer = VK_NULL_HANDLE;
    }

    if (m_queryPool != VK_NULL_HANDLE) {
        m_backend->dispatch().vkDestroyQueryPool(m_backend->device(), m_queryPool, nullptr);
        m_queryPool = VK_NULL_HANDLE;
    }
}

void VlknContext::initDefaultRenderStates() {
    m_renderStates[SVGA3D_RS_ZENABLE] = 1;
    m_renderStates[SVGA3D_RS_ZWRITEENABLE] = 1;
    m_renderStates[SVGA3D_RS_ZFUNC] = SVGA3D_CMP_LESSEQUAL;
    m_renderStates[SVGA3D_RS_ALPHATESTENABLE] = 0;
    m_renderStates[SVGA3D_RS_SRCBLEND] = SVGA3D_BLENDOP_ONE;
    m_renderStates[SVGA3D_RS_DSTBLEND] = SVGA3D_BLENDOP_ZERO;
    m_renderStates[SVGA3D_RS_CULLMODE] = SVGA3D_FACE_BACK;
    m_renderStates[SVGA3D_RS_DITHERENABLE] = 0;
    m_renderStates[SVGA3D_RS_BLENDENABLE] = 0;
    m_renderStates[SVGA3D_RS_FOGENABLE] = 0;
    m_renderStates[SVGA3D_RS_SPECULARENABLE] = 0;
    m_renderStates[SVGA3D_RS_STENCILENABLE] = 0;
    m_renderStates[SVGA3D_RS_STENCILFAIL] = SVGA3D_STENCILOP_KEEP;
    m_renderStates[SVGA3D_RS_STENCILZFAIL] = SVGA3D_STENCILOP_KEEP;
    m_renderStates[SVGA3D_RS_STENCILPASS] = SVGA3D_STENCILOP_KEEP;
    m_renderStates[SVGA3D_RS_STENCILFUNC] = SVGA3D_CMP_ALWAYS;
    m_renderStates[SVGA3D_RS_STENCILREF] = 0;
    m_renderStates[SVGA3D_RS_STENCILMASK] = 0xFFFFFFFF;
    m_renderStates[SVGA3D_RS_STENCILWRITEMASK] = 0xFFFFFFFF;
    m_renderStates[SVGA3D_RS_TEXTUREFACTOR] = 0xFFFFFFFF;
    m_renderStates[SVGA3D_RS_CLIPPING] = 1;
    m_renderStates[SVGA3D_RS_LIGHTINGENABLE] = 1;
    m_renderStates[SVGA3D_RS_AMBIENT] = 0;
    m_renderStates[SVGA3D_RS_COLORWRITEENABLE] = 0xF; /* RGBA */
    m_renderStates[SVGA3D_RS_BLENDEQUATION] = SVGA3D_BLENDEQ_ADD;
    m_renderStates[SVGA3D_RS_SCISSORTESTENABLE] = 0;
    m_renderStates[SVGA3D_RS_SLOPESCALEDEPTHBIAS] = 0;
    m_renderStates[SVGA3D_RS_DEPTHBIAS] = 0;
    m_renderStates[SVGA3D_RS_SEPARATEALPHABLENDENABLE] = 0;
    m_renderStates[SVGA3D_RS_SRCBLENDALPHA] = SVGA3D_BLENDOP_ONE;
    m_renderStates[SVGA3D_RS_DSTBLENDALPHA] = SVGA3D_BLENDOP_ZERO;
    m_renderStates[SVGA3D_RS_BLENDEQUATIONALPHA] = SVGA3D_BLENDEQ_ADD;

    SVGA3dFillMode fm = {};
    fm.s.mode = SVGA3D_FILLMODE_FILL;
    fm.s.face = SVGA3D_FACE_FRONT_BACK;
    m_renderStates[SVGA3D_RS_FILLMODE] = fm.uintValue;
}

Svga3VlknStatus VlknContext::setRenderState(SVGA3dRenderStateName state, uint32_t value) {
    m_renderStates[(uint32_t)state] = value;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::getRenderState(SVGA3dRenderStateName state, uint32_t *outValue) const {
    if (!outValue) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    auto it = m_renderStates.find((uint32_t)state);
    if (it != m_renderStates.end()) {
        *outValue = it->second;
        return SVGA3_VLKN_SUCCESS;
    }
    return SVGA3_VLKN_ERROR_NOT_FOUND;
}

Svga3VlknStatus VlknContext::setRenderTarget(SVGA3dRenderTargetType type, uint32_t sid, uint32_t face, uint32_t mipmap) {
    endRenderPassIfActive();

    if (type >= SVGA3D_RT_COLOR0 && type <= SVGA3D_RT_COLOR3) {
        uint32_t idx = type - SVGA3D_RT_COLOR0;
        m_renderTargets[idx].sid = sid;
        m_renderTargets[idx].face = face;
        m_renderTargets[idx].mipmap = mipmap;
    } else if (type == SVGA3D_RT_DEPTH || type == SVGA3D_RT_STENCIL) {
        m_depthStencilTarget.sid = sid;
        m_depthStencilTarget.face = face;
        m_depthStencilTarget.mipmap = mipmap;
    } else {
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }
    return SVGA3_VLKN_SUCCESS;
}

const RenderTargetBinding* VlknContext::getRenderTarget(SVGA3dRenderTargetType type) const {
    if (type >= SVGA3D_RT_COLOR0 && type <= SVGA3D_RT_COLOR3) {
        return &m_renderTargets[type - SVGA3D_RT_COLOR0];
    } else if (type == SVGA3D_RT_DEPTH || type == SVGA3D_RT_STENCIL) {
        return &m_depthStencilTarget;
    }
    return nullptr;
}

Svga3VlknStatus VlknContext::setTexture(uint32_t stage, uint32_t sid) {
    if (stage >= SVGA3_MAX_TEXTURE_STAGES) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    m_stages[stage].sid = sid;
    m_descriptorSetDirty = true;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::setTextureStageState(uint32_t stage, SVGA3dTextureStateName name, uint32_t value) {
    if (stage >= SVGA3_MAX_TEXTURE_STAGES) return SVGA3_VLKN_ERROR_INVALID_PARAM;

    TextureStageState &s = m_stages[stage];
    m_descriptorSetDirty = true;
    switch (name) {
        case SVGA3D_TS_ADDRESSU:
            s.addressU = value;
            s.samplerDirty = true;
            break;
        case SVGA3D_TS_ADDRESSV:
            s.addressV = value;
            s.samplerDirty = true;
            break;
        case SVGA3D_TS_ADDRESSW:
            s.addressW = value;
            s.samplerDirty = true;
            break;
        case SVGA3D_TS_MINFILTER:
            s.minFilter = value;
            s.samplerDirty = true;
            break;
        case SVGA3D_TS_MAGFILTER:
            s.magFilter = value;
            s.samplerDirty = true;
            break;
        case SVGA3D_TS_MIPFILTER:
            s.mipFilter = value;
            s.samplerDirty = true;
            break;
        case SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL:
            s.maxAnisotropy = value;
            s.samplerDirty = true;
            break;
        case SVGA3D_TS_TEXTURE_LOD_BIAS: {
            union { uint32_t u; float f; } u;
            u.u = value;
            s.mipLodBias = u.f;
            s.samplerDirty = true;
            break;
        }
        default:
            break;
    }
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::setViewport(const SVGA3dRect *rect) {
    if (!rect) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    m_viewport.x = (float)rect->x;
    m_viewport.y = (float)rect->y;
    m_viewport.width = (float)rect->w;
    m_viewport.height = (float)rect->h;
    m_viewport.minDepth = 0.0f;
    m_viewport.maxDepth = 1.0f;
    if (!m_renderStates[SVGA3D_RS_SCISSORTESTENABLE]) {
        m_scissor.offset.x = rect->x;
        m_scissor.offset.y = rect->y;
        m_scissor.extent.width = rect->w;
        m_scissor.extent.height = rect->h;
    }
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::setScissorRect(const SVGA3dRect *rect) {
    if (!rect) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    m_scissor.offset.x = (int32_t)rect->x;
    m_scissor.offset.y = (int32_t)rect->y;
    m_scissor.extent.width = rect->w;
    m_scissor.extent.height = rect->h;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::setTransform(SVGA3dTransformType type, const float matrix[16]) {
    if (!matrix) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    std::array<float, 16> mat;
    std::copy(matrix, matrix + 16, mat.begin());
    m_transforms[(uint32_t)type] = mat;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::getTransform(SVGA3dTransformType type, float outMatrix[16]) const {
    if (!outMatrix) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    auto it = m_transforms.find((uint32_t)type);
    if (it == m_transforms.end()) {
        memset(outMatrix, 0, sizeof(float) * 16);
        outMatrix[0] = outMatrix[5] = outMatrix[10] = outMatrix[15] = 1.0f;
        return SVGA3_VLKN_SUCCESS;
    }
    std::copy(it->second.begin(), it->second.end(), outMatrix);
    return SVGA3_VLKN_SUCCESS;
}

std::array<float, 16> VlknContext::getTransformOrDefault(SVGA3dTransformType type) const {
    auto it = m_transforms.find((uint32_t)type);
    if (it != m_transforms.end()) {
        return it->second;
    }
    std::array<float, 16> identity = {};
    identity[0] = identity[5] = identity[10] = identity[15] = 1.0f;
    return identity;
}

static std::array<float, 16> multiplyMatrix4x4(const std::array<float, 16> &a, const std::array<float, 16> &b) {
    std::array<float, 16> r = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a[i * 4 + k] * b[k * 4 + j];
            }
            r[i * 4 + j] = sum;
        }
    }
    return r;
}

Svga3VlknStatus VlknContext::setZRange(const SVGA3dZRange *zRange) {
    if (!zRange) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    m_zRange = *zRange;
    m_viewport.minDepth = zRange->min;
    m_viewport.maxDepth = zRange->max;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::setClipPlane(uint32_t index, const float plane[4]) {
    if (index >= 6 || !plane) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    memcpy(m_clipPlanes[index].plane, plane, sizeof(float) * 4);
    m_clipPlanes[index].enabled = true;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::getClipPlane(uint32_t index, float outPlane[4]) const {
    if (index >= 6 || !outPlane) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    memcpy(outPlane, m_clipPlanes[index].plane, sizeof(float) * 4);
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::setMaterial(SVGA3dFace face, const SVGA3dMaterial *mat) {
    if (!mat) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    if (face == SVGA3D_FACE_FRONT || face == SVGA3D_FACE_FRONT_BACK) {
        m_materials[0] = *mat;
    }
    if (face == SVGA3D_FACE_BACK || face == SVGA3D_FACE_FRONT_BACK) {
        m_materials[1] = *mat;
    }
    return SVGA3_VLKN_SUCCESS;
}

const SVGA3dMaterial* VlknContext::getMaterial(SVGA3dFace face) const {
    if (face == SVGA3D_FACE_BACK) return &m_materials[1];
    return &m_materials[0];
}

Svga3VlknStatus VlknContext::setLightData(uint32_t index, const SVGA3dLightData *data) {
    if (index >= 8 || !data) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    m_lights[index].data = *data;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::setLightEnabled(uint32_t index, uint32_t enabled) {
    if (index >= 8) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    m_lights[index].enabled = (enabled != 0);
    return SVGA3_VLKN_SUCCESS;
}

bool VlknContext::isLightEnabled(uint32_t index) const {
    if (index >= 8) return false;
    return m_lights[index].enabled;
}

Svga3VlknStatus VlknContext::defineShader(uint32_t shid, SVGA3dShaderType type, const uint32_t *bytecode, uint32_t numDwords) {
    if (!bytecode || numDwords == 0) return SVGA3_VLKN_ERROR_INVALID_PARAM;

    auto &table = (type == SVGA3D_SHADERTYPE_VS) ? m_vertexShaders : m_pixelShaders;
    auto it = table.find(shid);
    if (it != table.end()) {
        if (it->second.module && it->second.module != m_defaultVS && it->second.module != m_defaultFS) {
            m_backend->dispatch().vkDestroyShaderModule(m_backend->device(), it->second.module, nullptr);
        }
        table.erase(it);
    }

    Svga3Shader shader;
    shader.shid = shid;
    shader.type = type;
    shader.bytecode.assign(bytecode, bytecode + numDwords);
    shader.inputLocationMask = 0;

    static const uint32_t SPIRV_MAGIC = 0x07230203;
    if (bytecode[0] == SPIRV_MAGIC) {
        VkShaderModuleCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = numDwords * sizeof(uint32_t);
        info.pCode = bytecode;
        VkResult res = m_backend->dispatch().vkCreateShaderModule(m_backend->device(), &info, nullptr, &shader.module);
        if (res != VK_SUCCESS) {
            return SVGA3_VLKN_ERROR_PIPELINE_CREATION;
        }
    } else {
        std::vector<uint32_t> spirv;
        std::string err;
        uint32_t inMask = 0;
        Svga3VlknStatus st = svga3_translate_shader_d3d9(type, bytecode, numDwords, spirv, err, &inMask);
        if (st != SVGA3_VLKN_SUCCESS) {
            return st; /* Explicit error on unsupported instructions or malformed tokens */
        }
        shader.inputLocationMask = inMask;
        VkShaderModuleCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = spirv.size() * sizeof(uint32_t);
        info.pCode = spirv.data();
        VkResult res = m_backend->dispatch().vkCreateShaderModule(m_backend->device(), &info, nullptr, &shader.module);
        if (res != VK_SUCCESS) {
            return SVGA3_VLKN_ERROR_PIPELINE_CREATION;
        }
    }

    table[shid] = std::move(shader);
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::destroyShader(uint32_t shid, SVGA3dShaderType type) {
    auto &table = (type == SVGA3D_SHADERTYPE_VS) ? m_vertexShaders : m_pixelShaders;
    auto it = table.find(shid);
    if (it == table.end()) {
        return SVGA3_VLKN_ERROR_NOT_FOUND;
    }
    if (it->second.module && it->second.module != m_defaultVS && it->second.module != m_defaultFS) {
        m_backend->dispatch().vkDestroyShaderModule(m_backend->device(), it->second.module, nullptr);
    }
    table.erase(it);
    if (type == SVGA3D_SHADERTYPE_VS && m_boundVS == shid) m_boundVS = SVGA3D_INVALID_ID;
    if (type == SVGA3D_SHADERTYPE_PS && m_boundPS == shid) m_boundPS = SVGA3D_INVALID_ID;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::setShader(SVGA3dShaderType type, uint32_t shid) {
    if (shid == SVGA3D_INVALID_ID) {
        if (type == SVGA3D_SHADERTYPE_VS) m_boundVS = SVGA3D_INVALID_ID;
        else m_boundPS = SVGA3D_INVALID_ID;
        return SVGA3_VLKN_SUCCESS;
    }
    const auto &table = (type == SVGA3D_SHADERTYPE_VS) ? m_vertexShaders : m_pixelShaders;
    if (table.find(shid) == table.end()) {
        return SVGA3_VLKN_ERROR_NOT_FOUND;
    }
    if (type == SVGA3D_SHADERTYPE_VS) m_boundVS = shid;
    else m_boundPS = shid;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::setShaderConst(uint32_t reg, SVGA3dShaderType type, SVGA3dShaderConstType ctype, const uint32_t values[4]) {
    if (!values) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    ShaderConstantBank &bank = (type == SVGA3D_SHADERTYPE_VS) ? m_vsConsts : m_psConsts;

    if (ctype == SVGA3D_CONST_TYPE_FLOAT) {
        if (reg >= 256) return SVGA3_VLKN_ERROR_INVALID_PARAM;
        const float *f = reinterpret_cast<const float*>(values);
        bank.floatConsts[reg][0] = f[0];
        bank.floatConsts[reg][1] = f[1];
        bank.floatConsts[reg][2] = f[2];
        bank.floatConsts[reg][3] = f[3];
    } else if (ctype == SVGA3D_CONST_TYPE_INT) {
        if (reg >= 16) return SVGA3_VLKN_ERROR_INVALID_PARAM;
        const int32_t *i = reinterpret_cast<const int32_t*>(values);
        bank.intConsts[reg][0] = i[0];
        bank.intConsts[reg][1] = i[1];
        bank.intConsts[reg][2] = i[2];
        bank.intConsts[reg][3] = i[3];
    } else if (ctype == SVGA3D_CONST_TYPE_BOOL) {
        if (reg >= 16) return SVGA3_VLKN_ERROR_INVALID_PARAM;
        if (values[0]) {
            bank.boolConsts |= (1u << reg);
        } else {
            bank.boolConsts &= ~(1u << reg);
        }
    } else {
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }
    m_constantsDirty = true;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::getShaderConst(uint32_t reg, SVGA3dShaderType type, SVGA3dShaderConstType ctype, uint32_t outValues[4]) const {
    if (!outValues) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    const ShaderConstantBank &bank = (type == SVGA3D_SHADERTYPE_VS) ? m_vsConsts : m_psConsts;

    if (ctype == SVGA3D_CONST_TYPE_FLOAT) {
        if (reg >= 256) return SVGA3_VLKN_ERROR_INVALID_PARAM;
        memcpy(outValues, bank.floatConsts[reg], sizeof(float) * 4);
    } else if (ctype == SVGA3D_CONST_TYPE_INT) {
        if (reg >= 16) return SVGA3_VLKN_ERROR_INVALID_PARAM;
        memcpy(outValues, bank.intConsts[reg], sizeof(int32_t) * 4);
    } else if (ctype == SVGA3D_CONST_TYPE_BOOL) {
        if (reg >= 16) return SVGA3_VLKN_ERROR_INVALID_PARAM;
        outValues[0] = (bank.boolConsts & (1u << reg)) ? 1 : 0;
        outValues[1] = outValues[2] = outValues[3] = 0;
    } else {
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }
    return SVGA3_VLKN_SUCCESS;
}

uint32_t VlknContext::getBoundShader(SVGA3dShaderType type) const {
    return (type == SVGA3D_SHADERTYPE_VS) ? m_boundVS : m_boundPS;
}

bool VlknContext::hasShader(uint32_t shid, SVGA3dShaderType type) const {
    const auto &table = (type == SVGA3D_SHADERTYPE_VS) ? m_vertexShaders : m_pixelShaders;
    return table.find(shid) != table.end();
}

VkSampler VlknContext::getOrCreateSampler(uint32_t stage) {
    TextureStageState &s = m_stages[stage];
    if (s.sampler && !s.samplerDirty) {
        return s.sampler;
    }

    if (s.sampler) {
        m_backend->dispatch().vkDestroySampler(m_backend->device(), s.sampler, nullptr);
        s.sampler = VK_NULL_HANDLE;
    }

    VkSamplerCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = (s.magFilter == SVGA3D_TEX_FILTER_LINEAR) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    info.minFilter = (s.minFilter == SVGA3D_TEX_FILTER_LINEAR) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    info.mipmapMode = (s.mipFilter == SVGA3D_TEX_FILTER_LINEAR) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU = ::svga3_texture_address_to_vk((SVGA3dTextureAddress)s.addressU);
    info.addressModeV = ::svga3_texture_address_to_vk((SVGA3dTextureAddress)s.addressV);
    info.addressModeW = ::svga3_texture_address_to_vk((SVGA3dTextureAddress)s.addressW);
    info.mipLodBias = s.mipLodBias;
    info.anisotropyEnable = (s.maxAnisotropy > 1) ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy = (float)std::max(1u, s.maxAnisotropy);
    info.minLod = 0.0f;
    info.maxLod = 16.0f;

    m_backend->dispatch().vkCreateSampler(m_backend->device(), &info, nullptr, &s.sampler);
    s.samplerDirty = false;
    return s.sampler;
}

Svga3VlknStatus VlknContext::ensureRenderPassActive() {
    if (m_inRenderPass) return SVGA3_VLKN_SUCCESS;

    VlknSurface *colorSurf = m_surfaceMgr->getSurface(m_renderTargets[0].sid);
    VlknSurface *depthSurf = m_surfaceMgr->getSurface(m_depthStencilTarget.sid);

    static uint32_t erp_count = 0;
    erp_count++;
    if (erp_count <= 5 || (erp_count % 500) == 0) {
        log_msg("[libqemu_svga3d] ensureRenderPassActive #%u: colorSid=%u (surf=%p), depthSid=%u (surf=%p)\n",
                erp_count, m_renderTargets[0].sid, colorSurf, m_depthStencilTarget.sid, depthSurf);
    }

    VkFormat colorFmt = colorSurf ? colorSurf->vkFormat() : VK_FORMAT_UNDEFINED;
    VkFormat depthFmt = depthSurf ? depthSurf->vkFormat() : VK_FORMAT_UNDEFINED;

    if (colorFmt == VK_FORMAT_UNDEFINED && depthFmt == VK_FORMAT_UNDEFINED) {
        colorFmt = VK_FORMAT_B8G8R8A8_UNORM; /* Default fallback target */
    }

    m_activeRenderPass = m_backend->getOrCreateRenderPass(colorFmt, depthFmt);

    /* Build Framebuffer */
    VkImageView attachments[2];
    uint32_t attachCount = 0;
    uint32_t fbWidth = 800;
    uint32_t fbHeight = 600;

    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();

    if (colorSurf) {
        if (colorSurf->currentLayout() != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = colorSurf->currentLayout();
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            barrier.image = colorSurf->image();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = colorSurf->mipLevels();
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = colorSurf->arrayLayers();

            m_backend->dispatch().vkCmdPipelineBarrier(
                cb,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier
            );
            colorSurf->setLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
        attachments[attachCount++] = colorSurf->getRenderTargetView(m_renderTargets[0].mipmap, m_renderTargets[0].face);
        fbWidth = colorSurf->width();
        fbHeight = colorSurf->height();
    }
    if (depthSurf) {
        if (depthSurf->currentLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = depthSurf->currentLayout();
            barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            barrier.image = depthSurf->image();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = depthSurf->mipLevels();
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = depthSurf->arrayLayers();

            m_backend->dispatch().vkCmdPipelineBarrier(
                cb,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier
            );
            depthSurf->setLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }
        attachments[attachCount++] = depthSurf->getRenderTargetView(m_depthStencilTarget.mipmap, m_depthStencilTarget.face);
        fbWidth = depthSurf->width();
        fbHeight = depthSurf->height();
    }

    if (m_activeFramebuffer) {
        m_backend->dispatch().vkDestroyFramebuffer(m_backend->device(), m_activeFramebuffer, nullptr);
        m_activeFramebuffer = VK_NULL_HANDLE;
    }

    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_activeRenderPass;
    fbInfo.attachmentCount = attachCount;
    fbInfo.pAttachments = attachments;
    fbInfo.width = fbWidth;
    fbInfo.height = fbHeight;
    fbInfo.layers = 1;

    m_backend->dispatch().vkCreateFramebuffer(m_backend->device(), &fbInfo, nullptr, &m_activeFramebuffer);

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = m_activeRenderPass;
    beginInfo.framebuffer = m_activeFramebuffer;
    beginInfo.renderArea.offset.x = 0;
    beginInfo.renderArea.offset.y = 0;
    beginInfo.renderArea.extent.width = fbWidth;
    beginInfo.renderArea.extent.height = fbHeight;

    m_backend->dispatch().vkCmdBeginRenderPass(cb, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    m_inRenderPass = true;

    return SVGA3_VLKN_SUCCESS;
}

void VlknContext::endRenderPassIfActive() {
    if (m_inRenderPass) {
        VkCommandBuffer cb = m_backend->getActiveCommandBuffer();
        m_backend->dispatch().vkCmdEndRenderPass(cb);
        m_inRenderPass = false;
        m_backend->flushCommandBuffer();
    }
}

Svga3VlknStatus VlknContext::clear(SVGA3dClearFlag flags,
                                  uint32_t colorRGBA,
                                  float depth,
                                  uint32_t stencil,
                                  const SVGA3dRect *rects,
                                  uint32_t numRects)
{
    ensureRenderPassActive();
    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();

    VkClearAttachment attachments[2];
    uint32_t attachCount = 0;

    if (flags & SVGA3D_CLEAR_COLOR) {
        VkClearAttachment &a = attachments[attachCount++];
        a.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        a.colorAttachment = 0;
        /* Unpack RGBA */
        float r = ((colorRGBA >> 16) & 0xFF) / 255.0f;
        float g = ((colorRGBA >> 8) & 0xFF) / 255.0f;
        float b = (colorRGBA & 0xFF) / 255.0f;
        float alpha = ((colorRGBA >> 24) & 0xFF) / 255.0f;
        a.clearValue.color.float32[0] = r;
        a.clearValue.color.float32[1] = g;
        a.clearValue.color.float32[2] = b;
        a.clearValue.color.float32[3] = alpha;
    }

    if ((flags & SVGA3D_CLEAR_DEPTH) || (flags & SVGA3D_CLEAR_STENCIL)) {
        VkClearAttachment &a = attachments[attachCount++];
        a.aspectMask = 0;
        if (flags & SVGA3D_CLEAR_DEPTH) a.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (flags & SVGA3D_CLEAR_STENCIL) a.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        a.colorAttachment = 0;
        a.clearValue.depthStencil.depth = depth;
        a.clearValue.depthStencil.stencil = stencil;
    }

    std::vector<VkClearRect> clearRects;
    if (numRects > 0 && rects) {
        for (uint32_t i = 0; i < numRects; ++i) {
            VkClearRect r = {};
            r.rect.offset.x = (int32_t)rects[i].x;
            r.rect.offset.y = (int32_t)rects[i].y;
            r.rect.extent.width = rects[i].w;
            r.rect.extent.height = rects[i].h;
            r.baseArrayLayer = 0;
            r.layerCount = 1;
            clearRects.push_back(r);
        }
    } else {
        VkClearRect r = {};
        r.rect.offset.x = (int32_t)m_viewport.x;
        r.rect.offset.y = (int32_t)m_viewport.y;
        r.rect.extent.width = (uint32_t)m_viewport.width;
        r.rect.extent.height = (uint32_t)m_viewport.height;
        r.baseArrayLayer = 0;
        r.layerCount = 1;
        clearRects.push_back(r);
    }

    if (attachCount > 0 && !clearRects.empty()) {
        m_backend->dispatch().vkCmdClearAttachments(
            cb, attachCount, attachments, (uint32_t)clearRects.size(), clearRects.data()
        );
    }

    m_clearCount++;
    uint32_t rtSid = m_renderTargets[0].sid;
    if (rtSid != 0 && rtSid != SVGA3D_INVALID_ID) {
        VlknSurface *rtSurf = m_surfaceMgr->getSurface(rtSid);
        if (rtSurf && rtSurf->width() >= 320 && rtSurf->height() >= 240 && !rtSurf->isDepthStencil()) {
            markWindowDrawn(rtSid);
        }
    }
    return SVGA3_VLKN_SUCCESS;
}

VkPipeline VlknContext::getOrCreatePipeline(SVGA3dPrimitiveType primitiveType,
                                            const SVGA3dVertexDecl *decls,
                                            uint32_t numDecls,
                                            VkRenderPass renderPass)
{
    PipelineKey key = {};
    key.topology = (uint32_t)::svga3_primitive_to_vk(primitiveType);

    SVGA3dFillMode fm;
    fm.uintValue = m_renderStates[SVGA3D_RS_FILLMODE];
    key.fillMode = fm.s.mode;

    bool hasDepthAttachment = (m_depthStencilTarget.sid != SVGA3D_INVALID_ID && m_depthStencilTarget.sid != 0);
    key.cullMode = m_renderStates[SVGA3D_RS_CULLMODE];
    key.depthTestEnable = hasDepthAttachment ? m_renderStates[SVGA3D_RS_ZENABLE] : 0;
    key.depthWriteEnable = hasDepthAttachment ? m_renderStates[SVGA3D_RS_ZWRITEENABLE] : 0;
    key.depthFunc = m_renderStates[SVGA3D_RS_ZFUNC];
    key.blendEnable = m_renderStates[SVGA3D_RS_BLENDENABLE];
    key.srcColorBlend = m_renderStates[SVGA3D_RS_SRCBLEND];
    key.dstColorBlend = m_renderStates[SVGA3D_RS_DSTBLEND];
    key.colorBlendEq = m_renderStates[SVGA3D_RS_BLENDEQUATION];
    key.srcAlphaBlend = m_renderStates[SVGA3D_RS_SRCBLENDALPHA];
    key.dstAlphaBlend = m_renderStates[SVGA3D_RS_DSTBLENDALPHA];
    key.alphaBlendEq = m_renderStates[SVGA3D_RS_BLENDEQUATIONALPHA];
    key.stencilEnable = hasDepthAttachment ? m_renderStates[SVGA3D_RS_STENCILENABLE] : 0;
    key.stencilFunc = m_renderStates[SVGA3D_RS_STENCILFUNC];
    key.colorWriteMask = m_renderStates[SVGA3D_RS_COLORWRITEENABLE];
    key.numVertexDecls = numDecls;
    key.renderPass = renderPass;

    /* Hash vertex decls into key */
    if (numDecls > 0 && decls) {
        uint64_t h = 0;
        for (uint32_t i = 0; i < numDecls; ++i) {
            h ^= ((uint64_t)decls[i].identity.type << 24) |
                 ((uint64_t)decls[i].identity.usage << 16) |
                 ((uint64_t)decls[i].identity.usageIndex << 8) |
                 ((uint64_t)decls[i].array.offset << 32) |
                 ((uint64_t)(decls[i].array.surfaceId & 0xFFFF) << 48) |
                 decls[i].array.stride;
            h = (h << 5) | (h >> 59);
        }
        key.vertexDeclHash = h;
    }

    key.boundVS = m_boundVS;
    key.boundPS = m_boundPS;
    bool useFfTex = (m_boundPS == SVGA3D_INVALID_ID && m_stages[0].sid != SVGA3D_INVALID_ID && m_stages[0].sid != 0 && m_surfaceMgr->getSurface(m_stages[0].sid) != nullptr);
    key.ffTextureStage0 = useFfTex ? 1 : 0;
    key.pad = 0;

    auto it = m_pipelineCache.find(key);
    if (it != m_pipelineCache.end()) {
        return it->second;
    }

    /* Create Graphics Pipeline */
    VkGraphicsPipelineCreateInfo pipeInfo = {};
    pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    /* Shader stages */
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = m_defaultVS;
    stages[0].pName = "main";
    if (m_boundVS != SVGA3D_INVALID_ID) {
        auto vit = m_vertexShaders.find(m_boundVS);
        if (vit != m_vertexShaders.end() && vit->second.module) {
            stages[0].module = vit->second.module;
        }
    }

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = useFfTex ? m_defaultFSTex : m_defaultFS;
    stages[1].pName = "main";
    if (m_boundPS != SVGA3D_INVALID_ID) {
        auto pit = m_pixelShaders.find(m_boundPS);
        if (pit != m_pixelShaders.end() && pit->second.module) {
            stages[1].module = pit->second.module;
        }
    }

    pipeInfo.stageCount = 2;
    pipeInfo.pStages = stages;

    /* Vertex Input State */
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;

    if (numDecls > 0 && decls) {
        struct StreamBindingInfo {
            uint32_t surfaceId;
            uint32_t stride;
            VkDeviceSize bufferOffset;
        };
        std::vector<StreamBindingInfo> streamBindings;
        std::vector<uint32_t> declToBinding(numDecls);

        for (uint32_t i = 0; i < numDecls; ++i) {
            uint32_t sid = decls[i].array.surfaceId;
            uint32_t stride = decls[i].array.stride;
            int foundIdx = -1;
            for (size_t s = 0; s < streamBindings.size(); ++s) {
                if (streamBindings[s].surfaceId == sid && streamBindings[s].stride == stride) {
                    foundIdx = (int)s;
                    break;
                }
            }
            if (foundIdx < 0) {
                foundIdx = (int)streamBindings.size();
                StreamBindingInfo info = { sid, stride, 0 };
                streamBindings.push_back(info);
            }
            declToBinding[i] = (uint32_t)foundIdx;
        }

        for (size_t s = 0; s < streamBindings.size(); ++s) {
            uint32_t minOff = 0xFFFFFFFF;
            for (uint32_t i = 0; i < numDecls; ++i) {
                if (declToBinding[i] == s && decls[i].array.offset < minOff) {
                    minOff = decls[i].array.offset;
                }
            }
            if (minOff >= 2048 && minOff != 0xFFFFFFFF) {
                uint32_t align = (streamBindings[s].stride > 0) ? streamBindings[s].stride : 64;
                streamBindings[s].bufferOffset = (minOff / align) * align;
            } else {
                streamBindings[s].bufferOffset = 0;
            }
        }

        for (size_t s = 0; s < streamBindings.size(); ++s) {
            VkVertexInputBindingDescription b = {};
            b.binding = (uint32_t)s;
            b.stride = streamBindings[s].stride;
            b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            bindings.push_back(b);
        }

        for (uint32_t i = 0; i < numDecls; ++i) {
            uint32_t s = declToBinding[i];
            VkVertexInputAttributeDescription a = {};
            a.binding = s;
            uint32_t loc = i;
            if (decls[i].identity.usage == SVGA3D_DECLUSAGE_POSITION) {
                loc = 0;
            } else if (decls[i].identity.usage == SVGA3D_DECLUSAGE_COLOR) {
                loc = (decls[i].identity.usageIndex == 0) ? 1 : 7;
            } else if (decls[i].identity.usage == SVGA3D_DECLUSAGE_TEXCOORD) {
                loc = 2 + decls[i].identity.usageIndex;
            } else if (decls[i].identity.usage == SVGA3D_DECLUSAGE_NORMAL) {
                loc = 6;
            }
            a.location = loc;
            a.format = svga3_decl_type_to_vk((SVGA3dDeclType)decls[i].identity.type);
            a.offset = decls[i].array.offset - (uint32_t)streamBindings[s].bufferOffset;
            attrs.push_back(a);
        }

        uint32_t providedInputMask = 0;
        for (const auto &a : attrs) {
            providedInputMask |= (1u << a.location);
        }

        uint32_t requiredInputMask = (m_boundVS != SVGA3D_INVALID_ID) ?
            m_vertexShaders[m_boundVS].inputLocationMask : m_defaultVsInputMask;

        uint32_t missingInputMask = requiredInputMask & ~providedInputMask;
        if (missingInputMask != 0 && m_dummyVb) {
            uint32_t dummyBindingIdx = (uint32_t)bindings.size();
            VkVertexInputBindingDescription dummyBinding = {};
            dummyBinding.binding = dummyBindingIdx;
            dummyBinding.stride = 0;
            dummyBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            bindings.push_back(dummyBinding);

            for (uint32_t loc = 0; loc < 16; ++loc) {
                if (missingInputMask & (1u << loc)) {
                    VkVertexInputAttributeDescription dummyAttr = {};
                    dummyAttr.binding = dummyBindingIdx;
                    dummyAttr.location = loc;
                    dummyAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                    dummyAttr.offset = 0;
                    attrs.push_back(dummyAttr);
                }
            }
        }
    }

    VkPipelineVertexInputStateCreateInfo viInfo = {};
    viInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    viInfo.vertexBindingDescriptionCount = (uint32_t)bindings.size();
    viInfo.pVertexBindingDescriptions = bindings.data();
    viInfo.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
    viInfo.pVertexAttributeDescriptions = attrs.data();
    pipeInfo.pVertexInputState = &viInfo;

    /* Input Assembly */
    VkPipelineInputAssemblyStateCreateInfo iaInfo = {};
    iaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaInfo.topology = (VkPrimitiveTopology)key.topology;
    iaInfo.primitiveRestartEnable = VK_FALSE;
    pipeInfo.pInputAssemblyState = &iaInfo;

    /* Viewport / Scissor */
    VkPipelineViewportStateCreateInfo vpInfo = {};
    vpInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpInfo.viewportCount = 1;
    vpInfo.pViewports = &m_viewport;
    vpInfo.scissorCount = 1;
    vpInfo.pScissors = &m_scissor;
    pipeInfo.pViewportState = &vpInfo;

    /* Rasterization */
    VkPipelineRasterizationStateCreateInfo rastInfo = {};
    rastInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rastInfo.polygonMode = (key.fillMode == SVGA3D_FILLMODE_LINE) ? VK_POLYGON_MODE_LINE :
                           (key.fillMode == SVGA3D_FILLMODE_POINT) ? VK_POLYGON_MODE_POINT : VK_POLYGON_MODE_FILL;

    switch (key.cullMode) {
        case SVGA3D_FACE_FRONT:
            rastInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
            break;
        case SVGA3D_FACE_BACK:
            rastInfo.cullMode = VK_CULL_MODE_BACK_BIT;
            break;
        case SVGA3D_FACE_FRONT_BACK:
            rastInfo.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
            break;
        case SVGA3D_FACE_NONE:
        default:
            rastInfo.cullMode = VK_CULL_MODE_NONE;
            break;
    }
    rastInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rastInfo.lineWidth = 1.0f;
    pipeInfo.pRasterizationState = &rastInfo;

    /* Multisample */
    VkPipelineMultisampleStateCreateInfo msInfo = {};
    msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipeInfo.pMultisampleState = &msInfo;

    /* Depth / Stencil */
    VkPipelineDepthStencilStateCreateInfo dsInfo = {};
    dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.depthTestEnable = key.depthTestEnable ? VK_TRUE : VK_FALSE;
    dsInfo.depthWriteEnable = key.depthWriteEnable ? VK_TRUE : VK_FALSE;
    dsInfo.depthCompareOp = ::svga3_cmp_func_to_vk((SVGA3dCmpFunc)key.depthFunc);
    dsInfo.stencilTestEnable = key.stencilEnable ? VK_TRUE : VK_FALSE;
    pipeInfo.pDepthStencilState = &dsInfo;

    /* Color Blend */
    VkPipelineColorBlendAttachmentState cbAttach = {};
    cbAttach.blendEnable = key.blendEnable ? VK_TRUE : VK_FALSE;
    cbAttach.srcColorBlendFactor = ::svga3_blend_factor_to_vk((SVGA3dBlendOp)key.srcColorBlend);
    cbAttach.dstColorBlendFactor = ::svga3_blend_factor_to_vk((SVGA3dBlendOp)key.dstColorBlend);
    cbAttach.colorBlendOp = ::svga3_blend_eq_to_vk((SVGA3dBlendEquation)key.colorBlendEq);
    cbAttach.srcAlphaBlendFactor = ::svga3_blend_factor_to_vk((SVGA3dBlendOp)key.srcAlphaBlend);
    cbAttach.dstAlphaBlendFactor = ::svga3_blend_factor_to_vk((SVGA3dBlendOp)key.dstAlphaBlend);
    cbAttach.alphaBlendOp = ::svga3_blend_eq_to_vk((SVGA3dBlendEquation)key.alphaBlendEq);
    cbAttach.colorWriteMask = key.colorWriteMask & 0xF;

    VkPipelineColorBlendStateCreateInfo blendInfo = {};
    blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendInfo.attachmentCount = 1;
    blendInfo.pAttachments = &cbAttach;
    pipeInfo.pColorBlendState = &blendInfo;

    /* Dynamic State */
    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynInfo = {};
    dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynInfo.dynamicStateCount = 2;
    dynInfo.pDynamicStates = dynStates;
    pipeInfo.pDynamicState = &dynInfo;

    pipeInfo.layout = m_defaultPipelineLayout;
    pipeInfo.renderPass = renderPass;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult pipeRes = m_backend->dispatch().vkCreateGraphicsPipelines(
        m_backend->device(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline
    );
    if (pipeRes != VK_SUCCESS) {
        log_msg("[libqemu_svga3d] ERROR: vkCreateGraphicsPipelines failed with VkResult=%d\n", pipeRes);
    }

    m_pipelineCache[key] = pipeline;
    return pipeline;
}

static uint32_t calcVertexCount(SVGA3dPrimitiveType type, uint32_t primCount) {
    switch (type) {
        case SVGA3D_PRIMITIVE_POINTLIST:     return primCount;
        case SVGA3D_PRIMITIVE_LINELIST:      return primCount * 2;
        case SVGA3D_PRIMITIVE_LINESTRIP:     return primCount + 1;
        case SVGA3D_PRIMITIVE_TRIANGLELIST:  return primCount * 3;
        case SVGA3D_PRIMITIVE_TRIANGLESTRIP: return primCount + 2;
        case SVGA3D_PRIMITIVE_TRIANGLEFAN:   return primCount + 2;
        default:                             return primCount * 3;
    }
}

Svga3VlknStatus VlknContext::draw(SVGA3dPrimitiveType primitiveType,
                                 const SVGA3dVertexDecl *decls,
                                 uint32_t numDecls,
                                 const SVGA3dPrimitiveRange *ranges,
                                 uint32_t numRanges)
{
    /* Compute MVP = World * View * Projection when using fixed-function vertex shader */
    if (m_boundVS == SVGA3D_INVALID_ID) {
        std::array<float, 16> world = getTransformOrDefault(SVGA3D_TRANSFORM_WORLD);
        std::array<float, 16> view = getTransformOrDefault(SVGA3D_TRANSFORM_VIEW);
        std::array<float, 16> proj = getTransformOrDefault(SVGA3D_TRANSFORM_PROJECTION);
        std::array<float, 16> wv = multiplyMatrix4x4(world, view);
        std::array<float, 16> mvp = multiplyMatrix4x4(wv, proj);
        bool changed = false;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float val = mvp[row * 4 + col];
                if (m_vsConsts.floatConsts[col][row] != val) {
                    m_vsConsts.floatConsts[col][row] = val;
                    changed = true;
                }
            }
        }
        if (changed) {
            m_constantsDirty = true;
        }
    }

    /* 1. Update Constant Buffers (UBOs) */
    if (m_constantsDirty) {
        endRenderPassIfActive();

        if (m_vsConstBuffer && m_vsConstMemory) {
            void *mapped = nullptr;
            if (m_backend->dispatch().vkMapMemory(m_backend->device(), m_vsConstMemory, 0, sizeof(m_vsConsts.floatConsts), 0, &mapped) == VK_SUCCESS) {
                memcpy(mapped, m_vsConsts.floatConsts, sizeof(m_vsConsts.floatConsts));
                m_backend->dispatch().vkUnmapMemory(m_backend->device(), m_vsConstMemory);
            }
        }
        if (m_psConstBuffer && m_psConstMemory) {
            void *mapped = nullptr;
            if (m_backend->dispatch().vkMapMemory(m_backend->device(), m_psConstMemory, 0, sizeof(m_psConsts.floatConsts), 0, &mapped) == VK_SUCCESS) {
                memcpy(mapped, m_psConsts.floatConsts, sizeof(m_psConsts.floatConsts));
                m_backend->dispatch().vkUnmapMemory(m_backend->device(), m_psConstMemory);
            }
        }
        m_constantsDirty = false;
    }

    /* 2. Update Descriptor Set (only when dirty or not yet initialized) */
    if (m_descriptorSet && (!m_descriptorSetInitialized || m_descriptorSetDirty)) {
        endRenderPassIfActive();

        VkDescriptorBufferInfo vsBufInfo = {};
        vsBufInfo.buffer = m_vsConstBuffer;
        vsBufInfo.offset = 0;
        vsBufInfo.range = sizeof(m_vsConsts.floatConsts);

        VkDescriptorBufferInfo psBufInfo = {};
        psBufInfo.buffer = m_psConstBuffer;
        psBufInfo.offset = 0;
        psBufInfo.range = sizeof(m_psConsts.floatConsts);

        VkDescriptorImageInfo imageInfos[8];
        for (uint32_t i = 0; i < 8; ++i) {
            imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VlknSurface *surf = (m_stages[i].sid != SVGA3D_INVALID_ID && m_stages[i].sid != 0) ? m_surfaceMgr->getSurface(m_stages[i].sid) : nullptr;
            if (surf && surf->imageView()) {
                imageInfos[i].imageView = surf->imageView();
                imageInfos[i].sampler = getOrCreateSampler(i);
            } else {
                imageInfos[i].imageView = m_dummyView;
                imageInfos[i].sampler = m_dummySampler;
            }
        }

        VkWriteDescriptorSet writes[10] = {};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_descriptorSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo = &vsBufInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_descriptorSet;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].pBufferInfo = &psBufInfo;

        for (uint32_t i = 0; i < 8; ++i) {
            writes[2 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2 + i].dstSet = m_descriptorSet;
            writes[2 + i].dstBinding = 2 + i;
            writes[2 + i].descriptorCount = 1;
            writes[2 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2 + i].pImageInfo = &imageInfos[i];
        }

        m_backend->dispatch().vkUpdateDescriptorSets(m_backend->device(), 10, writes, 0, nullptr);
        m_descriptorSetInitialized = true;
        m_descriptorSetDirty = false;
    }

    /* Ensure all bound textures are transitioned to SHADER_READ_ONLY_OPTIMAL */
    for (uint32_t i = 0; i < SVGA3_MAX_TEXTURE_STAGES; ++i) {
        if (m_stages[i].sid != SVGA3D_INVALID_ID && m_stages[i].sid != 0) {
            VlknSurface *surf = m_surfaceMgr->getSurface(m_stages[i].sid);
            if (surf && surf->image() && surf->currentLayout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                endRenderPassIfActive();
                VkCommandBuffer cb = m_backend->getActiveCommandBuffer();
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = surf->currentLayout();
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = (surf->currentLayout() == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) ?
                                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.image = surf->image();
                barrier.subresourceRange.aspectMask = surf->isDepthStencil() ?
                    (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = surf->mipLevels();
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = surf->arrayLayers();

                m_backend->dispatch().vkCmdPipelineBarrier(
                    cb,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &barrier
                );
                surf->setLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }
    }

    /* Pre-calculate vertex buffer stream bindings and ensure capacity BEFORE starting render pass */
    struct StreamBindingInfo {
        uint32_t surfaceId;
        uint32_t stride;
        VkDeviceSize bufferOffset;
    };
    std::vector<StreamBindingInfo> streamBindings;
    std::vector<uint32_t> declToBinding(numDecls);

    if (numDecls > 0 && decls) {
        for (uint32_t i = 0; i < numDecls; ++i) {
            uint32_t sid = decls[i].array.surfaceId;
            uint32_t stride = decls[i].array.stride;
            int foundIdx = -1;
            for (size_t s = 0; s < streamBindings.size(); ++s) {
                if (streamBindings[s].surfaceId == sid && streamBindings[s].stride == stride) {
                    foundIdx = (int)s;
                    break;
                }
            }
            if (foundIdx < 0) {
                foundIdx = (int)streamBindings.size();
                StreamBindingInfo info = { sid, stride, 0 };
                streamBindings.push_back(info);
            }
            declToBinding[i] = (uint32_t)foundIdx;
        }

        for (size_t s = 0; s < streamBindings.size(); ++s) {
            uint32_t minOff = 0xFFFFFFFF;
            for (uint32_t i = 0; i < numDecls; ++i) {
                if (declToBinding[i] == s && decls[i].array.offset < minOff) {
                    minOff = decls[i].array.offset;
                }
            }
            if (minOff >= 2048 && minOff != 0xFFFFFFFF) {
                uint32_t align = (streamBindings[s].stride > 0) ? streamBindings[s].stride : 64;
                streamBindings[s].bufferOffset = (minOff / align) * align;
            } else {
                streamBindings[s].bufferOffset = 0;
            }
        }

        for (size_t s = 0; s < streamBindings.size(); ++s) {
            uint32_t sid = streamBindings[s].surfaceId;
            VlknSurface *surf = m_surfaceMgr->getSurface(sid);
            if (surf) {
                size_t maxNeeded = streamBindings[s].bufferOffset;
                for (uint32_t i = 0; i < numDecls; ++i) {
                    if (declToBinding[i] == s) {
                        size_t attrMax = decls[i].array.offset + (decls[i].array.stride ? decls[i].array.stride * 4096 : 4096);
                        if (attrMax > maxNeeded) maxNeeded = attrMax;
                    }
                }
                if (maxNeeded > surf->bufferSize()) {
                    surf->ensureBufferSize(maxNeeded);
                }
            }
        }
    }

    /* Pre-check and ensure capacity for all index buffers BEFORE starting render pass */
    for (uint32_t i = 0; i < numRanges; ++i) {
        const SVGA3dPrimitiveRange &r = ranges[i];
        if (r.indexArray.surfaceId != SVGA3D_INVALID_ID && r.indexArray.surfaceId != 0 && r.indexArray.stride > 0) {
            VlknSurface *idxSurf = m_surfaceMgr->getSurface(r.indexArray.surfaceId);
            if (idxSurf) {
                SVGA3dPrimitiveType ptype = (r.primType != SVGA3D_PRIMITIVE_INVALID) ? (SVGA3dPrimitiveType)r.primType : primitiveType;
                uint32_t count = calcVertexCount(ptype, r.primitiveCount);
                uint32_t idxStride = r.indexWidth ? r.indexWidth : r.indexArray.stride;
                size_t neededIdxBytes = r.indexArray.offset + count * (idxStride ? idxStride : 2);
                if (neededIdxBytes > idxSurf->bufferSize()) {
                    idxSurf->ensureBufferSize(neededIdxBytes);
                }
            }
        }
    }

    ensureRenderPassActive();
    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();

    if (m_drawCount <= 5 || (m_drawCount % 500) == 0) {
        bool useFfTex = (m_boundPS == SVGA3D_INVALID_ID && m_stages[0].sid != SVGA3D_INVALID_ID && m_stages[0].sid != 0 && m_surfaceMgr->getSurface(m_stages[0].sid) != nullptr);
        log_msg("[libqemu_svga3d] ctx::draw #%u: boundVS=%u, boundPS=%u, useFfTex=%d, stage0.sid=%u, vp=(%.1f,%.1f %.1fx%.1f)\n",
                m_drawCount + 1, m_boundVS, m_boundPS, (int)useFfTex, m_stages[0].sid,
                m_viewport.x, m_viewport.y, m_viewport.width, m_viewport.height);
        log_msg("[libqemu_svga3d]   mvp: [%g, %g, %g, %g] [%g, %g, %g, %g] [%g, %g, %g, %g] [%g, %g, %g, %g]\n",
                m_vsConsts.floatConsts[0][0], m_vsConsts.floatConsts[0][1], m_vsConsts.floatConsts[0][2], m_vsConsts.floatConsts[0][3],
                m_vsConsts.floatConsts[1][0], m_vsConsts.floatConsts[1][1], m_vsConsts.floatConsts[1][2], m_vsConsts.floatConsts[1][3],
                m_vsConsts.floatConsts[2][0], m_vsConsts.floatConsts[2][1], m_vsConsts.floatConsts[2][2], m_vsConsts.floatConsts[2][3],
                m_vsConsts.floatConsts[3][0], m_vsConsts.floatConsts[3][1], m_vsConsts.floatConsts[3][2], m_vsConsts.floatConsts[3][3]);
    }

    /* 3. Bind Pipeline and Descriptor Sets */
    VkPipeline pipeline = getOrCreatePipeline(primitiveType, decls, numDecls, m_activeRenderPass);
    m_backend->dispatch().vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    if (m_descriptorSet) {
        m_backend->dispatch().vkCmdBindDescriptorSets(
            cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_defaultPipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr
        );
    }

    m_backend->dispatch().vkCmdSetViewport(cb, 0, 1, &m_viewport);
    VkRect2D activeScissor = m_scissor;
    if (!m_renderStates[SVGA3D_RS_SCISSORTESTENABLE]) {
        activeScissor.offset.x = (int32_t)m_viewport.x;
        activeScissor.offset.y = (int32_t)m_viewport.y;
        activeScissor.extent.width = (uint32_t)m_viewport.width;
        activeScissor.extent.height = (uint32_t)m_viewport.height;
    }
    m_backend->dispatch().vkCmdSetScissor(cb, 0, 1, &activeScissor);

    /* 4. Bind Vertex Buffers */
    uint32_t numStreamBindings = 0;
    if (numDecls > 0 && decls) {
        for (size_t s = 0; s < streamBindings.size(); ++s) {
            uint32_t sid = streamBindings[s].surfaceId;
            VlknSurface *surf = m_surfaceMgr->getSurface(sid);
            if (surf && surf->buffer()) {
                VkBuffer vbuf = surf->buffer();
                VkDeviceSize offset = streamBindings[s].bufferOffset;
                m_backend->dispatch().vkCmdBindVertexBuffers(cb, (uint32_t)s, 1, &vbuf, &offset);
            }
        }
        numStreamBindings = (uint32_t)streamBindings.size();
    }

    /* Bind dummy vertex buffer if any required shader inputs were missing from decls */
    uint32_t requiredInputMask = (m_boundVS != SVGA3D_INVALID_ID) ?
        m_vertexShaders[m_boundVS].inputLocationMask : m_defaultVsInputMask;
    uint32_t providedInputMask = 0;
    for (uint32_t i = 0; i < numDecls; ++i) {
        uint32_t loc = i;
        if (decls[i].identity.usage == SVGA3D_DECLUSAGE_POSITION) loc = 0;
        else if (decls[i].identity.usage == SVGA3D_DECLUSAGE_COLOR) loc = (decls[i].identity.usageIndex == 0) ? 1 : 7;
        else if (decls[i].identity.usage == SVGA3D_DECLUSAGE_TEXCOORD) loc = 2 + decls[i].identity.usageIndex;
        else if (decls[i].identity.usage == SVGA3D_DECLUSAGE_NORMAL) loc = 6;
        providedInputMask |= (1u << loc);
    }
    if ((requiredInputMask & ~providedInputMask) != 0 && m_dummyVb) {
        VkDeviceSize offset = 0;
        m_backend->dispatch().vkCmdBindVertexBuffers(cb, numStreamBindings, 1, &m_dummyVb, &offset);
    }

    /* 5. Draw Primitive Ranges */
    SVGA3dPrimitiveType currentBoundType = primitiveType;
    for (uint32_t i = 0; i < numRanges; ++i) {
        const SVGA3dPrimitiveRange &r = ranges[i];
        SVGA3dPrimitiveType ptype = (r.primType != SVGA3D_PRIMITIVE_INVALID) ? (SVGA3dPrimitiveType)r.primType : primitiveType;
        if (i == 0 || ptype != currentBoundType) {
            VkPipeline pipe = getOrCreatePipeline(ptype, decls, numDecls, m_activeRenderPass);
            m_backend->dispatch().vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
            currentBoundType = ptype;
        }
        uint32_t count = calcVertexCount(ptype, r.primitiveCount);
        if (r.indexArray.surfaceId != SVGA3D_INVALID_ID && r.indexArray.surfaceId != 0 && r.indexArray.stride > 0) {
            /* Indexed draw */
            VlknSurface *idxSurf = m_surfaceMgr->getSurface(r.indexArray.surfaceId);
            if (idxSurf && idxSurf->buffer()) {
                VkIndexType idxType = (r.indexArray.stride == 2) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                m_backend->dispatch().vkCmdBindIndexBuffer(cb, idxSurf->buffer(), 0, idxType);
            }
            uint32_t idxStride = r.indexWidth ? r.indexWidth : r.indexArray.stride;
            uint32_t firstIndex = (idxStride > 0) ? (r.indexArray.offset / idxStride) : 0;
            m_backend->dispatch().vkCmdDrawIndexed(
                cb, count, 1, firstIndex, r.indexBias, 0
            );
        } else {
            /* Non-indexed draw */
            uint32_t firstVertex = (r.indexBias > 0) ? (uint32_t)r.indexBias : 0;
            m_backend->dispatch().vkCmdDraw(
                cb, count, 1, firstVertex, 0
            );
        }
        m_vertexCount += count;
    }

    m_drawCount += numRanges;
    uint32_t rtSid = m_renderTargets[0].sid;
    if (rtSid != 0 && rtSid != SVGA3D_INVALID_ID) {
        VlknSurface *rtSurf = m_surfaceMgr->getSurface(rtSid);
        if (rtSurf && rtSurf->width() >= 320 && rtSurf->height() >= 240 && !rtSurf->isDepthStencil()) {
            markWindowDrawn(rtSid);
        }
    }
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::beginQuery(SVGA3dQueryType type) {
    if (type >= SVGA3D_QUERYTYPE_MAX) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    if (m_queryActive) return SVGA3_VLKN_ERROR_INVALID_PARAM;

    if (m_queryPool != VK_NULL_HANDLE) {
        VkCommandBuffer cb = m_backend->getActiveCommandBuffer();
        m_backend->dispatch().vkCmdResetQueryPool(cb, m_queryPool, 0, 1);
        m_backend->dispatch().vkCmdBeginQuery(cb, m_queryPool, 0, 0);
    }
    m_queryActive = true;
    m_queryEnded = false;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::endQuery(SVGA3dQueryType type) {
    if (type >= SVGA3D_QUERYTYPE_MAX) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    if (!m_queryActive) return SVGA3_VLKN_ERROR_INVALID_PARAM;

    if (m_queryPool != VK_NULL_HANDLE) {
        VkCommandBuffer cb = m_backend->getActiveCommandBuffer();
        m_backend->dispatch().vkCmdEndQuery(cb, m_queryPool, 0);
        m_backend->flushCommandBuffer();
    }
    m_queryActive = false;
    m_queryEnded = true;
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContext::waitForQuery(SVGA3dQueryType type, uint32_t *outResult) {
    if (type >= SVGA3D_QUERYTYPE_MAX) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    if (!m_queryEnded && !m_queryActive) return SVGA3_VLKN_ERROR_INVALID_PARAM;

    m_backend->waitIdle();
    uint64_t pixels = 0;
    if (m_queryPool != VK_NULL_HANDLE) {
        VkResult res = m_backend->dispatch().vkGetQueryPoolResults(
            m_backend->device(),
            m_queryPool,
            0, 1,
            sizeof(uint64_t),
            &pixels,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );
        if (res == VK_SUCCESS) {
            m_lastQueryResult = (uint32_t)pixels;
        } else {
            m_lastQueryResult = (uint32_t)(m_vertexCount > 0 ? (m_drawCount * 1920) : 0);
        }
    } else {
        m_lastQueryResult = (uint32_t)(m_vertexCount > 0 ? (m_drawCount * 1920) : 0);
    }

    if (outResult) {
        *outResult = m_lastQueryResult;
    }
    m_queryEnded = false;
    return SVGA3_VLKN_SUCCESS;
}

bool VlknContext::isQueryActive(SVGA3dQueryType type) const {
    (void)type;
    return m_queryActive;
}

/* Context Manager */

VlknContextManager::VlknContextManager(VlknBackend *backend, VlknSurfaceManager *surfaceMgr)
    : m_backend(backend)
    , m_surfaceMgr(surfaceMgr)
{}

VlknContextManager::~VlknContextManager() {
    clear();
}

Svga3VlknStatus VlknContextManager::createContext(uint32_t cid) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_contexts.find(cid) != m_contexts.end()) {
        return SVGA3_VLKN_ERROR_ALREADY_EXISTS;
    }

    m_contexts[cid] = std::make_unique<VlknContext>(m_backend, m_surfaceMgr, cid);
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknContextManager::destroyContext(uint32_t cid) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_contexts.find(cid);
    if (it == m_contexts.end()) {
        return SVGA3_VLKN_ERROR_NOT_FOUND;
    }

    m_contexts.erase(it);
    return SVGA3_VLKN_SUCCESS;
}

VlknContext* VlknContextManager::getContext(uint32_t cid) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_contexts.find(cid);
    if (it != m_contexts.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool VlknContextManager::exists(uint32_t cid) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_contexts.find(cid) != m_contexts.end();
}

void VlknContextManager::clear() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_contexts.clear();
}

size_t VlknContextManager::count() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_contexts.size();
}

void VlknContextManager::endAllRenderPasses() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto &pair : m_contexts) {
        if (pair.second) {
            pair.second->endRenderPassIfActive();
        }
    }
}

void VlknContextManager::endAllRenderPassesExcept(uint32_t cid) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto &pair : m_contexts) {
        if (pair.first != cid && pair.second) {
            pair.second->endRenderPassIfActive();
        }
    }
}

std::vector<std::pair<uint32_t, uint32_t>> VlknContextManager::collectPendingWindowPresents() {
    std::vector<std::pair<uint32_t, uint32_t>> list;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto &pair : m_contexts) {
        if (pair.first != 246 && pair.second && pair.second->hasDrawnToWindow()) {
            uint32_t sid = pair.second->lastDrawnWindowSid();
            if (sid != 0 && sid != SVGA3D_INVALID_ID) {
                list.push_back({pair.first, sid});
            }
            pair.second->resetDrawnToWindow();
        }
    }
    return list;
}

} // namespace svga3_vlkn
