#include "svga3_device.h"
#include "svga3_context.h"
#include "svga3_surface.h"
#include "vlkn_backend.h"
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
static unsigned barriers, imageCopies, bufferCopies;
static PFN_vkCreateSampler savedSampler;
static VkSamplerCreateInfo samplerInfo;
static unsigned samplerAttempts;
static bool rejectAniso, rejectAllSamplers;
static VkResult VKAPI_CALL captureSampler(VkDevice device,const VkSamplerCreateInfo *info,const VkAllocationCallbacks *alloc,VkSampler *out) {
    samplerInfo=*info; ++samplerAttempts;
    if (rejectAllSamplers || (rejectAniso && info->anisotropyEnable)) { *out=VK_NULL_HANDLE; return VK_ERROR_OUT_OF_HOST_MEMORY; }
    return savedSampler(device,info,alloc,out);
}
static VkImageAspectFlags clearAspect;
static void VKAPI_CALL captureClear(VkCommandBuffer, uint32_t count, const VkClearAttachment *attachments, uint32_t, const VkClearRect *) { clearAspect = count ? attachments[count-1].aspectMask : 0; }
static VkIndexType boundIndexType = VK_INDEX_TYPE_UINT32;
static void VKAPI_CALL countBarriers(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, uint32_t, const VkMemoryBarrier *, uint32_t, const VkBufferMemoryBarrier *, uint32_t, const VkImageMemoryBarrier *) { ++barriers; }
static void VKAPI_CALL countImageCopies(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout, uint32_t, const VkImageCopy *) { ++imageCopies; }
static void VKAPI_CALL countBufferCopies(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy *) { ++bufferCopies; }
static void VKAPI_CALL captureIndex(VkCommandBuffer, VkBuffer, VkDeviceSize, VkIndexType type) { boundIndexType = type; }
static PFN_vkDestroyImage savedDestroyImage;
static VkImage watchedImage;
static unsigned watchedDestroys, waitCalls, failWaitAt;
static VkResult VKAPI_CALL failQueueWait(VkQueue) { return ++waitCalls==failWaitAt ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS; }
static VkResult VKAPI_CALL failDeviceWait(VkDevice) { return VK_ERROR_OUT_OF_HOST_MEMORY; }
static void VKAPI_CALL watchDestroyImage(VkDevice device,VkImage image,const VkAllocationCallbacks *alloc) {
    if (image==watchedImage) ++watchedDestroys;
    savedDestroyImage(device,image,alloc);
}
static int checkDeferredImages() {
    Svga3VlknConfig cfg{}; cfg.forceMockBackend=true;
    auto *dev=svga3_vlkn_device_create(&cfg); CHECK(dev);
    auto &dispatch=dev->backend->dispatch();
    savedDestroyImage=dispatch.vkDestroyImage; dispatch.vkDestroyImage=watchDestroyImage;
    auto savedWait=dispatch.vkQueueWaitIdle; auto savedIdle=dispatch.vkDeviceWaitIdle;
    SVGA3dSize size{4,4,1};
    for (bool volume : {false,true}) {
        CHECK(dev->surfaceMgr->defineSurface(1,0,SVGA3D_A8R8G8B8,&size,1)==SVGA3_VLKN_SUCCESS);
        CHECK(dev->backend->flushCommandBuffer()==SVGA3_VLKN_SUCCESS);
        auto *surface=dev->surfaceMgr->getSurface(1);
        watchedImage=surface->image(); watchedDestroys=waitCalls=0; failWaitAt=volume?2:1;
        surface->transitionLayout(dev->backend->getActiveCommandBuffer(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        dispatch.vkQueueWaitIdle=failQueueWait;
        if (volume) CHECK(surface->ensureVolumeImage()!=SVGA3_VLKN_SUCCESS);
        else CHECK(dev->surfaceMgr->destroySurface(1)==SVGA3_VLKN_SUCCESS);
        CHECK(watchedDestroys==0);
        dispatch.vkDeviceWaitIdle=failDeviceWait;
        CHECK(dev->backend->waitIdle()!=SVGA3_VLKN_SUCCESS && watchedDestroys==0);
        dispatch.vkDeviceWaitIdle=savedIdle; dispatch.vkQueueWaitIdle=savedWait;
        CHECK(dev->backend->waitIdle()==SVGA3_VLKN_SUCCESS && watchedDestroys==1);
        if (volume) CHECK(dev->surfaceMgr->destroySurface(1)==SVGA3_VLKN_SUCCESS);
    }
    dispatch.vkDestroyImage=savedDestroyImage;
    svga3_vlkn_device_destroy(dev);
    return 0;
}
int main() {
    CHECK(checkDeferredImages()==0);
    Svga3VlknConfig cfg{}; cfg.forceMockBackend = true;
    auto *dev = svga3_vlkn_device_create(&cfg); CHECK(dev);
    CHECK(dev->contextMgr->createContext(1) == SVGA3_VLKN_SUCCESS);
    auto *ctx = dev->contextMgr->getContext(1);
    SVGA3dPrimitiveRange noTargetRange{};
    noTargetRange.primType=SVGA3D_PRIMITIVE_TRIANGLELIST; noTargetRange.primitiveCount=1;
    noTargetRange.indexArray.surfaceId=SVGA3D_INVALID_ID;
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,nullptr,0,&noTargetRange,1)==SVGA3_VLKN_ERROR_INVALID_PARAM);
    CHECK(ctx->setRenderTarget(SVGA3D_RT_DEPTH,9999,0,0)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->clear(SVGA3D_CLEAR_DEPTH,0,1,0,nullptr,0)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->setRenderTarget(SVGA3D_RT_DEPTH,SVGA3D_INVALID_ID,0,0)==SVGA3_VLKN_SUCCESS);
    for (uint32_t factor : {uint32_t(SVGA3D_BLENDOP_SRC1COLOR),uint32_t(SVGA3D_BLENDOP_INVSRC1COLOR),
        uint32_t(SVGA3D_BLENDOP_SRC1ALPHA),uint32_t(SVGA3D_BLENDOP_INVSRC1ALPHA),UINT32_MAX})
        CHECK(ctx->setRenderState(SVGA3D_RS_SRCBLEND,factor)==SVGA3_VLKN_ERROR_INVALID_PARAM);
    auto &features=const_cast<VkPhysicalDeviceFeatures &>(dev->backend->features());
    auto savedFeatures=features;
    features.fillModeNonSolid=features.independentBlend=VK_FALSE;
    CHECK(ctx->setRenderState(SVGA3D_RS_FILLMODE,SVGA3D_FILLMODE_LINE)==SVGA3_VLKN_ERROR_INVALID_PARAM);
    uint32_t maxTargets=0;
    CHECK(svga3_vlkn_query_cap(dev,SVGA3D_DEVCAP_MAX_RENDER_TARGETS,&maxTargets) && maxTargets==1);
    CHECK(ctx->setRenderTarget(SVGA3D_RT_COLOR1,1,0,0)==SVGA3_VLKN_ERROR_INVALID_PARAM);
    features=savedFeatures;
    SVGA3dZRange depthRange{0.25f,0.75f};
    CHECK(ctx->setZRange(&depthRange)==SVGA3_VLKN_SUCCESS);
    SVGA3dRect firstViewport{0,0,4,4}; CHECK(ctx->setViewport(&firstViewport)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->getZRange().min==0.25f && ctx->getZRange().max==0.75f);
    depthRange={-1,2}; CHECK(ctx->setZRange(&depthRange)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->getZRange().min==0 && ctx->getZRange().max==1);
    SVGA3dSize imageSize{4,4,1}, bufferSize{256,1,1};
    for (unsigned sid : {1u,2u,3u}) CHECK(dev->surfaceMgr->defineSurface(sid,0,SVGA3D_A8R8G8B8,&imageSize,1) == SVGA3_VLKN_SUCCESS);
    CHECK(dev->surfaceMgr->defineSurface(4,SVGA3D_SURFACE_HINT_VERTEXBUFFER,SVGA3D_BUFFER,&bufferSize,1) == SVGA3_VLKN_SUCCESS);
    CHECK(dev->surfaceMgr->defineSurface(5,SVGA3D_SURFACE_HINT_INDEXBUFFER,SVGA3D_BUFFER,&bufferSize,1) == SVGA3_VLKN_SUCCESS);
    for (auto format : {SVGA3D_UYVY,SVGA3D_YUY2,SVGA3D_NV12,SVGA3D_AYUV,static_cast<SVGA3dSurfaceFormat>(UINT32_MAX)})
        CHECK(dev->surfaceMgr->defineSurface(99,0,format,&imageSize,1) == SVGA3_VLKN_ERROR_UNSUPPORTED_FORMAT && !dev->surfaceMgr->getSurface(99));
    CHECK(dev->surfaceMgr->defineSurfaceV2(99,0,SVGA3D_A8R8G8B8,4,SVGA3D_TEX_FILTER_NONE,&imageSize,1) != SVGA3_VLKN_SUCCESS && !dev->surfaceMgr->getSurface(99));
    auto *old = dev->surfaceMgr->getSurface(1);
    auto oldImage = old->image();
    size_t budget = dev->backend->resourceBudgets().surfaceBytes();
    svga3_mock_fail_bind_image_memory(1);
    CHECK(dev->surfaceMgr->defineSurface(1,0,SVGA3D_A8R8G8B8,&imageSize,1) != SVGA3_VLKN_SUCCESS);
    CHECK(dev->surfaceMgr->getSurface(1) == old && old->image() == oldImage && dev->backend->resourceBudgets().surfaceBytes() == budget);
    svga3_mock_fail_bind_image_memory(1);
    CHECK(dev->surfaceMgr->defineSurfaceV2(1,0,SVGA3D_A8R8G8B8,1,SVGA3D_TEX_FILTER_LINEAR,&imageSize,1) != SVGA3_VLKN_SUCCESS);
    CHECK(dev->surfaceMgr->getSurface(1) == old && old->image() == oldImage && dev->backend->resourceBudgets().surfaceBytes() == budget);
    CHECK(ctx->setRenderTarget(SVGA3D_RT_COLOR0,1,0,0) == SVGA3_VLKN_SUCCESS);
    SVGA3dRect viewport{0,0,4,4}; CHECK(ctx->setViewport(&viewport) == SVGA3_VLKN_SUCCESS);
    CHECK(ctx->setTextureStageState(0,SVGA3D_TS_BIND_TEXTURE,3) == SVGA3_VLKN_SUCCESS);
    CHECK(dev->backend->flushCommandBuffer() == SVGA3_VLKN_SUCCESS);
    auto &dispatch = dev->backend->dispatch();
    auto savedBarrier = dispatch.vkCmdPipelineBarrier;
    auto savedImageCopy = dispatch.vkCmdCopyImage;
    auto savedBufferCopy = dispatch.vkCmdCopyBuffer;
    auto savedIndex = dispatch.vkCmdBindIndexBuffer;
    dispatch.vkCmdPipelineBarrier = countBarriers; dispatch.vkCmdCopyImage = countImageCopies;
    dispatch.vkCmdCopyBuffer = countBufferCopies; dispatch.vkCmdBindIndexBuffer = captureIndex;
    auto srcLayout = old->currentLayout(), dstLayout = dev->surfaceMgr->getSurface(2)->currentLayout();
    SVGA3dCopyBox boxes[2]{{0,0,0,1,1,1,0,0,0},{4,0,0,1,1,1,0,0,0}};
    CHECK(dev->surfaceMgr->copy(1,2,boxes,2) == SVGA3_VLKN_ERROR_INVALID_PARAM);
    CHECK(!barriers && !imageCopies && !bufferCopies && old->currentLayout() == srcLayout && dev->surfaceMgr->getSurface(2)->currentLayout() == dstLayout);
    boxes[1].x = dev->surfaceMgr->getSurface(5)->bufferSize(); boxes[1].w = 4;
    CHECK(dev->surfaceMgr->copy(4,5,boxes,2) == SVGA3_VLKN_ERROR_INVALID_PARAM);
    CHECK(!barriers && !bufferCopies);
    SVGA3dVertexDecl decl{};
    decl.identity.type = SVGA3D_DECLTYPE_FLOAT3; decl.identity.usage = SVGA3D_DECLUSAGE_POSITION;
    decl.array.surfaceId = 4; decl.array.stride = UINT32_MAX;
    SVGA3dPrimitiveRange range{};
    range.primType = SVGA3D_PRIMITIVE_TRIANGLELIST; range.primitiveCount = 1;
    range.indexArray.surfaceId = SVGA3D_INVALID_ID;
    auto *texture = dev->surfaceMgr->getSurface(3); auto textureLayout = texture->currentLayout();
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,&decl,1,&range,1) == SVGA3_VLKN_ERROR_INVALID_PARAM);
    CHECK(!barriers && texture->currentLayout() == textureLayout);
    decl.array.stride = 12; decl.identity.type = static_cast<SVGA3dDeclType>(UINT32_MAX);
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,&decl,1,&range,1) == SVGA3_VLKN_ERROR_INVALID_PARAM && !barriers);
    decl.identity.type = SVGA3D_DECLTYPE_FLOAT3;
    range.indexArray.surfaceId = 5; range.indexWidth = 2; range.indexArray.stride = 4;
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,&decl,1,&range,1) == SVGA3_VLKN_ERROR_INVALID_PARAM && !barriers);
    range.indexArray.stride = 0;
    SVGA3dVertexDecl duplicate[2]{decl,decl};
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,duplicate,2,&range,1)==SVGA3_VLKN_ERROR_INVALID_PARAM && !barriers);
    duplicate[1].identity.usage=SVGA3D_DECLUSAGE_TEXCOORD; duplicate[1].identity.usageIndex=8;
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,duplicate,2,&range,1)==SVGA3_VLKN_ERROR_INVALID_PARAM && !barriers);
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,&decl,1,&range,1) == SVGA3_VLKN_SUCCESS && boundIndexType == VK_INDEX_TYPE_UINT16);
    savedSampler=dispatch.vkCreateSampler; dispatch.vkCreateSampler=captureSampler;
    CHECK(ctx->setTexture(0,3)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->setTextureStageState(0,SVGA3D_TS_MINFILTER,SVGA3D_TEX_FILTER_ANISOTROPIC)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->setTextureStageState(0,SVGA3D_TS_MAGFILTER,SVGA3D_TEX_FILTER_ANISOTROPIC)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->setTextureStageState(0,SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL,UINT32_MAX)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,&decl,1,&range,1)==SVGA3_VLKN_SUCCESS);
    CHECK(samplerInfo.minFilter==VK_FILTER_LINEAR && samplerInfo.magFilter==VK_FILTER_LINEAR && samplerInfo.anisotropyEnable && samplerInfo.maxAnisotropy==16);
    features.samplerAnisotropy=VK_FALSE;
    CHECK(ctx->setTextureStageState(0,SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL,123)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,&decl,1,&range,1)==SVGA3_VLKN_SUCCESS && !samplerInfo.anisotropyEnable);
    CHECK(svga3_vlkn_query_cap(dev,SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY,&maxTargets) && maxTargets==1);
    features=savedFeatures; rejectAniso=true;
    CHECK(ctx->setTextureStageState(0,SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL,124)==SVGA3_VLKN_SUCCESS);
    samplerAttempts=0;
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,&decl,1,&range,1)==SVGA3_VLKN_SUCCESS && samplerAttempts==2 && !samplerInfo.anisotropyEnable);
    rejectAllSamplers=true;
    CHECK(ctx->setTextureStageState(0,SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL,125)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,&decl,1,&range,1)!=SVGA3_VLKN_SUCCESS);
    rejectAniso=rejectAllSamplers=false; samplerAttempts=0;
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,&decl,1,&range,1)==SVGA3_VLKN_SUCCESS && samplerAttempts==1);
    CHECK(ctx->setTextureStageState(0,SVGA3D_TS_ADDRESSU,SVGA3D_TEX_ADDRESS_MIRRORONCE)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,&decl,1,&range,1)==SVGA3_VLKN_SUCCESS && samplerInfo.addressModeU==VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    dispatch.vkCreateSampler=savedSampler;
    auto savedClear = dispatch.vkCmdClearAttachments;
    dispatch.vkCmdClearAttachments = captureClear;
    for (auto format : {SVGA3D_Z_D16,SVGA3D_Z_D24S8}) {
        CHECK(dev->surfaceMgr->defineSurface(6,SVGA3D_SURFACE_HINT_DEPTHSTENCIL,format,&imageSize,1) == SVGA3_VLKN_SUCCESS);
        CHECK(ctx->setRenderTarget(SVGA3D_RT_DEPTH,6,0,0) == SVGA3_VLKN_SUCCESS);
        clearAspect = 0;
        CHECK(ctx->clear(static_cast<SVGA3dClearFlag>(SVGA3D_CLEAR_DEPTH|SVGA3D_CLEAR_STENCIL),0,1,3,nullptr,0) == SVGA3_VLKN_SUCCESS);
        CHECK(clearAspect == (format == SVGA3D_Z_D16 ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT));
    }
    dispatch.vkCmdClearAttachments = savedClear;
    dispatch.vkCmdPipelineBarrier = savedBarrier; dispatch.vkCmdCopyImage = savedImageCopy;
    dispatch.vkCmdCopyBuffer = savedBufferCopy; dispatch.vkCmdBindIndexBuffer = savedIndex;
    svga3_vlkn_device_destroy(dev);
    std::puts("Atomic surface replacement/copy, early draw validation and indexWidth: PASS");
}
