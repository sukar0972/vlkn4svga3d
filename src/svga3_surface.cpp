/*
 * SVGA3=VLKN - SVGA3D Surface & Resource Manager Implementation
 */

#include "svga3_surface.h"
#include "svga3_context.h"
#include "svga3_guest_mem.h"
#include "../data/svga3d_reference.h"
#include <cstring>
#include <algorithm>
#include <cmath>

extern "C" void log_msg(const char *fmt, ...);

namespace svga3_vlkn {

static VkComponentMapping sampleComponents(SVGA3dSurfaceFormat format) {
    VkComponentMapping identity = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    switch (format) {
        case SVGA3D_X8R8G8B8: case SVGA3D_X1R5G5B5:
            identity.a = VK_COMPONENT_SWIZZLE_ONE; return identity;
        case SVGA3D_LUMINANCE8: case SVGA3D_LUMINANCE16:
            return {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE};
        case SVGA3D_LUMINANCE8_ALPHA8:
            return {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G};
        case SVGA3D_ALPHA8:
            return {VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R};
        default: return identity;
    }
}

size_t svga3_format_bytes_per_pixel(SVGA3dSurfaceFormat format) {
    switch (format) {
        case SVGA3D_BUFFER:
        case SVGA3D_LUMINANCE8:
        case SVGA3D_ALPHA8:
            return 1;

        case SVGA3D_R5G6B5:
        case SVGA3D_X1R5G5B5:
        case SVGA3D_A1R5G5B5:
        case SVGA3D_A4R4G4B4:
        case SVGA3D_Z_D16:
        case SVGA3D_Z_D15S1:
        case SVGA3D_Z_DF16:
        case SVGA3D_LUMINANCE16:
        case SVGA3D_LUMINANCE8_ALPHA8:
        case SVGA3D_BUMPU8V8:
        case SVGA3D_V8U8:
        case SVGA3D_CxV8U8:
        case SVGA3D_R_S10E5:
            return 2;

        case SVGA3D_X8R8G8B8:
        case SVGA3D_A8R8G8B8:
        case SVGA3D_Z_D32:
        case SVGA3D_Z_D24S8:
        case SVGA3D_Z_D24X8:
        case SVGA3D_Z_DF24:
        case SVGA3D_Z_D24S8_INT:
        case SVGA3D_A2R10G10B10:
        case SVGA3D_A2W10V10U10:
        case SVGA3D_BUMPX8L8V8U8:
        case SVGA3D_X8L8V8U8:
        case SVGA3D_Q8W8V8U8:
        case SVGA3D_R8G8B8A8_UNORM:
        case SVGA3D_R_S23E8:
        case SVGA3D_RG_S10E5:
        case SVGA3D_V16U16:
        case SVGA3D_G16R16:
            return 4;

        case SVGA3D_ARGB_S10E5:
        case SVGA3D_RG_S23E8:
        case SVGA3D_A16B16G16R16:
            return 8;

        case SVGA3D_ARGB_S23E8:
            return 16;

        case SVGA3D_DXT1:
        case SVGA3D_BC4_UNORM:
            return 8; /* 8 bytes per 4x4 block */

        case SVGA3D_DXT2:
        case SVGA3D_DXT3:
        case SVGA3D_DXT4:
        case SVGA3D_DXT5:
        case SVGA3D_BC5_UNORM:
            return 16; /* 16 bytes per 4x4 block */

        default:
            return 4;
    }
}

bool svga3_format_has_stencil(SVGA3dSurfaceFormat format) {
    switch (format) {
        case SVGA3D_Z_D24S8:
        case SVGA3D_Z_D15S1:
        case SVGA3D_Z_D24S8_INT:
            return true;
        default:
            return false;
    }
}

bool svga3_format_is_depth_stencil(SVGA3dSurfaceFormat format) {
    switch (format) {
        case SVGA3D_Z_D32:
        case SVGA3D_Z_D16:
        case SVGA3D_Z_D24S8:
        case SVGA3D_Z_D15S1:
        case SVGA3D_Z_D24X8:
        case SVGA3D_Z_DF16:
        case SVGA3D_Z_DF24:
        case SVGA3D_Z_D24S8_INT:
            return true;
        default:
            return false;
    }
}

bool svga3_format_is_compressed(SVGA3dSurfaceFormat format) {
    switch (format) {
        case SVGA3D_DXT1:
        case SVGA3D_DXT2:
        case SVGA3D_DXT3:
        case SVGA3D_DXT4:
        case SVGA3D_DXT5:
        case SVGA3D_BC4_UNORM:
        case SVGA3D_BC5_UNORM:
            return true;
        default:
            return false;
    }
}

VlknSurface::VlknSurface(VlknBackend *backend,
                         uint32_t sid,
                         uint32_t surfaceFlags,
                         SVGA3dSurfaceFormat format,
                         const SVGA3dSize *sizes,
                         uint32_t numSizes,
                         uint32_t multisampleCount,
                         SVGA3dTextureFilter autogenFilter)
    : m_backend(backend)
    , m_sid(sid)
    , m_flags(surfaceFlags)
    , m_svgaFormat(format)
    , m_vkFormat(VK_FORMAT_UNDEFINED)
    , m_width(0)
    , m_height(0)
    , m_depth(1)
    , m_mipLevels(1)
    , m_arrayLayers(1)
    , m_multisampleCount(multisampleCount ? multisampleCount : 1)
    , m_autogenFilter(autogenFilter)
    , m_isDepthStencil(false)
    , m_isCubeMap(false)
    , m_active(true)
    , m_image(VK_NULL_HANDLE)
    , m_memory(VK_NULL_HANDLE)
    , m_imageView(VK_NULL_HANDLE)
    , m_viewMipLevels(1)
    , m_currentLayout(VK_IMAGE_LAYOUT_UNDEFINED)
    , m_buffer(VK_NULL_HANDLE)
    , m_bufferMemory(VK_NULL_HANDLE)
    , m_bufferSize(0)
    , m_readbackValid(false)
    , m_readbackW(0)
    , m_readbackH(0)
    , m_readbackPitch(0)
{
    m_vkFormat = (VkFormat)svga3d_to_vk_format((uint32_t)format);

    m_isDepthStencil = svga3_format_is_depth_stencil(format);
    m_isCubeMap = (surfaceFlags & SVGA3D_SURFACE_CUBEMAP) != 0;

    if (numSizes > 0 && sizes) {
        /* Cap guest-controlled dimensions: uncapped 32-bit dimensions
         * overflowed the pitch arithmetic below (wrapping to small values)
         * and could request absurd Vulkan images.
         * Note: 1D surfaces (h<=1, d<=1) with large width are legitimate
         * linear buffers (e.g. 32768x1x1 vertex data); they take the
         * pureBuffer path in allocate(), which has its own size cap. */
        uint32_t w0 = sizes[0].width ? sizes[0].width : 1;
        uint32_t h0 = sizes[0].height ? sizes[0].height : 1;
        uint32_t d0 = sizes[0].depth ? sizes[0].depth : 1;
        bool is1D = (h0 <= 1 && d0 <= 1);
        if ((w0 > SVGA3_MAX_SURFACE_DIM && !is1D) || h0 > SVGA3_MAX_SURFACE_DIM ||
            d0 > SVGA3_MAX_SURFACE_DIM) {
            log_msg("[libqemu_svga3d] defineSurface error: dimensions %ux%ux%u exceed max %u (sid=%u)\n",
                    w0, h0, d0, SVGA3_MAX_SURFACE_DIM, sid);
            m_width = 1; m_height = 1; m_depth = 1;
            m_mipLevels = 1; m_arrayLayers = 1;
            m_mips.push_back(SurfaceMipLevel{1, 1, 1, 4, 4, 4});
            m_allocFailed = true;
            m_budgetedBytes = estimatedBytes();
            return;
        }
        m_width = w0;
        m_height = h0;
        m_depth = d0;

        if (m_isCubeMap) {
            m_arrayLayers = 6;
            m_mipLevels = numSizes / 6;
            if (m_mipLevels == 0) m_mipLevels = 1;
        } else {
            m_arrayLayers = 1;
            m_mipLevels = numSizes;
        }
        if (m_mipLevels == 0 || m_mipLevels > SVGA3_MAX_MIP_LEVELS) {
            log_msg("[libqemu_svga3d] defineSurface error: mip levels %u exceeds max %u (sid=%u)\n",
                    m_mipLevels, SVGA3_MAX_MIP_LEVELS, sid);
            m_width = 1; m_height = 1; m_depth = 1;
            m_mipLevels = 1; m_arrayLayers = 1;
            m_mips.push_back(SurfaceMipLevel{1, 1, 1, 4, 4, 4});
            m_allocFailed = true;
            m_budgetedBytes = estimatedBytes();
            return;
        }

        size_t bpp = svga3_format_bytes_per_pixel(format);
        bool compressed = svga3_format_is_compressed(format);

        for (uint32_t i = 0; i < m_mipLevels; ++i) {
            SurfaceMipLevel mip = {};
            mip.width = std::max(1u, m_width >> i);
            mip.height = std::max(1u, m_height >> i);
            mip.depth = std::max(1u, m_depth >> i);

            /* Overflow-checked: guest dimensions are capped above, but
             * belt-and-braces against future changes. */
            size_t rowPitch = 0, slicePitch = 0, totalBytes = 0;
            bool overflow = false;
            if (compressed) {
                uint32_t blocksW = (mip.width + 3) / 4;
                uint32_t blocksH = (mip.height + 3) / 4;
                overflow = __builtin_mul_overflow((size_t)blocksW, bpp, &rowPitch) ||
                           __builtin_mul_overflow(rowPitch, (size_t)blocksH, &slicePitch) ||
                           __builtin_mul_overflow(slicePitch, (size_t)mip.depth, &totalBytes);
            } else {
                overflow = __builtin_mul_overflow((size_t)mip.width, bpp, &rowPitch) ||
                           __builtin_mul_overflow(rowPitch, (size_t)mip.height, &slicePitch) ||
                           __builtin_mul_overflow(slicePitch, (size_t)mip.depth, &totalBytes);
            }
            if (overflow) {
                log_msg("[libqemu_svga3d] defineSurface error: mip pitch overflow (sid=%u)\n", sid);
                m_allocFailed = true;
                m_budgetedBytes = estimatedBytes();
                return;
            }
            mip.rowPitch = rowPitch;
            mip.slicePitch = slicePitch;
            mip.totalBytes = totalBytes;
            m_mips.push_back(mip);
        }
    } else {
        m_width = 1;
        m_height = 1;
        m_depth = 1;
        m_mipLevels = 1;
        m_arrayLayers = 1;
        SurfaceMipLevel mip = { 1, 1, 1, 4, 4, 4 };
        m_mips.push_back(mip);
    }
    m_budgetedBytes = estimatedBytes();
}

VlknSurface::~VlknSurface() {
    destroy();
}

/* Upper bound on the host/device memory this surface will consume, used
 * for the device-wide aggregate budget. Mirrors the pureBuffer decision in
 * allocate(): 1D/buffer surfaces allocate a host-visible buffer of at
 * least 256 KiB; image surfaces allocate one image covering all mips,
 * array layers, and MSAA samples. */
size_t VlknSurface::estimatedBytes() const {
    bool pureBuffer = (m_svgaFormat == SVGA3D_BUFFER) ||
                      ((m_flags & (SVGA3D_SURFACE_HINT_VERTEXBUFFER | SVGA3D_SURFACE_HINT_INDEXBUFFER)) != 0 && m_height <= 1 && m_depth <= 1) ||
                      (m_height <= 1 && m_depth <= 1 && m_width > 16384);
    if (pureBuffer) {
        size_t mip0Bytes = m_mips.empty() ? (size_t)m_width : m_mips[0].totalBytes;
        return std::max((size_t)262144, mip0Bytes);
    }
    /* Clamp the sample factor the way allocate() does: it maps the guest
     * count to a VkSampleCountFlagBits, so charging the same factor keeps
     * the estimate aligned with the real image. */
    uint64_t samples = 1;
    if (m_multisampleCount >= 16) samples = 16;
    else if (m_multisampleCount >= 8) samples = 8;
    else if (m_multisampleCount >= 4) samples = 4;
    else if (m_multisampleCount >= 2) samples = 2;
    uint64_t total = 0;
    for (const auto &mip : m_mips) {
        total += (uint64_t)mip.totalBytes;
    }
    total *= (uint64_t)m_arrayLayers * samples;
    return total > (uint64_t)SIZE_MAX ? SIZE_MAX : (size_t)total;
}

Svga3VlknStatus VlknSurface::allocate() {
    if (m_svgaFormat != SVGA3D_BUFFER && m_vkFormat == VK_FORMAT_UNDEFINED) {
        log_msg("[libqemu_svga3d] Unsupported surface format %u for sid=%u\n", m_svgaFormat, m_sid);
        return SVGA3_VLKN_ERROR_UNSUPPORTED_FORMAT;
    }
    if (m_multisampleCount > 1) {
        log_msg("[libqemu_svga3d] Multisample surfaces are unsupported (sid=%u samples=%u)\n",m_sid,m_multisampleCount);
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }
    const bool compressed = svga3_format_is_compressed(m_svgaFormat);
    if (compressed && (m_flags & SVGA3D_SURFACE_HINT_RENDERTARGET))
        return SVGA3_VLKN_ERROR_UNSUPPORTED_FORMAT;
    if (m_allocFailed) {
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }
    bool pureBuffer = (m_svgaFormat == SVGA3D_BUFFER) ||
                      ((m_flags & (SVGA3D_SURFACE_HINT_VERTEXBUFFER | SVGA3D_SURFACE_HINT_INDEXBUFFER)) != 0 && m_height <= 1 && m_depth <= 1) ||
                      (m_height <= 1 && m_depth <= 1 && m_width > 16384);
    if (pureBuffer) {
        m_image = VK_NULL_HANDLE;
        m_memory = VK_NULL_HANDLE;
        m_imageView = VK_NULL_HANDLE;
        m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        size_t mip0Bytes = m_mips.empty() ? m_width : m_mips[0].totalBytes;
        /* Cap buffer size: a 1D surface with width 2^30 would otherwise
         * allocate 1GB+ of host memory (H3). 256MB is generous for
         * vertex/index/constant buffers. */
        static const size_t MAX_BUFFER_BYTES = 256 * 1024 * 1024;
        if (mip0Bytes > MAX_BUFFER_BYTES) {
            log_msg("[libqemu_svga3d] allocate error: buffer size %zu exceeds max %zu (sid=%u)\n",
                    mip0Bytes, MAX_BUFFER_BYTES, m_sid);
            m_allocFailed = true;
            return SVGA3_VLKN_ERROR_INVALID_PARAM;
        }
        m_bufferSize = std::max((size_t)262144, mip0Bytes);
        Svga3VlknStatus st = m_backend->createBuffer(
            m_bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &m_buffer, &m_bufferMemory
        );
        if (st == SVGA3_VLKN_SUCCESS && m_bufferMemory) {
            void *p = nullptr;
            if (m_backend->dispatch().vkMapMemory(m_backend->device(), m_bufferMemory, 0, m_bufferSize, 0, &p) == VK_SUCCESS) {
                memset(p, 0, m_bufferSize);
                m_bufferMapped = p;
            }
        }
        return st;
    }

    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = ((m_depth > 1 || m_volumeImage) && !m_isCubeMap) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    imgInfo.format = m_vkFormat;
    imgInfo.extent.width = m_width;
    imgInfo.extent.height = m_height;
    imgInfo.extent.depth = m_depth;
    imgInfo.mipLevels = m_mipLevels;
    imgInfo.arrayLayers = m_arrayLayers;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    if (m_multisampleCount >= 16) samples = (VkSampleCountFlagBits)0x00000010;
    else if (m_multisampleCount >= 8) samples = (VkSampleCountFlagBits)0x00000008;
    else if (m_multisampleCount >= 4) samples = (VkSampleCountFlagBits)0x00000004;
    else if (m_multisampleCount >= 2) samples = (VkSampleCountFlagBits)0x00000002;
    imgInfo.samples = samples;
    if (imgInfo.imageType == VK_IMAGE_TYPE_3D) imgInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT;

    if (m_isDepthStencil) {
        imgInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    } else if (!compressed) {
        imgInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (m_isCubeMap) {
        imgInfo.flags |= 0x00000010; /* VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT */
    }

    VkResult res = m_backend->dispatch().vkCreateImage(m_backend->device(), &imgInfo, nullptr, &m_image);
    if (res != VK_SUCCESS) {
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }

    VkMemoryRequirements memReqs;
    m_backend->dispatch().vkGetImageMemoryRequirements(m_backend->device(), m_image, &memReqs);

    int memType = m_backend->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType < 0) {
        m_backend->dispatch().vkDestroyImage(m_backend->device(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }
    Svga3VlknStatus st = m_backend->allocateMemory(memReqs.size, memType, &m_memory);
    if (st != SVGA3_VLKN_SUCCESS) {
        m_backend->dispatch().vkDestroyImage(m_backend->device(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        return st;
    }

    VkResult bindRes = m_backend->dispatch().vkBindImageMemory(m_backend->device(), m_image, m_memory, 0);
    if (bindRes != VK_SUCCESS) {
        /* Without bound memory the image is unusable: fail the allocation
         * and release both resources instead of building a view on top of
         * an unbound image. */
        log_msg("[libqemu_svga3d] allocate: vkBindImageMemory failed (%d)\n", bindRes);
        m_backend->dispatch().vkDestroyImage(m_backend->device(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        m_backend->freeMemory(m_memory);
        m_memory = VK_NULL_HANDLE;
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }

    /* Create default ImageView for full resource sampling */
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    if (m_isCubeMap) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    } else if (m_depth > 1 || m_volumeImage) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    } else {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    }
    viewInfo.format = m_vkFormat;
    viewInfo.components = sampleComponents(m_svgaFormat);
    /* Sample only depth; attachment views retain both depth and stencil. */
    viewInfo.subresourceRange.aspectMask = m_isDepthStencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    m_viewMipLevels = (m_autogenFilter != SVGA3D_TEX_FILTER_NONE) ? m_mipLevels : 1;
    viewInfo.subresourceRange.levelCount = m_viewMipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = m_arrayLayers;

    res = m_backend->dispatch().vkCreateImageView(m_backend->device(), &viewInfo, nullptr, &m_imageView);
    if (res != VK_SUCCESS) {
        /* Release the image and its memory: returning with them still
         * attached leaked both on every view-creation failure. */
        log_msg("[libqemu_svga3d] allocate: vkCreateImageView failed (%d)\n", res);
        m_imageView = VK_NULL_HANDLE;
        m_backend->dispatch().vkDestroyImage(m_backend->device(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        m_backend->freeMemory(m_memory);
        m_memory = VK_NULL_HANDLE;
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }

    m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    /* Images only need a second buffer when used as vertex/index data.
     * Linear DMA can allocate it lazily through ensureBufferSize(). */
    if (m_flags & (SVGA3D_SURFACE_HINT_VERTEXBUFFER | SVGA3D_SURFACE_HINT_INDEXBUFFER)) {
        size_t mip0Bytes = m_mips.empty() ? 4096 : m_mips[0].totalBytes;
        m_bufferSize = std::max((size_t)262144, mip0Bytes);
        Svga3VlknStatus bufferSt = m_backend->createBuffer(
            m_bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &m_buffer,
            &m_bufferMemory
        );
        if (bufferSt != SVGA3_VLKN_SUCCESS) return bufferSt;

        if (m_bufferMemory) {
            void *p = nullptr;
            if (m_backend->dispatch().vkMapMemory(m_backend->device(), m_bufferMemory, 0, m_bufferSize, 0, &p) == VK_SUCCESS) {
                memset(p, 0, m_bufferSize);
                m_bufferMapped = p;
            }
        }

    }
    return SVGA3_VLKN_SUCCESS;
}

// SVGA's old surface definition has no volume flag when depth is one.
// Promote it once the shader's sampler declaration supplies that information.
Svga3VlknStatus VlknSurface::ensureVolumeImage() {
    if (m_depth > 1 || m_volumeImage) return SVGA3_VLKN_SUCCESS;
    if (!m_image || m_isCubeMap || m_buffer) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    auto status = m_backend->flushCommandBuffer();
    if (status != SVGA3_VLKN_SUCCESS) return status;
    VkImage oldImage = m_image;
    VkDeviceMemory oldMemory = m_memory;
    VkImageView oldView = m_imageView;
    VkImageLayout oldLayout = m_currentLayout;
    uint32_t oldLevels = m_viewMipLevels;
    m_image = VK_NULL_HANDLE; m_memory = VK_NULL_HANDLE; m_imageView = VK_NULL_HANDLE;
    m_volumeImage = true;
    status = allocate();
    if (status != SVGA3_VLKN_SUCCESS) {
        m_volumeImage = false;
        m_image = oldImage; m_memory = oldMemory; m_imageView = oldView;
        m_currentLayout = oldLayout; m_viewMipLevels = oldLevels;
        return status;
    }
    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = oldLayout; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = oldImage;
    barrier.subresourceRange.aspectMask = nativeAspectMask();
    barrier.subresourceRange.levelCount = m_mipLevels; barrier.subresourceRange.layerCount = 1;
    m_backend->dispatch().vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    transitionLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    for (uint32_t level = 0; level < m_mipLevels; ++level) {
        VkImageCopy region = {};
        region.srcSubresource.aspectMask = m_isDepthStencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.mipLevel = level; region.srcSubresource.layerCount = 1;
        region.dstSubresource = region.srcSubresource;
        region.extent = {m_mips[level].width, m_mips[level].height, 1};
        m_backend->dispatch().vkCmdCopyImage(cb, oldImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        if (svga3_format_has_stencil(m_svgaFormat)) {
            region.srcSubresource.aspectMask = region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
            m_backend->dispatch().vkCmdCopyImage(cb, oldImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }
    }
    status = m_backend->flushCommandBuffer();
    m_backend->dispatch().vkDestroyImageView(m_backend->device(), oldView, nullptr);
    for (auto &view : m_rtViews) m_backend->dispatch().vkDestroyImageView(m_backend->device(), view.second, nullptr);
    m_rtViews.clear();
    m_backend->dispatch().vkDestroyImage(m_backend->device(), oldImage, nullptr);
    m_backend->freeMemory(oldMemory);
    invalidateReadback();
    return status;
}

/* The tracker describes the entire image, so every transition covers every
 * mip and face. Transfer regions still select the requested subresource. */
void VlknSurface::transitionLayout(VkCommandBuffer cb, VkImageLayout layout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = m_currentLayout;
    barrier.newLayout = layout;
    barrier.srcAccessMask = m_currentLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 :
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = nativeAspectMask();
    barrier.subresourceRange.levelCount = m_mipLevels;
    barrier.subresourceRange.layerCount = m_arrayLayers;
    m_backend->dispatch().vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    m_currentLayout = layout;
}

Svga3VlknStatus VlknSurface::ensureBufferSize(size_t requiredSize) {
    if (requiredSize <= m_bufferSize) {
        return SVGA3_VLKN_SUCCESS;
    }
    /* Cap growth: a single guest command must not be able to demand
     * gigabytes of host memory. */
    if (requiredSize > SVGA3_MAX_DMA_BYTES) {
        log_msg("[libqemu_svga3d] ensureBufferSize error: %zu exceeds max %zu (sid=%u)\n",
                requiredSize, SVGA3_MAX_DMA_BYTES, m_sid);
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }

    size_t newSize = m_bufferSize ? m_bufferSize : 262144;
    while (newSize < requiredSize) {
        /* Overflow-checked growth: newSize*3 used to wrap past SIZE_MAX
         * for huge requiredSize, spinning forever. */
        size_t grown = 0;
        if (__builtin_mul_overflow(newSize, 3, &grown)) {
            grown = requiredSize;
        } else {
            grown = grown / 2;
        }
        if (grown <= newSize) {
            grown = requiredSize;
        }
        newSize = grown;
    }
    newSize = (newSize + 65535) & ~((size_t)65535);

    /* Aggregate budget: buffer growth is guest-triggered host memory.
     * Reserve the delta before allocating; release it if creation fails. */
    size_t growthDelta = 0;
    if (newSize > m_budgetedBytes) {
        growthDelta = newSize - m_budgetedBytes;
        if (!m_backend->resourceBudgets().tryReserveSurfaceBytes(growthDelta)) {
            log_msg("[libqemu_svga3d] ensureBufferSize error: aggregate surface budget exhausted (sid=%u)\n",
                    m_sid);
            return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
        }
    }

    /* The old allocation can be in flight, and its contents must be stable
     * while copied into the replacement buffer. */
    Svga3VlknStatus flushSt = m_backend->flushCommandBuffer();
    if (flushSt != SVGA3_VLKN_SUCCESS) {
        if (growthDelta > 0) {
            m_backend->resourceBudgets().releaseSurfaceBytes(growthDelta);
        }
        return flushSt;
    }

    VkBuffer newBuffer = VK_NULL_HANDLE;
    VkDeviceMemory newMemory = VK_NULL_HANDLE;

    Svga3VlknStatus st = m_backend->createBuffer(
        newSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &newBuffer,
        &newMemory
    );
    if (st != SVGA3_VLKN_SUCCESS) {
        if (growthDelta > 0) {
            m_backend->resourceBudgets().releaseSurfaceBytes(growthDelta);
        }
        log_msg("[libqemu_svga3d] ERROR: Failed to expand buffer for sid=%u to %zu bytes\n", m_sid, newSize);
        return st;
    }

    void *newMapped = nullptr;
    if (m_backend->dispatch().vkMapMemory(m_backend->device(), newMemory, 0, newSize, 0, &newMapped) == VK_SUCCESS) {
        memset(newMapped, 0, newSize);
        if (m_bufferMapped && m_bufferSize > 0) {
            memcpy(newMapped, m_bufferMapped, m_bufferSize);
        } else if (m_bufferMemory && m_bufferSize > 0) {
            void *oldMapped = nullptr;
            if (m_backend->dispatch().vkMapMemory(m_backend->device(), m_bufferMemory, 0, m_bufferSize, 0, &oldMapped) == VK_SUCCESS) {
                memcpy(newMapped, oldMapped, m_bufferSize);
                m_backend->dispatch().vkUnmapMemory(m_backend->device(), m_bufferMemory);
            } else {
                m_backend->dispatch().vkUnmapMemory(m_backend->device(), newMemory);
                m_backend->destroyBuffer(newBuffer, newMemory);
                if (growthDelta) m_backend->resourceBudgets().releaseSurfaceBytes(growthDelta);
                return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
            }
        }
    } else {
        m_backend->destroyBuffer(newBuffer, newMemory);
        if (growthDelta) m_backend->resourceBudgets().releaseSurfaceBytes(growthDelta);
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }

    if (m_bufferMapped && m_bufferMemory) {
        m_backend->dispatch().vkUnmapMemory(m_backend->device(), m_bufferMemory);
        m_bufferMapped = nullptr;
    }
    if (m_buffer) {
        m_backend->dispatch().vkDestroyBuffer(m_backend->device(), m_buffer, nullptr);
    }
    if (m_bufferMemory) {
        m_backend->freeMemory(m_bufferMemory);
    }

    m_buffer = newBuffer;
    m_bufferMemory = newMemory;
    m_bufferMapped = newMapped;
    m_bufferSize = newSize;
    if (growthDelta > 0) {
        m_budgetedBytes = newSize;
    }

    log_msg("[libqemu_svga3d] Expanded buffer for sid=%u to %zu bytes\n", m_sid, m_bufferSize);
    return SVGA3_VLKN_SUCCESS;
}

bool VlknSurface::ensureViewMipLevels(uint32_t levels) {
    uint32_t targetLevels = std::min(levels, m_mipLevels);
    if (m_image == VK_NULL_HANDLE) {
        return false;
    }
    if (targetLevels <= m_viewMipLevels) {
        return true;
    }
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    if (m_isCubeMap) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    } else if (m_depth > 1 || m_volumeImage) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    } else {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    }
    viewInfo.format = m_vkFormat;
    viewInfo.components = sampleComponents(m_svgaFormat);
    /* Sample only depth; attachment views retain both depth and stencil. */
    viewInfo.subresourceRange.aspectMask = m_isDepthStencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = targetLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = m_arrayLayers;

    /* Create the wider view before dropping the old one: on failure the
     * surface keeps its previous view and level count, so a mip-filtered
     * draw degrades to the levels that view covers instead of binding a
     * destroyed or null view. */
    VkImageView newView = VK_NULL_HANDLE;
    VkResult result = m_backend->dispatch().vkCreateImageView(
        m_backend->device(), &viewInfo, nullptr, &newView);
    if (result != VK_SUCCESS || newView == VK_NULL_HANDLE) {
        log_msg("[libqemu_svga3d] ensureViewMipLevels: vkCreateImageView failed (%d) for sid=%u; keeping %u-level view\n",
                result, m_sid, m_viewMipLevels);
        return false;
    }
    if (m_imageView != VK_NULL_HANDLE) {
        m_previousSampledViews.push_back(m_imageView);
    }
    m_imageView = newView;
    m_viewMipLevels = targetLevels;
    return true;
}

void VlknSurface::destroy() {
    if (m_backend) {
        m_backend->flushCommandBuffer();
    }

    if (m_bufferMapped && m_bufferMemory) {
        m_backend->dispatch().vkUnmapMemory(m_backend->device(), m_bufferMemory);
        m_bufferMapped = nullptr;
    }
    if (m_buffer) {
        m_backend->dispatch().vkDestroyBuffer(m_backend->device(), m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    if (m_bufferMemory) {
        m_backend->freeMemory(m_bufferMemory);
        m_bufferMemory = VK_NULL_HANDLE;
    }
    m_bufferSize = 0;

    for (auto &pair : m_rtViews) {
        if (pair.second) {
            m_backend->dispatch().vkDestroyImageView(m_backend->device(), pair.second, nullptr);
        }
    }
    m_rtViews.clear();

    for (VkImageView view : m_previousSampledViews) {
        m_backend->dispatch().vkDestroyImageView(m_backend->device(), view, nullptr);
    }
    m_previousSampledViews.clear();

    if (m_imageView) {
        m_backend->dispatch().vkDestroyImageView(m_backend->device(), m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }

    if (m_image) {
        m_backend->dispatch().vkDestroyImage(m_backend->device(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }

    if (m_memory) {
        m_backend->freeMemory(m_memory);
        m_memory = VK_NULL_HANDLE;
    }
}

VkImageView VlknSurface::getRenderTargetView(uint32_t mip, uint32_t face) {
    /* Validate against the surface: out-of-range levels/faces make
     * vkCreateImageView fail or misbehave, and each unique (mip,face)
     * caches a view — unbounded without this check. */
    if (mip >= m_mipLevels || face >= m_arrayLayers) {
        return VK_NULL_HANDLE;
    }
    /* Bound the view cache: each entry holds a driver-side VkImageView. */
    if (m_rtViews.size() >= SVGA3_MAX_RT_VIEWS) {
        return VK_NULL_HANDLE;
    }
    uint64_t key = ((uint64_t)mip << 32) | (uint64_t)face;
    auto it = m_rtViews.find(key);
    if (it != m_rtViews.end()) {
        return it->second;
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_vkFormat;
    viewInfo.subresourceRange.aspectMask = nativeAspectMask();
    viewInfo.subresourceRange.baseMipLevel = mip;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = face;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView view = VK_NULL_HANDLE;
    VkResult viewRes = m_backend->dispatch().vkCreateImageView(m_backend->device(), &viewInfo, nullptr, &view);
    if (viewRes != VK_SUCCESS) {
        /* Do not cache the failure: a null entry would poison this
         * (mip, face) pair for the surface's whole lifetime, and callers
         * already treat a null return as "no view". */
        log_msg("[libqemu_svga3d] getRenderTargetView: vkCreateImageView failed (%d)\n", viewRes);
        return VK_NULL_HANDLE;
    }
    m_rtViews[key] = view;
    return view;
}

const SurfaceMipLevel* VlknSurface::getMipInfo(uint32_t mipLevel) const {
    if (mipLevel < m_mips.size()) {
        return &m_mips[mipLevel];
    }
    return nullptr;
}

/* Guest D24 stores depth in bits 31:8 and stencil in bits 7:0. Vulkan
 * buffer copies expose separate depth and stencil planes; copying the
 * guest word directly loses stencil and puts depth in the wrong bits. */
Svga3VlknStatus VlknSurface::dmaPackedDepth(bool upload, uint32_t mipLevel,
                                          const SVGA3dBox *box, void *guestData,
                                          size_t guestStride, uint32_t face) {
    const auto &mip = m_mips[mipLevel];
    const SVGA3dBox area = box ? *box : SVGA3dBox{0, 0, 0, mip.width, mip.height, mip.depth};
    const size_t pixels = size_t(area.w) * area.h * area.d;
    const bool stencil = svga3_format_has_stencil(m_svgaFormat);
    if (pixels > SIZE_MAX / (stencil ? 5 : 4)) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    const size_t depthBytes = pixels * 4;
    const size_t bytes = depthBytes + (stencil ? pixels : 0);
    const size_t stride = guestStride ? guestStride : size_t(area.w) * 4;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    auto st = m_backend->createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer, &memory);
    if (st != SVGA3_VLKN_SUCCESS) return st;
    void *mapped = nullptr;
    if (m_backend->dispatch().vkMapMemory(m_backend->device(), memory, 0, bytes, 0, &mapped) != VK_SUCCESS || !mapped) {
        m_backend->destroyBuffer(buffer, memory);
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }
    auto *plane = static_cast<uint8_t*>(mapped);
    auto *guest = static_cast<uint8_t*>(guestData);
    if (upload) {
        for (size_t i = 0; i < pixels; ++i) {
            uint32_t packed;
            memcpy(&packed, guest + (i / area.w) * stride + (i % area.w) * 4, 4);
            if (m_vkFormat == VK_FORMAT_D32_SFLOAT) {
                const float depth = float(packed >> 8) / 16777215.0f;
                memcpy(plane + i * 4, &depth, 4);
            } else {
                const uint32_t depth = packed >> 8;
                memcpy(plane + i * 4, &depth, 4);
            }
            if (stencil) plane[depthBytes + i] = uint8_t(packed);
        }
    }
    const VkImageAspectFlags aspects = nativeAspectMask();
    const VkImageLayout transferLayout = upload ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = m_currentLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = upload ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = m_currentLayout;
    barrier.newLayout = transferLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange = {aspects, 0, m_mipLevels, 0, m_arrayLayers};
    m_backend->dispatch().vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy regions[2] = {};
    regions[0].imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, mipLevel, face, 1};
    regions[0].imageOffset = {int32_t(area.x), int32_t(area.y), int32_t(area.z)};
    regions[0].imageExtent = {area.w, area.h, area.d};
    regions[1] = regions[0];
    regions[1].bufferOffset = depthBytes;
    regions[1].imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    if (upload) m_backend->dispatch().vkCmdCopyBufferToImage(cb, buffer, m_image, transferLayout, stencil ? 2 : 1, regions);
    else m_backend->dispatch().vkCmdCopyImageToBuffer(cb, m_image, transferLayout, buffer, stencil ? 2 : 1, regions);
    barrier.oldLayout = transferLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = upload ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    m_backend->dispatch().vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
    m_currentLayout = barrier.newLayout;
    if (!upload) m_backend->recordHostReadBarrier(cb, buffer, 0, bytes);
    st = m_backend->flushCommandBuffer();
    if (st == SVGA3_VLKN_SUCCESS && !upload) {
        for (size_t i = 0; i < pixels; ++i) {
            uint32_t depth;
            if (m_vkFormat == VK_FORMAT_D32_SFLOAT) {
                float value;
                memcpy(&value, plane + i * 4, 4);
                depth = uint32_t(std::lround(std::clamp(double(value), 0.0, 1.0) * 16777215.0));
            } else memcpy(&depth, plane + i * 4, 4);
            const uint32_t packed = ((depth & 0xFFFFFF) << 8) | (stencil ? plane[depthBytes + i] : 0);
            memcpy(guest + (i / area.w) * stride + (i % area.w) * 4, &packed, 4);
        }
    }
    m_backend->dispatch().vkUnmapMemory(m_backend->device(), memory);
    m_backend->destroyBuffer(buffer, memory);
    return st;
}

Svga3VlknStatus VlknSurface::dmaUpload(uint32_t mipLevel,
                                      const SVGA3dBox *box,
                                      const void *guestData,
                                      size_t guestStride,
                                      bool isLinear, uint32_t face)
{
    if (!guestData || (mipLevel >= m_mipLevels || face >= m_arrayLayers)) {
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }
    invalidateReadback();

    const SurfaceMipLevel &mip = m_mips[mipLevel];
    uint32_t bw = box ? box->w : mip.width;
    uint32_t bh = box ? box->h : mip.height;
    uint32_t bd = box ? box->d : mip.depth;
    uint32_t bx = box ? box->x : 0;
    uint32_t by = box ? box->y : 0;
    uint32_t bz = box ? box->z : 0;

    bool isLinearBuffer = isLinear || (m_svgaFormat == SVGA3D_BUFFER) ||
                          (m_image == VK_NULL_HANDLE) ||
                          (((m_flags & (SVGA3D_SURFACE_HINT_VERTEXBUFFER | SVGA3D_SURFACE_HINT_INDEXBUFFER)) != 0) && m_height <= 1 && m_depth <= 1) ||
                          (m_height <= 1 && m_depth <= 1 && (bw > mip.width || (bx + bw) > mip.width));

    if (isLinearBuffer) {
        uint64_t offset64 = bx;
        uint64_t len64 = bw;
        uint64_t end64 = offset64 + len64;
        if (end64 > m_bufferSize) {
            if (end64 > SVGA3_MAX_DMA_BYTES) {
                log_msg("[libqemu_svga3d] dmaUpload error: linear range %lu exceeds max %lu (sid=%u)\n",
                        (unsigned long)end64, (unsigned long)SVGA3_MAX_DMA_BYTES, m_sid);
                return SVGA3_VLKN_ERROR_INVALID_PARAM;
            }
            Svga3VlknStatus ensSt = ensureBufferSize((size_t)end64);
            if (ensSt != SVGA3_VLKN_SUCCESS) return ensSt;
        }
        uint32_t offset = bx;
        size_t len = bw;
        if (!m_buffer || !m_bufferMemory) return SVGA3_VLKN_ERROR_INVALID_PARAM;

        /* Preserve recorded vertices/indices without a queue-idle per upload.
         * Charge both allocations until the retired one is completed. */
        if (m_lastBoundDrawSerial > m_backend->completedSubmissionSerial()) {
            const bool reserved = m_backend->resourceBudgets().tryReserveSurfaceBytes(m_bufferSize);
            VkBuffer newBuffer = VK_NULL_HANDLE;
            VkDeviceMemory newMemory = VK_NULL_HANDLE;
            void *newMapped = nullptr;
            Svga3VlknStatus st = reserved ? m_backend->createBuffer(
                m_bufferSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &newBuffer, &newMemory) : SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
            if (st == SVGA3_VLKN_SUCCESS && m_bufferMapped &&
                m_backend->dispatch().vkMapMemory(m_backend->device(), newMemory, 0, m_bufferSize, 0, &newMapped) == VK_SUCCESS) {
                memcpy(newMapped, m_bufferMapped, m_bufferSize);
                m_backend->retireBuffer(m_buffer, m_bufferMemory, m_bufferMapped,
                                       m_lastBoundDrawSerial, m_bufferSize);
                m_buffer = newBuffer;
                m_bufferMemory = newMemory;
                m_bufferMapped = newMapped;
                m_lastBoundDrawSerial = 0;
            } else {
                if (newBuffer || newMemory) m_backend->destroyBuffer(newBuffer, newMemory);
                if (reserved) m_backend->resourceBudgets().releaseSurfaceBytes(m_bufferSize);
                st = m_backend->flushCommandBuffer();
                if (st != SVGA3_VLKN_SUCCESS) return st;
            }
        }

        if (offset < m_bufferSize) {
            if (!m_bufferMapped) {
                m_backend->dispatch().vkMapMemory(m_backend->device(), m_bufferMemory, 0, m_bufferSize, 0, &m_bufferMapped);
            }
            if (m_bufferMapped) {
                size_t copyLen = std::min(static_cast<size_t>(len), m_bufferSize - offset);
                memcpy(static_cast<uint8_t*>(m_bufferMapped) + offset, guestData, copyLen);
            } else {
                log_msg("[libqemu_svga3d] dmaUpload error: vkMapMemory failed (sid=%u, bufSize=%zu)\n",
                        m_sid, m_bufferSize);
                return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
            }
        }
        return SVGA3_VLKN_SUCCESS;
    }

    const bool compressed = svga3_format_is_compressed(m_svgaFormat);
    const size_t block = compressed ? 4 : 1;
    const size_t bpp = svga3_format_bytes_per_pixel(m_svgaFormat);
    if (!bw || !bh || !bd || !bpp || uint64_t(bx) + bw > mip.width ||
        uint64_t(by) + bh > mip.height || uint64_t(bz) + bd > mip.depth ||
        (compressed && ((bx % block) || (by % block) ||
         ((bw % block) && uint64_t(bx) + bw != mip.width) ||
         ((bh % block) && uint64_t(by) + bh != mip.height))))
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    const size_t copyRowBytes = ((size_t(bw) + block - 1) / block) * bpp;
    const size_t copyRows = (size_t(bh) + block - 1) / block;
    if (copyRowBytes > SIZE_MAX / copyRows / bd)
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    const size_t totalBytes = copyRowBytes * copyRows * bd;
    if (guestStride && guestStride < copyRowBytes)
        return SVGA3_VLKN_ERROR_INVALID_PARAM;

    /* 64-bit comparisons: the 32-bit sums wrapped for large boxes,
     * letting out-of-image boxes pass and driving negative Vulkan extents. */
    bool fitsInImage = (m_image != VK_NULL_HANDLE) &&
                       ((uint64_t)bx + bw <= mip.width) &&
                       ((uint64_t)by + bh <= mip.height) &&
                       ((uint64_t)bz + bd <= mip.depth);

    if (!fitsInImage) {
        log_msg("[libqemu_svga3d] dmaUpload error: box does not fit in image (sid=%u, box=(%u,%u %ux%u), mip=(%ux%u))\n",
                m_sid, bx, by, bw, bh, mip.width, mip.height);
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }

    if (mipLevel > 0) {
        ensureViewMipLevels(mipLevel + 1);
    }
    if (isPackedDepth()) return dmaPackedDepth(true, mipLevel, box, const_cast<void*>(guestData), guestStride, face);

    /* Validate the linear shadow-copy range BEFORE recording anything: a
     * failure here must not leave a half-recorded barrier in the open
     * command buffer with m_currentLayout unchanged. */
    const bool doLinearShadowCopy = (mipLevel == 0 && m_buffer && m_bufferMemory && !compressed);
    if (doLinearShadowCopy) {
        /* 64-bit: (bx+bw) wrapped in 32-bit, undersizing the buffer. */
        uint64_t bxw = (uint64_t)bx + bw;
        uint64_t bzw = (uint64_t)bz + bd;
        uint64_t byh = (uint64_t)by + bh;
        uint64_t maxOffset64 = 0;
        bool ov = (bd == 0 || bzw == 0) ||
                  __builtin_mul_overflow(bzw - 1, (uint64_t)mip.height, &maxOffset64) ||
                  __builtin_add_overflow(maxOffset64, byh - (bh ? 1 : 0), &maxOffset64) ||
                  __builtin_mul_overflow(maxOffset64, (uint64_t)mip.rowPitch, &maxOffset64) ||
                  __builtin_add_overflow(maxOffset64, bxw * bpp, &maxOffset64);
        if (ov || maxOffset64 > SVGA3_MAX_DMA_BYTES) {
            return SVGA3_VLKN_ERROR_INVALID_PARAM;
        }
        Svga3VlknStatus ensSt = ensureBufferSize((size_t)maxOffset64);
        if (ensSt != SVGA3_VLKN_SUCCESS) return ensSt;
    }

    /* Allocate staging buffer space: bump-allocate the persistent buffer so
     * consecutive uploads batch without a queue idle each. Flush only when
     * the range would be reused (bump overflow) or when a reader runs. */
    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    void *mapped = nullptr;
    VkDeviceSize stagingOffset = 0;
    std::unique_lock<std::mutex> stagingLock;
    bool usingPersistentStaging = false;

    if (totalBytes <= m_backend->stagingSize() && m_backend->stagingBuffer() && m_backend->stagingMapped()) {
        stagingLock = std::unique_lock<std::mutex>(m_backend->stagingMutex());
        stagingOffset = m_backend->stagingAlloc(totalBytes);
        if (stagingOffset == VlknBackend::kStagingAllocFailed) {
            /* Range would be reused: flush to retire pending ranges, retry. */
            Svga3VlknStatus fst = m_backend->flushCommandBuffer();
            if (fst != SVGA3_VLKN_SUCCESS) return fst;
            stagingOffset = m_backend->stagingAlloc(totalBytes);
            if (stagingOffset == VlknBackend::kStagingAllocFailed)
                return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
        }
        stagingBuf = m_backend->stagingBuffer();
        mapped = m_backend->stagingMappedAt(stagingOffset);
        usingPersistentStaging = true;
    } else {
        Svga3VlknStatus st = m_backend->createBuffer(
            totalBytes,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingBuf,
            &stagingMem
        );
        if (st != SVGA3_VLKN_SUCCESS) return st;
        if (m_backend->dispatch().vkMapMemory(m_backend->device(), stagingMem, 0,
                                              totalBytes, 0, &mapped) != VK_SUCCESS ||
            !mapped) {
            m_backend->destroyBuffer(stagingBuf, stagingMem);
            return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
        }
    }

    if (guestStride == 0 || guestStride == copyRowBytes) {
        memcpy(mapped, guestData, totalBytes);
    } else {
        /* Strided row-by-row copy */
        uint8_t *dst = (uint8_t*)mapped;
        const uint8_t *src = (const uint8_t*)guestData;
        for (uint32_t z = 0; z < bd; ++z) {
            for (size_t y = 0; y < copyRows; ++y) {
                memcpy(dst, src, copyRowBytes);
                dst += copyRowBytes;
                src += guestStride;
            }
        }
    }

    /* Also copy into vertex/index backing VkBuffer (range validated above,
     * before any command was recorded). */
    if (doLinearShadowCopy) {
        if (m_buffer && m_bufferMemory) {
            void *bufMapped = m_bufferMapped;
            bool needUnmap = false;
            if (!bufMapped) {
                if (m_backend->dispatch().vkMapMemory(m_backend->device(), m_bufferMemory, 0, m_bufferSize, 0, &bufMapped) == VK_SUCCESS) {
                    needUnmap = true;
                }
            }
            if (bufMapped) {
                uint8_t *dstBuf = static_cast<uint8_t*>(bufMapped);
                const uint8_t *srcBuf = static_cast<const uint8_t*>(mapped);
                for (uint32_t z = 0; z < bd; ++z) {
                    for (size_t y = 0; y < copyRows; ++y) {
                        size_t dstOffset = (static_cast<size_t>(bz + z) * mip.height + (by + y)) * mip.rowPitch + static_cast<size_t>(bx) * bpp;
                        if (dstOffset < m_bufferSize) {
                            size_t copyLen = std::min(copyRowBytes, m_bufferSize - dstOffset);
                            memcpy(dstBuf + dstOffset, srcBuf + (z * bh + y) * copyRowBytes, copyLen);
                        }
                    }
                }
                if (needUnmap) {
                    m_backend->dispatch().vkUnmapMemory(m_backend->device(), m_bufferMemory);
                }

                static uint32_t vbuf_upload_cnt = 0;
                vbuf_upload_cnt++;
                if (vbuf_upload_cnt <= 10 || (vbuf_upload_cnt % 500) == 0) {
                    log_msg("[libqemu_svga3d] 2D Buffer upload #%u: sid=%u, box=(%u,%u %ux%u), bpp=%zu, totalBytes=%zu\n",
                            vbuf_upload_cnt, m_sid, bx, by, bw, bh, bpp, totalBytes);
                }
            }
        }
    }

    /* Guest A4R4G4B4 is ARGB in a 16-bit word; the portable Vulkan
     * B4G4R4A4 format reverses the component nibble order. */
    if (m_svgaFormat == SVGA3D_A4R4G4B4) {
        for (size_t i = 0; i < totalBytes; i += 2) {
            uint16_t value;
            memcpy(&value, static_cast<uint8_t*>(mapped) + i, 2);
            value = uint16_t(((value & 0x000F) << 12) | ((value & 0x00F0) << 4) |
                             ((value & 0x0F00) >> 4) | ((value & 0xF000) >> 12));
            memcpy(static_cast<uint8_t*>(mapped) + i, &value, 2);
        }
    }

    if (!usingPersistentStaging) {
        m_backend->dispatch().vkUnmapMemory(m_backend->device(), stagingMem);
    }

    if (m_svgaFormat == SVGA3D_BUFFER) {
        if (!usingPersistentStaging) {
            m_backend->destroyBuffer(stagingBuf, stagingMem);
        }
        return SVGA3_VLKN_SUCCESS;
    }

    /* Allocation may submit and reset the command buffer on arena wrap.
     * Acquire the recording buffer only after allocation and host writes. */
    /* Transition image to TRANSFER_DST_OPTIMAL */
    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = m_currentLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = m_currentLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = nativeAspectMask();
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = m_arrayLayers;

    m_backend->dispatch().vkCmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );

    /* Record buffer to image copy */
    VkBufferImageCopy region = {};
    region.bufferOffset = usingPersistentStaging ? stagingOffset : 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = m_isDepthStencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = mipLevel;
    region.imageSubresource.baseArrayLayer = face;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = (int32_t)bx;
    region.imageOffset.y = (int32_t)by;
    region.imageOffset.z = (int32_t)bz;
    region.imageExtent.width = bw;
    region.imageExtent.height = bh;
    region.imageExtent.depth = bd;

    m_backend->dispatch().vkCmdCopyBufferToImage(
        cb, stagingBuf, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region
    );

    /* Transition back to SHADER_READ_ONLY_OPTIMAL or COLOR_ATTACHMENT_OPTIMAL */
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    m_backend->dispatch().vkCmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );

    m_currentLayout = barrier.newLayout;
    if (!usingPersistentStaging) {
        /* Temporary buffer: must complete before it can be destroyed. */
        Svga3VlknStatus flushSt = m_backend->flushCommandBuffer();
        m_backend->destroyBuffer(stagingBuf, stagingMem);
        return flushSt;
    }
    /* Persistent staging: leave the copy unflushed. The next reader (present,
     * blit readback, guest readback, SVGA_CMD_FENCE) or a bump-overflow flush
     * submits it in order, so pixel order is unchanged. */
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknSurface::dmaDownloadToStaging(uint32_t mipLevel,
                                                const SVGA3dBox *box,
                                                const void **outMappedData,
                                                size_t *outRowPitch,
                                                std::unique_lock<std::mutex> &outLock, uint32_t face)
{
    if (!outMappedData || !outRowPitch || (mipLevel >= m_mipLevels || face >= m_arrayLayers)) {
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }
    /* Buffer surfaces have no VkImage; the copy below would null-deref. */
    if (m_image == VK_NULL_HANDLE) {
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }

    const SurfaceMipLevel &mip = m_mips[mipLevel];
    uint32_t bw = box ? box->w : mip.width;
    uint32_t bh = box ? box->h : mip.height;
    uint32_t bd = box ? box->d : mip.depth;
    uint32_t bx = box ? box->x : 0;
    uint32_t by = box ? box->y : 0;
    uint32_t bz = box ? box->z : 0;

    const bool compressed = svga3_format_is_compressed(m_svgaFormat);
    const size_t block = compressed ? 4 : 1;
    const size_t bpp = svga3_format_bytes_per_pixel(m_svgaFormat);
    if (!bw || !bh || !bd || !bpp || uint64_t(bx) + bw > mip.width ||
        uint64_t(by) + bh > mip.height || uint64_t(bz) + bd > mip.depth ||
        (compressed && ((bx % block) || (by % block) ||
         ((bw % block) && uint64_t(bx) + bw != mip.width) ||
         ((bh % block) && uint64_t(by) + bh != mip.height))))
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    const size_t copyRowBytes = ((size_t(bw) + block - 1) / block) * bpp;
    const size_t copyRows = (size_t(bh) + block - 1) / block;
    if (copyRowBytes > SIZE_MAX / copyRows / bd)
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    const size_t totalBytes = copyRowBytes * copyRows * bd;


    if (isPackedDepth()) {
        std::unique_lock<std::mutex> lock(m_backend->stagingMutex());
        m_packedDepthReadback.resize(totalBytes);
        auto st = dmaPackedDepth(false, mipLevel, box, m_packedDepthReadback.data(), copyRowBytes, face);
        if (st != SVGA3_VLKN_SUCCESS) return st;
        *outMappedData = m_packedDepthReadback.data();
        *outRowPitch = copyRowBytes;
        outLock = std::move(lock);
        return SVGA3_VLKN_SUCCESS;
    }

    if (totalBytes > m_backend->stagingSize() || !m_backend->stagingBuffer() || !m_backend->stagingMapped()) {
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }

    std::unique_lock<std::mutex> lock(m_backend->stagingMutex());

    /* Bump-allocate the download range. This is a reader: the flush below
     * submits any pending uploads first, in order, then this copy. */
    VkDeviceSize stagingOffset = m_backend->stagingAlloc(totalBytes);
    if (stagingOffset == VlknBackend::kStagingAllocFailed) {
        Svga3VlknStatus fst = m_backend->flushCommandBuffer();
        if (fst != SVGA3_VLKN_SUCCESS) return fst;
        stagingOffset = m_backend->stagingAlloc(totalBytes);
        if (stagingOffset == VlknBackend::kStagingAllocFailed)
            return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }

    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();

    /* Transition image to TRANSFER_SRC_OPTIMAL */
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = m_currentLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = nativeAspectMask();
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = m_arrayLayers;

    // A prior transfer-source layout has only readers since its last
    // transition. Preserve every write-to-read dependency on other layouts.
    if (m_currentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        m_backend->dispatch().vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    VkBufferImageCopy region = {};
    region.bufferOffset = stagingOffset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = m_isDepthStencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = mipLevel;
    region.imageSubresource.baseArrayLayer = face;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = (int32_t)bx;
    region.imageOffset.y = (int32_t)by;
    region.imageOffset.z = (int32_t)bz;
    region.imageExtent.width = bw;
    region.imageExtent.height = bh;
    region.imageExtent.depth = bd;

    m_backend->dispatch().vkCmdCopyImageToBuffer(
        cb, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_backend->stagingBuffer(), 1, &region
    );

    // Color images remain transfer sources until their next consumer requests
    // its layout. This avoids a restore/transition pair on repeated readbacks.
    // Depth restoration retains its existing attachment synchronization.
    if (m_isDepthStencil) {
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        m_backend->dispatch().vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        m_currentLayout = barrier.newLayout;
    } else {
        m_currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    m_backend->recordHostReadBarrier(cb, m_backend->stagingBuffer(), stagingOffset, totalBytes);
    Svga3VlknStatus flushSt = m_backend->flushCommandBuffer();
    if (flushSt != SVGA3_VLKN_SUCCESS) {
        return flushSt;
    }

    if (m_svgaFormat == SVGA3D_A4R4G4B4) {
        auto *packed = static_cast<uint8_t*>(m_backend->stagingMappedAt(stagingOffset));
        for (size_t i = 0; i < totalBytes; i += 2) {
            uint16_t value;
            memcpy(&value, packed + i, 2);
            value = uint16_t(((value & 0x000F) << 12) | ((value & 0x00F0) << 4) |
                             ((value & 0x0F00) >> 4) | ((value & 0xF000) >> 12));
            memcpy(packed + i, &value, 2);
        }
    }
    *outMappedData = m_backend->stagingMappedAt(stagingOffset);
    *outRowPitch = copyRowBytes;
    outLock = std::move(lock);

    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknSurface::dmaDownload(uint32_t mipLevel,
                                        const SVGA3dBox *box,
                                        void *outGuestData,
                                        size_t guestStride,
                                        bool isLinear, uint32_t face)
{
    if (!outGuestData || (mipLevel >= m_mipLevels || face >= m_arrayLayers)) {
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }

    const SurfaceMipLevel &mip = m_mips[mipLevel];
    uint32_t bw = box ? box->w : mip.width;
    uint32_t bh = box ? box->h : mip.height;
    uint32_t bd = box ? box->d : mip.depth;
    uint32_t bx = box ? box->x : 0;
    uint32_t by = box ? box->y : 0;
    uint32_t bz = box ? box->z : 0;

    bool isLinearBuffer = isLinear || (m_svgaFormat == SVGA3D_BUFFER) ||
                          (m_image == VK_NULL_HANDLE) ||
                          (((m_flags & (SVGA3D_SURFACE_HINT_VERTEXBUFFER | SVGA3D_SURFACE_HINT_INDEXBUFFER)) != 0) && m_height <= 1 && m_depth <= 1) ||
                          (m_height <= 1 && m_depth <= 1 && (bw > mip.width || (bx + bw) > mip.width));

    if (isLinearBuffer) {
        if (!m_buffer || !m_bufferMemory) return SVGA3_VLKN_ERROR_INVALID_PARAM;
        uint32_t offset = bx;
        uint32_t len = bw;
        if (offset < m_bufferSize) {
            if (!m_bufferMapped) {
                m_backend->dispatch().vkMapMemory(m_backend->device(), m_bufferMemory, 0, m_bufferSize, 0, &m_bufferMapped);
            }
            if (m_bufferMapped) {
                size_t copyLen = std::min(static_cast<size_t>(len), m_bufferSize - offset);
                memcpy(outGuestData, static_cast<const uint8_t*>(m_bufferMapped) + offset, copyLen);
            }
        }
        return SVGA3_VLKN_SUCCESS;
    }

    const bool compressed = svga3_format_is_compressed(m_svgaFormat);
    const size_t block = compressed ? 4 : 1;
    const size_t bpp = svga3_format_bytes_per_pixel(m_svgaFormat);
    if (!bw || !bh || !bd || !bpp || uint64_t(bx) + bw > mip.width ||
        uint64_t(by) + bh > mip.height || uint64_t(bz) + bd > mip.depth ||
        (compressed && ((bx % block) || (by % block) ||
         ((bw % block) && uint64_t(bx) + bw != mip.width) ||
         ((bh % block) && uint64_t(by) + bh != mip.height))))
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    const size_t copyRowBytes = ((size_t(bw) + block - 1) / block) * bpp;
    const size_t copyRows = (size_t(bh) + block - 1) / block;
    if (copyRowBytes > SIZE_MAX / copyRows / bd)
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    const size_t totalBytes = copyRowBytes * copyRows * bd;
    if (guestStride && guestStride < copyRowBytes)
        return SVGA3_VLKN_ERROR_INVALID_PARAM;

    /* 64-bit comparisons: the 32-bit sums wrapped for large boxes,
     * letting out-of-image boxes pass and driving negative Vulkan extents. */
    bool fitsInImage = (m_image != VK_NULL_HANDLE) &&
                       ((uint64_t)bx + bw <= mip.width) &&
                       ((uint64_t)by + bh <= mip.height) &&
                       ((uint64_t)bz + bd <= mip.depth);

    if (!fitsInImage) {
        log_msg("[libqemu_svga3d] dmaDownload error: box does not fit in image (sid=%u, box=(%u,%u %ux%u), mip=(%ux%u))\n",
                m_sid, bx, by, bw, bh, mip.width, mip.height);
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }

    if (isPackedDepth()) return dmaPackedDepth(false, mipLevel, box, outGuestData, guestStride, face);

    if (totalBytes <= m_backend->stagingSize() && m_backend->stagingBuffer() && m_backend->stagingMapped()) {
        const void *mapped = nullptr;
        size_t rowPitch = 0;
        std::unique_lock<std::mutex> lock;
        Svga3VlknStatus st = dmaDownloadToStaging(mipLevel, box, &mapped, &rowPitch, lock, face);
        if (st != SVGA3_VLKN_SUCCESS) {
            return st;
        }

        if (guestStride == 0 || guestStride == copyRowBytes) {
            memcpy(outGuestData, mapped, totalBytes);
        } else {
            const uint8_t *src = (const uint8_t*)mapped;
            uint8_t *dst = (uint8_t*)outGuestData;
            for (uint32_t z = 0; z < bd; ++z) {
                for (size_t y = 0; y < copyRows; ++y) {
                    memcpy(dst, src, copyRowBytes);
                    src += copyRowBytes;
                    dst += guestStride;
                }
            }
        }
        bool fullMip = bx == 0 && by == 0 && bz == 0 &&
                       bw == mip.width && bh == mip.height && bd == 1;
        if (mipLevel == 0 && !m_isDepthStencil && !compressed && fullMip && face == 0) {
            storeReadback(bw, bh, rowPitch, mapped);
        }
        return SVGA3_VLKN_SUCCESS;
    }

    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    Svga3VlknStatus st = m_backend->createBuffer(
        totalBytes,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuf,
        &stagingMem
    );
    if (st != SVGA3_VLKN_SUCCESS) return st;

    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();

    /* Transition image to TRANSFER_SRC_OPTIMAL */
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = m_currentLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = nativeAspectMask();
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = m_arrayLayers;

    m_backend->dispatch().vkCmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = m_isDepthStencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = mipLevel;
    region.imageSubresource.baseArrayLayer = face;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = (int32_t)bx;
    region.imageOffset.y = (int32_t)by;
    region.imageOffset.z = (int32_t)bz;
    region.imageExtent.width = bw;
    region.imageExtent.height = bh;
    region.imageExtent.depth = bd;

    m_backend->dispatch().vkCmdCopyImageToBuffer(
        cb, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuf, 1, &region
    );

    /* Transition back */
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    m_backend->dispatch().vkCmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );

    m_currentLayout = barrier.newLayout;
    m_backend->recordHostReadBarrier(cb, stagingBuf, 0, totalBytes);
    Svga3VlknStatus flushSt = m_backend->flushCommandBuffer();
    if (flushSt != SVGA3_VLKN_SUCCESS) {
        /* The copy never executed: drop the staging buffer. */
        m_backend->destroyBuffer(stagingBuf, stagingMem);
        return flushSt;
    }

    void *mapped = nullptr;
    if (m_backend->dispatch().vkMapMemory(m_backend->device(), stagingMem, 0,
                                          totalBytes, 0, &mapped) != VK_SUCCESS ||
        !mapped) {
        m_backend->destroyBuffer(stagingBuf, stagingMem);
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }

    if (m_svgaFormat == SVGA3D_A4R4G4B4) {
        for (size_t i = 0; i < totalBytes; i += 2) {
            uint16_t value;
            memcpy(&value, static_cast<uint8_t*>(mapped) + i, 2);
            value = uint16_t(((value & 0x000F) << 12) | ((value & 0x00F0) << 4) |
                             ((value & 0x0F00) >> 4) | ((value & 0xF000) >> 12));
            memcpy(static_cast<uint8_t*>(mapped) + i, &value, 2);
        }
    }
    if (guestStride == 0 || guestStride == copyRowBytes) {
        memcpy(outGuestData, mapped, totalBytes);
    } else {
        const uint8_t *src = (const uint8_t*)mapped;
        uint8_t *dst = (uint8_t*)outGuestData;
        for (uint32_t z = 0; z < bd; ++z) {
            for (size_t y = 0; y < copyRows; ++y) {
                memcpy(dst, src, copyRowBytes);
                src += copyRowBytes;
                dst += guestStride;
            }
        }
    }

    bool fullMip = bx == 0 && by == 0 && bz == 0 &&
                   bw == mip.width && bh == mip.height && bd == 1;
    if (mipLevel == 0 && !m_isDepthStencil && !compressed && fullMip && face == 0) {
        storeReadback(bw, bh, copyRowBytes, mapped);
    }

    m_backend->dispatch().vkUnmapMemory(m_backend->device(), stagingMem);
    m_backend->destroyBuffer(stagingBuf, stagingMem);

    return SVGA3_VLKN_SUCCESS;
}

void VlknSurface::storeReadback(uint32_t w, uint32_t h, size_t pitch, const void *src) {
    if (!src || w == 0 || h == 0 || pitch == 0) {
        m_readbackValid = false;
        return;
    }
    m_readback.resize(pitch * h);
    memcpy(m_readback.data(), src, pitch * h);
    m_readbackW = w;
    m_readbackH = h;
    m_readbackPitch = pitch;
    m_readbackValid = true;
}

VlknSurfaceManager::VlknSurfaceManager(VlknBackend *backend)
    : m_backend(backend)
{}

VlknSurfaceManager::~VlknSurfaceManager() {
    clear();
}

Svga3VlknStatus VlknSurfaceManager::defineSurface(uint32_t sid,
                                                 uint32_t surfaceFlags,
                                                 SVGA3dSurfaceFormat format,
                                                 const SVGA3dSize *sizes,
                                                 uint32_t numSizes)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_surfaces.find(sid);
    const size_t oldBytes = it == m_surfaces.end() ? 0 : it->second->budgetedBytes();
    if (it == m_surfaces.end() && m_surfaces.size() >= SVGA3_MAX_SURFACES)
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;

    auto surf = std::make_unique<VlknSurface>(m_backend, sid, surfaceFlags, format, sizes, numSizes);
    /* Aggregate device budget: reject before touching the host allocator,
     * so a guest cannot exhaust host memory with many per-object-legal
     * surfaces. */
    size_t needBytes = surf->budgetedBytes();
    const size_t extraBytes = needBytes > oldBytes ? needBytes - oldBytes : 0;
    if (!m_backend->resourceBudgets().tryReserveSurfaceBytes(extraBytes)) {
        log_msg("[libqemu_svga3d] defineSurface error: aggregate surface budget exhausted (%zu bytes, sid=%u)\n",
                needBytes, sid);
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }
    Svga3VlknStatus st = surf->allocate();
    if (st != SVGA3_VLKN_SUCCESS) {
        m_backend->resourceBudgets().releaseSurfaceBytes(extraBytes);
        return st;
    }

    if (it != m_surfaces.end() && m_contextMgr) m_contextMgr->invalidateSurface(sid);
    if (oldBytes > needBytes) m_backend->resourceBudgets().releaseSurfaceBytes(oldBytes - needBytes);
    m_surfaces[sid] = std::move(surf);
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknSurfaceManager::destroySurface(uint32_t sid) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_surfaces.find(sid);
    if (it == m_surfaces.end()) {
        return SVGA3_VLKN_ERROR_NOT_FOUND;
    }

    if (m_contextMgr) m_contextMgr->invalidateSurface(sid);
    m_backend->resourceBudgets().releaseSurfaceBytes(it->second->budgetedBytes());
    m_surfaces.erase(it);
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknSurfaceManager::ensureVolumeSurface(uint32_t sid) {
    auto *surface = getSurface(sid);
    if (!surface) return SVGA3_VLKN_ERROR_NOT_FOUND;
    if (surface->isVolumeImage()) return SVGA3_VLKN_SUCCESS;
    if (m_contextMgr) m_contextMgr->invalidateSurface(sid);
    return surface->ensureVolumeImage();
}

VlknSurface* VlknSurfaceManager::getSurface(uint32_t sid) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_surfaces.find(sid);
    if (it != m_surfaces.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool VlknSurfaceManager::exists(uint32_t sid) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_surfaces.find(sid) != m_surfaces.end();
}

void VlknSurfaceManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_backend) m_backend->waitIdle();
    if (m_contextMgr) {
        for (const auto &pair : m_surfaces) {
            m_contextMgr->invalidateSurface(pair.first);
        }
    }
    if (m_backend) {
        for (const auto &pair : m_surfaces) {
            m_backend->resourceBudgets().releaseSurfaceBytes(pair.second->budgetedBytes());
        }
    }
    m_surfaces.clear();
}

size_t VlknSurfaceManager::count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_surfaces.size();
}

Svga3VlknStatus VlknSurfaceManager::copy(uint32_t srcSid,
                                        uint32_t dstSid,
                                        const SVGA3dCopyBox *boxes,
                                        uint32_t numBoxes,
                                        uint32_t srcMip,
                                        uint32_t srcFace,
                                        uint32_t dstMip,
                                        uint32_t dstFace)
{
    VlknSurface *src = getSurface(srcSid);
    VlknSurface *dst = getSurface(dstSid);
    if (!src || !dst) {
        return SVGA3_VLKN_ERROR_NOT_FOUND;
    }

    /* The command's image ids select one mip level / face per side.
     * Mesa's level-restricted sampler views depend on this: a view
     * surface's level 0 is filled from the texture's level N, and
     * copying level 0 instead produces visibly wrong sampling. */
    const SurfaceMipLevel *srcMipInfo = src->getMipInfo(srcMip);
    const SurfaceMipLevel *dstMipInfo = dst->getMipInfo(dstMip);
    if (!srcMipInfo || !dstMipInfo ||
        srcFace >= src->arrayLayers() || dstFace >= dst->arrayLayers()) {
        log_msg("[libqemu_svga3d] copy error: mip/face out of range (src mip=%u face=%u, dst mip=%u face=%u)\n",
                srcMip, srcFace, dstMip, dstFace);
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }

    if (numBoxes && !boxes) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    if (src->image() && dst->image() && src->vkFormat() != dst->vkFormat())
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    for (uint32_t i = 0; i < numBoxes; ++i) {
        const auto &b = boxes[i];
        if (src->buffer() && dst->buffer()) {
            size_t bytes = b.w;
            if (src->image()) {
                if (__builtin_mul_overflow(bytes, size_t(b.h ? b.h : 1), &bytes) ||
                    __builtin_mul_overflow(bytes, size_t(b.d ? b.d : 1), &bytes) ||
                    __builtin_mul_overflow(bytes, svga3_format_bytes_per_pixel(src->svgaFormat()), &bytes))
                    return SVGA3_VLKN_ERROR_INVALID_PARAM;
            }
            if (b.srcx > src->bufferSize() || bytes > src->bufferSize() - b.srcx ||
                b.x > dst->bufferSize() || bytes > dst->bufferSize() - b.x)
                return SVGA3_VLKN_ERROR_INVALID_PARAM;
        }
        if (src->image() && dst->image() &&
            (uint64_t(b.srcx) + b.w > srcMipInfo->width ||
             uint64_t(b.srcy) + (b.h ? b.h : 1) > srcMipInfo->height ||
             uint64_t(b.srcz) + (b.d ? b.d : 1) > srcMipInfo->depth ||
             uint64_t(b.x) + b.w > dstMipInfo->width ||
             uint64_t(b.y) + (b.h ? b.h : 1) > dstMipInfo->height ||
             uint64_t(b.z) + (b.d ? b.d : 1) > dstMipInfo->depth))
            return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }

    if (m_contextMgr) m_contextMgr->endAllRenderPasses();
    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();
    const VkImageLayout srcLayout = src == dst ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    const VkImageLayout dstLayout = src == dst ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    if (src->image()) src->transitionLayout(cb, srcLayout);
    if (dst->image() && dst != src) dst->transitionLayout(cb, dstLayout);

    for (uint32_t i = 0; i < numBoxes; ++i) {
        const SVGA3dCopyBox &b = boxes[i];

        /* Copy backing buffer if present */
        if (src->buffer() && dst->buffer()) {
            /* Overflow-checked: b.w*b.h*b.d*bpp wrapped for huge boxes,
             * shrinking copyBytes and bypassing the bounds checks below. */
            size_t copyBytes = b.w;
            if (src->image() != VK_NULL_HANDLE) {
                size_t bpp = svga3_format_bytes_per_pixel(src->svgaFormat());
                size_t whd = 0;
                if (__builtin_mul_overflow((size_t)b.w, (size_t)(b.h ? b.h : 1), &whd) ||
                    __builtin_mul_overflow(whd, (size_t)(b.d ? b.d : 1), &whd) ||
                    __builtin_mul_overflow(whd, bpp, &copyBytes)) {
                    continue;
                }
            }
            /* 64-bit: b.srcx + copyBytes wrapped in 32-bit for huge boxes. */
            uint64_t srcEnd = (uint64_t)b.srcx + copyBytes;
            uint64_t dstEnd = (uint64_t)b.x + copyBytes;
            if (srcEnd <= src->bufferSize() && dstEnd <= dst->bufferSize()) {
                VkBufferCopy bufCopy = {};
                bufCopy.srcOffset = b.srcx;
                bufCopy.dstOffset = b.x;
                bufCopy.size = copyBytes;
                m_backend->dispatch().vkCmdCopyBuffer(cb, src->buffer(), dst->buffer(), 1, &bufCopy);
            }
        }

        /* Copy VkImage if both surfaces have images.
         * 64-bit box validation (C5): guest box fields went verbatim into
         * VkImageCopy; out-of-extents copies are Vulkan-level OOB. */
        if (src->image() != VK_NULL_HANDLE && dst->image() != VK_NULL_HANDLE) {
            uint32_t bd = b.d ? b.d : 1;
            if ((uint64_t)b.srcx + b.w > srcMipInfo->width ||
                (uint64_t)b.srcy + (b.h ? b.h : 1) > srcMipInfo->height ||
                (uint64_t)b.srcz + bd > srcMipInfo->depth ||
                (uint64_t)b.x + b.w > dstMipInfo->width ||
                (uint64_t)b.y + (b.h ? b.h : 1) > dstMipInfo->height ||
                (uint64_t)b.z + bd > dstMipInfo->depth) {
                log_msg("[libqemu_svga3d] copy error: box %u out of image bounds\n", i);
                return SVGA3_VLKN_ERROR_INVALID_PARAM;
            }
            VkImageCopy copyRegion = {};
            copyRegion.srcSubresource.aspectMask = src->isDepthStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.mipLevel = srcMip;
            copyRegion.srcSubresource.baseArrayLayer = srcFace;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset.x = (int32_t)b.srcx;
            copyRegion.srcOffset.y = (int32_t)b.srcy;
            copyRegion.srcOffset.z = (int32_t)b.srcz;

            copyRegion.dstSubresource.aspectMask = dst->isDepthStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.mipLevel = dstMip;
            copyRegion.dstSubresource.baseArrayLayer = dstFace;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset.x = (int32_t)b.x;
            copyRegion.dstOffset.y = (int32_t)b.y;
            copyRegion.dstOffset.z = (int32_t)b.z;

            copyRegion.extent.width = b.w;
            copyRegion.extent.height = b.h;
            copyRegion.extent.depth = b.d ? b.d : 1;

            m_backend->dispatch().vkCmdCopyImage(
                cb,
                src->image(), srcLayout,
                dst->image(), dstLayout,
                1, &copyRegion
            );
            if (svga3_format_has_stencil(src->svgaFormat()) && svga3_format_has_stencil(dst->svgaFormat())) {
                copyRegion.srcSubresource.aspectMask = copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
                m_backend->dispatch().vkCmdCopyImage(cb, src->image(), srcLayout, dst->image(), dstLayout, 1, &copyRegion);
            }
        }
    }

    /* The destination's contents changed: invalidate its readback cache. */
    dst->invalidateReadback();
    return m_backend->flushCommandBuffer();
}

Svga3VlknStatus VlknSurfaceManager::defineSurfaceV2(uint32_t sid,
                                                  uint32_t surfaceFlags,
                                                  SVGA3dSurfaceFormat format,
                                                  uint32_t multisampleCount,
                                                  SVGA3dTextureFilter autogenFilter,
                                                  const SVGA3dSize *sizes,
                                                  uint32_t numSizes)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_surfaces.find(sid);
    const size_t oldBytes = it == m_surfaces.end() ? 0 : it->second->budgetedBytes();
    if (it == m_surfaces.end() && m_surfaces.size() >= SVGA3_MAX_SURFACES)
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;

    auto surf = std::make_unique<VlknSurface>(m_backend, sid, surfaceFlags, format, sizes, numSizes, multisampleCount, autogenFilter);
    size_t needBytes = surf->budgetedBytes();
    const size_t extraBytes = needBytes > oldBytes ? needBytes - oldBytes : 0;
    if (!m_backend->resourceBudgets().tryReserveSurfaceBytes(extraBytes)) {
        log_msg("[libqemu_svga3d] defineSurfaceV2 error: aggregate surface budget exhausted (%zu bytes, sid=%u)\n",
                needBytes, sid);
        return SVGA3_VLKN_ERROR_OUT_OF_MEMORY;
    }
    Svga3VlknStatus st = surf->allocate();
    if (st != SVGA3_VLKN_SUCCESS) {
        m_backend->resourceBudgets().releaseSurfaceBytes(extraBytes);
        return st;
    }

    if (it != m_surfaces.end() && m_contextMgr) m_contextMgr->invalidateSurface(sid);
    if (oldBytes > needBytes) m_backend->resourceBudgets().releaseSurfaceBytes(oldBytes - needBytes);
    m_surfaces[sid] = std::move(surf);
    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknSurfaceManager::stretchBlt(uint32_t srcSid,
                                              uint32_t dstSid,
                                              const SVGA3dBox &boxSrc,
                                              const SVGA3dBox &boxDest,
                                              SVGA3dStretchBltMode mode)
{
    VlknSurface *src = getSurface(srcSid);
    VlknSurface *dst = getSurface(dstSid);
    if (!src || !dst) {
        return SVGA3_VLKN_ERROR_NOT_FOUND;
    }
    /* Buffer surfaces have no VkImage; blitting would null-deref. */
    if (src->image() == VK_NULL_HANDLE || dst->image() == VK_NULL_HANDLE) {
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }
    /* 64-bit box validation: (x+w) wrapped in 32-bit, producing negative
     * Vulkan extents and out-of-image blits. */
    if ((uint64_t)boxSrc.x + boxSrc.w > src->width() ||
        (uint64_t)boxSrc.y + boxSrc.h > src->height() ||
        (uint64_t)boxSrc.z + (boxSrc.d ? boxSrc.d : 1) > src->depth() ||
        (uint64_t)boxDest.x + boxDest.w > dst->width() ||
        (uint64_t)boxDest.y + boxDest.h > dst->height() ||
        (uint64_t)boxDest.z + (boxDest.d ? boxDest.d : 1) > dst->depth()) {
        log_msg("[libqemu_svga3d] stretchBlt error: box out of image bounds\n");
        return SVGA3_VLKN_ERROR_INVALID_PARAM;
    }

    if (m_contextMgr) m_contextMgr->endAllRenderPasses();
    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();
    const VkImageLayout srcLayout = src == dst ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    const VkImageLayout dstLayout = src == dst ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    if (src->image()) src->transitionLayout(cb, srcLayout);
    if (dst->image() && dst != src) dst->transitionLayout(cb, dstLayout);

    VkImageBlit blit = {};
    blit.srcSubresource.aspectMask = src->isDepthStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[0] = { (int32_t)boxSrc.x, (int32_t)boxSrc.y, (int32_t)boxSrc.z };
    blit.srcOffsets[1] = { (int32_t)(boxSrc.x + boxSrc.w), (int32_t)(boxSrc.y + boxSrc.h), (int32_t)(boxSrc.z + (boxSrc.d ? boxSrc.d : 1)) };

    blit.dstSubresource.aspectMask = dst->isDepthStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = 0;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[0] = { (int32_t)boxDest.x, (int32_t)boxDest.y, (int32_t)boxDest.z };
    blit.dstOffsets[1] = { (int32_t)(boxDest.x + boxDest.w), (int32_t)(boxDest.y + boxDest.h), (int32_t)(boxDest.z + (boxDest.d ? boxDest.d : 1)) };

    VkFilter filter = (!src->isDepthStencil() && mode == SVGA3D_STRETCH_BLT_LINEAR) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

    m_backend->dispatch().vkCmdBlitImage(
        cb,
        src->image(), srcLayout,
        dst->image(), dstLayout,
        1, &blit, filter
    );
    if (svga3_format_has_stencil(src->svgaFormat()) && svga3_format_has_stencil(dst->svgaFormat())) {
        blit.srcSubresource.aspectMask = blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        m_backend->dispatch().vkCmdBlitImage(cb, src->image(), srcLayout, dst->image(), dstLayout, 1, &blit, VK_FILTER_NEAREST);
    }

    dst->invalidateReadback();
    return m_backend->flushCommandBuffer();
}

Svga3VlknStatus VlknSurfaceManager::surfaceDMA(const SVGA3dGuestImage &guest,
                                              const SVGA3dSurfaceImageId &host,
                                              SVGA3dTransferType transfer,
                                              const SVGA3dCopyBox *boxes,
                                              uint32_t numBoxes,
                                              GuestMemoryManager *guestMem,
                                              const void *guestBuffer,
                                              size_t guestBufferSize)
{
    VlknSurface *surf = getSurface(host.sid);
    if (!surf) {
        return SVGA3_VLKN_ERROR_NOT_FOUND;
    }

    if (numBoxes == 0 || !boxes) {
        return SVGA3_VLKN_SUCCESS;
    }

    // FIFO/GMR DMA bypasses the public API wrapper. End any active
    // render pass before recording image transfer barriers or copies.
    bool isLinear = (surf->svgaFormat() == SVGA3D_BUFFER) ||
                    (surf->image() == VK_NULL_HANDLE) ||
                    ((surf->flags() & (SVGA3D_SURFACE_HINT_VERTEXBUFFER | SVGA3D_SURFACE_HINT_INDEXBUFFER)) != 0 && surf->height() <= 1 && surf->depth() <= 1);
    if (!isLinear && m_contextMgr) {
        m_contextMgr->endAllRenderPasses();
    }

    size_t baseBpp = svga3_format_bytes_per_pixel(surf->svgaFormat());
    for (uint32_t i = 0; i < numBoxes; ++i) {
        const SVGA3dCopyBox &box = boxes[i];
        uint32_t bw = box.w;
        uint32_t bh = box.h;
        uint32_t bd = box.d ? box.d : 1;
        if (bw == 0 || bh == 0) continue;

        bool isLinearBuffer = (surf->svgaFormat() == SVGA3D_BUFFER) ||
                              (surf->image() == VK_NULL_HANDLE) ||
                              (((surf->flags() & (SVGA3D_SURFACE_HINT_VERTEXBUFFER | SVGA3D_SURFACE_HINT_INDEXBUFFER)) != 0) && surf->height() <= 1 && surf->depth() <= 1) ||
                              (surf->height() <= 1 && surf->depth() <= 1 && (bw > surf->width() || (box.x + bw) > surf->width()));
        if (isLinearBuffer && surf->height() <= 1 && surf->depth() <= 1) {
            surf->addFlags(SVGA3D_SURFACE_HINT_VERTEXBUFFER);
        }

        const bool compressed = !isLinearBuffer && svga3_format_is_compressed(surf->svgaFormat());
        const uint32_t blockSize = compressed ? 4 : 1;
        if (compressed && (box.x % 4 || box.y % 4 || box.srcx % 4 || box.srcy % 4))
            return SVGA3_VLKN_ERROR_INVALID_PARAM;
        const size_t bpp = isLinearBuffer ? 1 : baseBpp;
        const size_t columns = (uint64_t(bw) + blockSize - 1) / blockSize;
        const uint32_t rows = static_cast<uint32_t>((uint64_t(bh) + blockSize - 1) / blockSize);
        size_t rowBytes = 0;
        if (!bpp || __builtin_mul_overflow(columns, bpp, &rowBytes))
            return SVGA3_VLKN_ERROR_INVALID_PARAM;
        const size_t guestStride = guest.pitch ? guest.pitch : rowBytes;
        if (guestStride < rowBytes) return SVGA3_VLKN_ERROR_INVALID_PARAM;

        // A guest pitch addresses block rows for BC images. Validate the last
        // byte touched, including source coordinates and partial final rows.
        uint64_t guestSlicePitch = 0, guestOffset = 0, yOffset = 0, xOffset = 0;
        uint64_t xferBytes = 0, tailRows = 0, endOffset = 0;
        if (__builtin_mul_overflow(uint64_t(guestStride), uint64_t(rows), &guestSlicePitch) ||
            __builtin_mul_overflow(uint64_t(box.srcz), guestSlicePitch, &guestOffset) ||
            __builtin_mul_overflow(uint64_t(box.srcy / blockSize), uint64_t(guestStride), &yOffset) ||
            __builtin_mul_overflow(uint64_t(box.srcx / blockSize), uint64_t(bpp), &xOffset) ||
            __builtin_add_overflow(guestOffset, uint64_t(guest.ptr.offset), &guestOffset) ||
            __builtin_add_overflow(guestOffset, yOffset, &guestOffset) ||
            __builtin_add_overflow(guestOffset, xOffset, &guestOffset) ||
            __builtin_mul_overflow(uint64_t(bd - 1), guestSlicePitch, &xferBytes) ||
            __builtin_mul_overflow(uint64_t(rows - 1), uint64_t(guestStride), &tailRows) ||
            __builtin_add_overflow(xferBytes, tailRows, &xferBytes) ||
            __builtin_add_overflow(xferBytes, uint64_t(rowBytes), &xferBytes) ||
            __builtin_add_overflow(guestOffset, xferBytes, &endOffset))
            return SVGA3_VLKN_ERROR_INVALID_PARAM;
        if (transfer != SVGA3D_WRITE_HOST_VRAM && transfer != SVGA3D_READ_HOST_VRAM)
            return SVGA3_VLKN_ERROR_INVALID_PARAM;

        SVGA3dBox sBox;
        sBox.x = box.x;
        sBox.y = box.y;
        sBox.z = box.z;
        sBox.w = bw;
        sBox.h = bh;
        sBox.d = bd;

        if (guestMem) {
            /* Overflow-checked: bw/bh/bd are guest-controlled, and a wrapped
             * product would undersize `staging`, overflowing the heap in the
             * row loops below. */
            size_t totalBytes = 0;
            if (__builtin_mul_overflow(rowBytes, static_cast<size_t>(rows), &totalBytes) ||
                __builtin_mul_overflow(totalBytes, static_cast<size_t>(bd), &totalBytes)) {
                return SVGA3_VLKN_ERROR_INVALID_PARAM;
            }
            /* Cap the staging allocation: one guest DMA must not allocate
             * gigabytes of host memory (DoS). */
            if (totalBytes > SVGA3_MAX_DMA_BYTES) {
                log_msg("[libqemu_svga3d] surfaceDMA error: transfer %zu bytes exceeds max %zu (sid=%u)\n",
                        totalBytes, SVGA3_MAX_DMA_BYTES, surf->sid());
                return SVGA3_VLKN_ERROR_INVALID_PARAM;
            }
            std::vector<uint8_t> staging(totalBytes);

            if (transfer == SVGA3D_WRITE_HOST_VRAM) {
                for (uint32_t z = 0; z < bd; ++z) {
                    for (uint32_t y = 0; y < rows; ++y) {
                        uint64_t rowGuestOffset = guestOffset + uint64_t(z) * guestSlicePitch + uint64_t(y) * guestStride;
                        if (rowGuestOffset > UINT32_MAX) return SVGA3_VLKN_ERROR_INVALID_PARAM;
                        SVGAGuestPtr rowPtr = { guest.ptr.gmrId, static_cast<uint32_t>(rowGuestOffset) };
                        uint8_t *dstRow = staging.data() + (size_t(z) * rows + y) * rowBytes;
                        Svga3VlknStatus st = guestMem->readGuest(rowPtr, dstRow, rowBytes);
                        if (st != SVGA3_VLKN_SUCCESS) {
                            static uint32_t dma_read_err_cnt = 0;
                            if (++dma_read_err_cnt <= 10) {
                                log_msg("[libqemu_svga3d] surfaceDMA error: readGuest failed (st=%d, gmrId=%u, offset=%lu, rowBytes=%zu)\n",
                                        st, rowPtr.gmrId, (unsigned long)rowGuestOffset, rowBytes);
                            }
                            return st;
                        }
                    }
                }
                Svga3VlknStatus st = surf->dmaUpload(host.mipmap, &sBox, staging.data(), rowBytes, isLinearBuffer, host.face);
                if (st != SVGA3_VLKN_SUCCESS) {
                    static uint32_t dma_upload_err_cnt = 0;
                    if (++dma_upload_err_cnt <= 10) {
                        log_msg("[libqemu_svga3d] surfaceDMA error: dmaUpload failed (st=%d, sid=%u, mip=%u)\n",
                                st, surf->sid(), host.mipmap);
                    }
                    return st;
                }
            } else if (transfer == SVGA3D_READ_HOST_VRAM) {
                Svga3VlknStatus st = surf->dmaDownload(host.mipmap, &sBox, staging.data(), rowBytes, isLinearBuffer, host.face);
                if (st != SVGA3_VLKN_SUCCESS) {
                    static uint32_t dma_dl_err_cnt = 0;
                    if (++dma_dl_err_cnt <= 10) {
                        log_msg("[libqemu_svga3d] surfaceDMA error: dmaDownload failed (st=%d, sid=%u, mip=%u)\n",
                                st, surf->sid(), host.mipmap);
                    }
                    return st;
                }

                for (uint32_t z = 0; z < bd; ++z) {
                    for (uint32_t y = 0; y < rows; ++y) {
                        uint64_t rowGuestOffset = guestOffset + uint64_t(z) * guestSlicePitch + uint64_t(y) * guestStride;
                        if (rowGuestOffset > UINT32_MAX) return SVGA3_VLKN_ERROR_INVALID_PARAM;
                        SVGAGuestPtr rowPtr = { guest.ptr.gmrId, static_cast<uint32_t>(rowGuestOffset) };
                        const uint8_t *srcRow = staging.data() + (size_t(z) * rows + y) * rowBytes;
                        st = guestMem->writeGuest(rowPtr, srcRow, rowBytes);
                        if (st != SVGA3_VLKN_SUCCESS) {
                            static uint32_t dma_write_err_cnt = 0;
                            if (++dma_write_err_cnt <= 10) {
                                log_msg("[libqemu_svga3d] surfaceDMA error: writeGuest failed (st=%d, gmrId=%u, offset=%lu, rowBytes=%zu)\n",
                                        st, rowPtr.gmrId, (unsigned long)rowGuestOffset, rowBytes);
                            }
                            return st;
                        }
                    }
                }
            }
        } else if (guestBuffer) {
            if (endOffset > guestBufferSize) return SVGA3_VLKN_ERROR_INVALID_PARAM;
            auto *data = reinterpret_cast<const uint8_t*>(guestBuffer) + guestOffset;
            Svga3VlknStatus st = transfer == SVGA3D_WRITE_HOST_VRAM
                ? surf->dmaUpload(host.mipmap, &sBox, data, guestStride, isLinearBuffer, host.face)
                : surf->dmaDownload(host.mipmap, &sBox, const_cast<uint8_t*>(data), guestStride, isLinearBuffer, host.face);
            if (st != SVGA3_VLKN_SUCCESS) return st;
        } else {
            return SVGA3_VLKN_ERROR_INVALID_PARAM;
        }
    }

    return SVGA3_VLKN_SUCCESS;
}

Svga3VlknStatus VlknSurfaceManager::generateMipmaps(uint32_t sid, SVGA3dTextureFilter filter) {
    VlknSurface *surf = getSurface(sid);
    if (!surf) return SVGA3_VLKN_ERROR_NOT_FOUND;
    if (surf->mipLevels() <= 1) return SVGA3_VLKN_SUCCESS;
    /* Buffer surfaces have no VkImage; blitting would null-deref. */
    if (surf->image() == VK_NULL_HANDLE) return SVGA3_VLKN_ERROR_INVALID_PARAM;

    if (m_contextMgr) m_contextMgr->endAllRenderPasses();
    VkCommandBuffer cb = m_backend->getActiveCommandBuffer();
    surf->transitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
    VkFilter vkFilt = (filter == SVGA3D_TEX_FILTER_NEAREST) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

    for (uint32_t i = 1; i < surf->mipLevels(); ++i) {
        if (i > 1) surf->transitionLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
        const SurfaceMipLevel *prevMip = surf->getMipInfo(i - 1);
        const SurfaceMipLevel *curMip = surf->getMipInfo(i);
        if (!prevMip || !curMip) break;

        VkImageBlit blit = {};
        blit.srcSubresource.aspectMask = surf->isDepthStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = surf->arrayLayers();
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { (int32_t)prevMip->width, (int32_t)prevMip->height, (int32_t)prevMip->depth };

        blit.dstSubresource.aspectMask = surf->isDepthStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = surf->arrayLayers();
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { (int32_t)curMip->width, (int32_t)curMip->height, (int32_t)curMip->depth };

        m_backend->dispatch().vkCmdBlitImage(
            cb,
            surf->image(), VK_IMAGE_LAYOUT_GENERAL,
            surf->image(), VK_IMAGE_LAYOUT_GENERAL,
            1, &blit, vkFilt
        );
    }

    surf->transitionLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    surf->ensureViewMipLevels(surf->mipLevels());
    surf->invalidateReadback();
    return m_backend->flushCommandBuffer();
}

namespace {
// Merge only when the union is itself a rectangle and source mapping agrees.
// For PRESENT this is applied only to consecutive writes, preserving ordering.
bool mergePresentationRect(SVGA3dCopyRect &a, const SVGA3dCopyRect &b) {
    if (int64_t(a.srcx) - a.x != int64_t(b.srcx) - b.x ||
        int64_t(a.srcy) - a.y != int64_t(b.srcy) - b.y) return false;
    const uint64_t ar = uint64_t(a.x) + a.w, ab = uint64_t(a.y) + a.h;
    const uint64_t br = uint64_t(b.x) + b.w, bb = uint64_t(b.y) + b.h;
    const bool contains = (a.x <= b.x && a.y <= b.y && ar >= br && ab >= bb) ||
                          (b.x <= a.x && b.y <= a.y && br >= ar && bb >= ab);
    const bool horizontal = a.y == b.y && a.h == b.h && a.x <= br && b.x <= ar;
    const bool vertical = a.x == b.x && a.w == b.w && a.y <= bb && b.y <= ab;
    if (!contains && !horizontal && !vertical) return false;
    const uint32_t x = std::min(a.x, b.x), y = std::min(a.y, b.y);
    a.srcx = uint32_t(int64_t(a.srcx) + int64_t(x) - a.x);
    a.srcy = uint32_t(int64_t(a.srcy) + int64_t(y) - a.y);
    a.x = x; a.y = y;
    a.w = uint32_t(std::max(ar, br) - x);
    a.h = uint32_t(std::max(ab, bb) - y);
    return true;
}

// Collapse tightly packed rows into one memcpy, retaining a pitched fallback.
bool copyPresentationRect(uint8_t *dst, size_t dstSize, size_t dstPitch, size_t dstBpp,
                          const uint8_t *src, size_t srcPitch, size_t srcBpp,
                          const SVGA3dCopyRect &r, PerformanceCounters *counters) {
    const size_t pixelBytes = std::min(srcBpp, dstBpp);
    const uint64_t first = uint64_t(r.y) * dstPitch + uint64_t(r.x) * dstBpp;
    const uint64_t last = uint64_t(r.y + r.h - 1) * dstPitch + uint64_t(r.x + r.w - 1) * dstBpp;
    if (last > dstSize || pixelBytes > dstSize - last) return false;
    dst += size_t(first);
    src += size_t(r.srcy) * srcPitch + size_t(r.srcx) * srcBpp;
    const size_t rowBytes = size_t(r.w) * pixelBytes;
    if (srcBpp == dstBpp && srcPitch == rowBytes && dstPitch == rowBytes) {
        memcpy(dst, src, rowBytes * r.h);
        if (counters) { counters->framebufferCopyBytes += rowBytes * r.h; ++counters->framebufferCopyCalls; }
    } else {
        for (uint32_t y = 0; y < r.h; ++y) {
            if (srcBpp == dstBpp) {
                memcpy(dst, src, rowBytes);
                if (counters) ++counters->framebufferCopyCalls;
            } else {
                for (uint32_t x = 0; x < r.w; ++x)
                    memcpy(dst + size_t(x) * dstBpp, src + size_t(x) * srcBpp, pixelBytes);
                if (counters) counters->framebufferCopyCalls += r.w;
            }
            if (counters) counters->framebufferCopyBytes += rowBytes;
            if (y + 1 < r.h) { dst += dstPitch; src += srcPitch; }
        }
    }
    return true;
}
}

Svga3VlknStatus VlknSurfaceManager::blitSurfaceToScreen(const SVGA3dSurfaceImageId &srcImage,
                                                      const SVGASignedRect &srcRect,
                                                      uint32_t destScreenId,
                                                      const SVGASignedRect &destRect,
                                                      const SVGASignedRect *clipRects,
                                                      uint32_t numClipRects,
                                                      GuestMemoryManager *guestMem)
{
    (void)destScreenId;
    auto *counters = m_backend->performanceCounters();
    if (counters) ++counters->presentationCalls;
    VlknSurface *surf = getSurface(srcImage.sid);
    if (!surf) return SVGA3_VLKN_ERROR_NOT_FOUND;
    const auto *mip = surf->getMipInfo(srcImage.mipmap);
    if (!mip) return SVGA3_VLKN_ERROR_INVALID_PARAM;
    if (svga3_format_is_compressed(surf->svgaFormat()) || surf->isDepthStencil())
        return SVGA3_VLKN_ERROR_UNSUPPORTED_FORMAT;

    if (guestMem && guestMem->getFramebuffer().hva) {
        const auto &fb = guestMem->getFramebuffer();
        const size_t bpp = svga3_format_bytes_per_pixel(surf->svgaFormat());
        const size_t dstBpp = fb.bpp;
        if (!bpp || !dstBpp) return SVGA3_VLKN_ERROR_UNSUPPORTED_FORMAT;
        const size_t dstPitch = fb.pitch ? fb.pitch : static_cast<size_t>(fb.width) * dstBpp;

        /* Clip in screen coordinates before readback. Use wide signed values
         * so negative origins and extreme guest rectangle endpoints are safe. */
        const int64_t sx = srcRect.left, sy = srcRect.top;
        const int64_t dx = destRect.left, dy = destRect.top;
        const int64_t sw = int64_t(srcRect.right) - sx;
        const int64_t sh = int64_t(srcRect.bottom) - sy;
        const int64_t dw = int64_t(destRect.right) - dx;
        const int64_t dh = int64_t(destRect.bottom) - dy;
        const int64_t x0 = std::max({dx, int64_t(0), dx - sx});
        const int64_t y0 = std::max({dy, int64_t(0), dy - sy});
        const int64_t x1 = std::min({dx + std::min(sw, dw), int64_t(fb.width), dx + mip->width - sx});
        const int64_t y1 = std::min({dy + std::min(sh, dh), int64_t(fb.height), dy + mip->height - sy});
        if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0 || x1 <= x0 || y1 <= y0)
            return SVGA3_VLKN_SUCCESS;

        /* SVGA clip rectangles are relative to the destination origin. Walk
         * their intersections twice: once to bound the GPU transfer and once
         * to copy visible rows. This avoids a per-frame allocation. */
        auto rawRects = [&](auto &&visit) {
            if (!numClipRects || !clipRects) {
                visit(x0, y0, x1, y1);
            } else {
                for (uint32_t c = 0; c < numClipRects; ++c) {
                    const int64_t cx0 = std::max(x0, dx + clipRects[c].left);
                    const int64_t cy0 = std::max(y0, dy + clipRects[c].top);
                    const int64_t cx1 = std::min(x1, dx + clipRects[c].right);
                    const int64_t cy1 = std::min(y1, dy + clipRects[c].bottom);
                    if (cx1 > cx0 && cy1 > cy0) visit(cx0, cy0, cx1, cy1);
                }
            }
        };
        // All clips in a BLIT share one source mapping. Bound the merge work
        // and scratch storage, falling back to streaming for larger guest lists.
        const bool mergeClips = numClipRects <= 64;
        if (mergeClips) {
            m_presentationRects.clear();
            rawRects([&](int64_t l, int64_t t, int64_t r, int64_t b) {
                SVGA3dCopyRect rect{uint32_t(l), uint32_t(t), uint32_t(r-l), uint32_t(b-t),
                    uint32_t(sx+l-dx), uint32_t(sy+t-dy)};
                for (auto &existing : m_presentationRects)
                    if (mergePresentationRect(existing, rect)) return;
                m_presentationRects.push_back(rect);
            });
        }
        auto forEachRect = [&](auto &&visit) {
            if (!mergeClips) { rawRects(visit); return; }
            for (const auto &r : m_presentationRects)
                visit(int64_t(r.x), int64_t(r.y), int64_t(r.x)+r.w, int64_t(r.y)+r.h);
        };
        int64_t left = x1, top = y1, right = x0, bottom = y0;
        forEachRect([&](int64_t l, int64_t t, int64_t r, int64_t b) {
            left = std::min(left, l); top = std::min(top, t);
            right = std::max(right, r); bottom = std::max(bottom, b);
        });
        if (right <= left || bottom <= top) return SVGA3_VLKN_SUCCESS;

        SVGA3dBox readbackBox = {static_cast<uint32_t>(sx + left - dx),
            static_cast<uint32_t>(sy + top - dy), 0,
            static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top), 1};
        const void *mappedData = nullptr;
        size_t rowPitch = 0;
        std::unique_lock<std::mutex> lock;
        Svga3VlknStatus st = surf->dmaDownloadToStaging(srcImage.mipmap, &readbackBox,
                                                      &mappedData, &rowPitch, lock);
        if (st != SVGA3_VLKN_SUCCESS) return st;

        bool boundsValid = true;
        uint8_t *scanoutBase = fb.scanoutHva();
        size_t scanoutSz = fb.scanoutSize();
        if (counters) counters->readbackBytes += size_t(readbackBox.w) * readbackBox.h * bpp;
        forEachRect([&](int64_t l, int64_t t, int64_t r, int64_t b) {
            const SVGA3dCopyRect rect{uint32_t(l), uint32_t(t), uint32_t(r-l), uint32_t(b-t),
                                     uint32_t(l-left), uint32_t(t-top)};
            if (!copyPresentationRect(scanoutBase, scanoutSz, dstPitch, dstBpp,
                                      static_cast<const uint8_t *>(mappedData), rowPitch, bpp, rect, counters)) {
                boundsValid = false; return;
            }
            guestMem->notifyDisplayUpdate(int32_t(l), int32_t(t), int32_t(r-l), int32_t(b-t));
        });
        if (!boundsValid) return SVGA3_VLKN_ERROR_INVALID_PARAM;
        return SVGA3_VLKN_SUCCESS;
    }
    return m_backend->flushCommandBuffer();
}

bool is_buffer_all_zero(const void *data, uint32_t w, uint32_t h, size_t rowPitch, size_t bpp) {
    if (!data) return true;
    const uint8_t *row = static_cast<const uint8_t*>(data);
    size_t rowBytes = w * bpp;
    for (uint32_t y = 0; y < h; ++y) {
        const uint64_t *p64 = reinterpret_cast<const uint64_t*>(row + y * rowPitch);
        size_t n64 = rowBytes / 8;
        for (size_t i = 0; i < n64; ++i) {
            if (p64[i] != 0) return false;
        }
        size_t rem = rowBytes % 8;
        const uint8_t *p8 = reinterpret_cast<const uint8_t*>(p64 + n64);
        for (size_t i = 0; i < rem; ++i) {
            if (p8[i] != 0) return false;
        }
    }
    return true;
}

bool is_buffer_all_black_or_zero(const void *data, uint32_t w, uint32_t h, size_t rowPitch, size_t bpp) {
    if (!data) return true;
    const uint8_t *row = static_cast<const uint8_t*>(data);
    if (bpp == 4) {
        for (uint32_t y = 0; y < h; ++y) {
            const uint32_t *p32 = reinterpret_cast<const uint32_t*>(row + y * rowPitch);
            for (uint32_t x = 0; x < w; ++x) {
                if ((p32[x] & 0x00FFFFFF) != 0) return false;
            }
        }
        return true;
    }
    return is_buffer_all_zero(data, w, h, rowPitch, bpp);
}

Svga3VlknStatus VlknSurfaceManager::present(uint32_t sid,
                                            const SVGA3dCopyRect *rects,
                                            uint32_t numRects,
                                            GuestMemoryManager *guestMem)
{
    VlknSurface *surf = getSurface(sid);
    if (!surf) return SVGA3_VLKN_ERROR_NOT_FOUND;
    auto *counters = m_backend->performanceCounters();
    if (counters) ++counters->presentationCalls;
    if (!guestMem || !guestMem->getFramebuffer().hva)
        return m_backend->flushCommandBuffer();
    const auto &fb = guestMem->getFramebuffer();
    const uint32_t surfW = surf->width(), surfH = surf->height();
    const uint32_t dstW = fb.width ? fb.width : surfW, dstH = fb.height ? fb.height : surfH;
    const size_t bpp = svga3_format_bytes_per_pixel(surf->svgaFormat());
    const size_t dstBpp = fb.bpp ? fb.bpp : 4;
    if (!bpp || surf->isDepthStencil() || svga3_format_is_compressed(surf->svgaFormat()))
        return SVGA3_VLKN_ERROR_UNSUPPORTED_FORMAT;
    const size_t dstPitch = fb.pitch ? fb.pitch : size_t(dstW) * dstBpp;

    // Stream clipped rectangles without allocations. Only neighboring writes
    // with identical mappings may merge; different source mappings retain order.
    auto forEachRect = [&](auto &&visit) {
        SVGA3dCopyRect pending{};
        bool havePending = false;
        auto add = [&](SVGA3dCopyRect r) {
            if (!r.w || !r.h || r.srcx >= surfW || r.srcy >= surfH || r.x >= dstW || r.y >= dstH) return;
            r.w = std::min({r.w, surfW - r.srcx, dstW - r.x});
            r.h = std::min({r.h, surfH - r.srcy, dstH - r.y});
            if (havePending && mergePresentationRect(pending, r)) return;
            if (havePending) visit(pending);
            pending = r; havePending = true;
        };
        if (!numRects || !rects) add({0, 0, surfW, surfH, 0, 0});
        else for (uint32_t i = 0; i < numRects; ++i) add(rects[i]);
        if (havePending) visit(pending);
    };
    uint32_t left = surfW, top = surfH, right = 0, bottom = 0;
    forEachRect([&](const SVGA3dCopyRect &r) {
        left = std::min(left, r.srcx); top = std::min(top, r.srcy);
        right = std::max(right, r.srcx + r.w); bottom = std::max(bottom, r.srcy + r.h);
    });
    // No pixels are observable; leave earlier commands queued for their fence.
    if (right <= left || bottom <= top) return SVGA3_VLKN_SUCCESS;
    const SVGA3dBox box{left, top, 0, right - left, bottom - top, 1};
    const void *mappedData = nullptr;
    size_t rowPitch = 0;
    std::unique_lock<std::mutex> lock;
    const auto st = surf->dmaDownloadToStaging(0, &box, &mappedData, &rowPitch, lock);
    if (st != SVGA3_VLKN_SUCCESS) return st;
    if (counters) counters->readbackBytes += size_t(box.w) * box.h * bpp;
    bool boundsValid = true;
    forEachRect([&](SVGA3dCopyRect r) {
        const uint32_t x = r.x, y = r.y;
        r.srcx -= left; r.srcy -= top;
        if (!copyPresentationRect(fb.scanoutHva(), fb.scanoutSize(), dstPitch, dstBpp,
                                  static_cast<const uint8_t *>(mappedData), rowPitch, bpp, r, counters)) {
            boundsValid = false; return;
        }
        guestMem->notifyDisplayUpdate(x, y, r.w, r.h);
    });
    // dmaDownloadToStaging already completed all commands needed by this frame.
    return boundsValid ? SVGA3_VLKN_SUCCESS : SVGA3_VLKN_ERROR_INVALID_PARAM;
}

Svga3VlknStatus VlknSurfaceManager::setSurfaceActive(uint32_t sid, bool active) {
    VlknSurface *surf = getSurface(sid);
    if (!surf) return SVGA3_VLKN_ERROR_NOT_FOUND;
    surf->setActive(active);
    return SVGA3_VLKN_SUCCESS;
}

} // namespace svga3_vlkn
