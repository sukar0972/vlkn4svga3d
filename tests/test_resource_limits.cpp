#include "svga3_device.h"
#include "svga3_context.h"
#include "svga3_surface.h"
#include "vlkn_backend.h"
#include <cstdio>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while (0)
static PFN_vkCreateGraphicsPipelines savedCreate;
static PFN_vkDestroyPipeline savedDestroy;
static unsigned created, live, peak;
static uint32_t stencilReference;
static VkPipelineRasterizationStateCreateInfo raster;
static VkResult VKAPI_CALL countCreate(VkDevice device,VkPipelineCache cache,uint32_t count,const VkGraphicsPipelineCreateInfo *info,const VkAllocationCallbacks *alloc,VkPipeline *out) {
    raster=*info->pRasterizationState;
    auto result = savedCreate(device,cache,count,info,alloc,out);
    if (result==VK_SUCCESS) { created+=count; live+=count; peak=std::max(peak,live); }
    return result;
}
static void VKAPI_CALL countDestroy(VkDevice device,VkPipeline pipeline,const VkAllocationCallbacks *alloc) { if (pipeline) --live; savedDestroy(device,pipeline,alloc); }
static void VKAPI_CALL captureReference(VkCommandBuffer,VkStencilFaceFlags,uint32_t value) { stencilReference=value; }
int main() {
    Svga3VlknConfig cfg{}; cfg.forceMockBackend=true; cfg.maxContexts=2; cfg.maxSurfaces=2;
    auto *dev=svga3_vlkn_device_create(&cfg); CHECK(dev);
    uint32_t cap=0;
    CHECK(svga3_vlkn_query_cap(dev,SVGA3D_DEVCAP_MAX_CONTEXT_IDS,&cap) && cap==2);
    CHECK(svga3_vlkn_query_cap(dev,SVGA3D_DEVCAP_MAX_SURFACE_IDS,&cap) && cap==2);
    CHECK(dev->contextMgr->createContext(1)==SVGA3_VLKN_SUCCESS);
    CHECK(dev->contextMgr->createContext(2)==SVGA3_VLKN_SUCCESS);
    CHECK(dev->contextMgr->createContext(3)==SVGA3_VLKN_ERROR_OUT_OF_MEMORY);
    CHECK(dev->contextMgr->destroyContext(2)==SVGA3_VLKN_SUCCESS && dev->contextMgr->createContext(3)==SVGA3_VLKN_SUCCESS);
    SVGA3dSize size{4,4,1};
    CHECK(dev->surfaceMgr->defineSurface(1,0,SVGA3D_A8R8G8B8,&size,1)==SVGA3_VLKN_SUCCESS);
    CHECK(dev->surfaceMgr->defineSurfaceV2(2,0,SVGA3D_Z_D24S8,1,SVGA3D_TEX_FILTER_NONE,&size,1)==SVGA3_VLKN_SUCCESS);
    CHECK(dev->surfaceMgr->defineSurface(3,0,SVGA3D_A8R8G8B8,&size,1)==SVGA3_VLKN_ERROR_OUT_OF_MEMORY);
    CHECK(dev->surfaceMgr->defineSurfaceV2(3,0,SVGA3D_A8R8G8B8,1,SVGA3D_TEX_FILTER_NONE,&size,1)==SVGA3_VLKN_ERROR_OUT_OF_MEMORY);
    CHECK(dev->surfaceMgr->defineSurface(1,0,SVGA3D_A8R8G8B8,&size,1)==SVGA3_VLKN_SUCCESS);
    auto *ctx=dev->contextMgr->getContext(1);
    CHECK(ctx->setRenderTarget(SVGA3D_RT_COLOR0,1,0,0)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->setRenderTarget(SVGA3D_RT_DEPTH,2,0,0)==SVGA3_VLKN_SUCCESS);
    SVGA3dRect vp{0,0,4,4}; CHECK(ctx->setViewport(&vp)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->setRenderState(SVGA3D_RS_STENCILENABLE,1)==SVGA3_VLKN_SUCCESS);
    auto &dispatch=dev->backend->dispatch(); savedCreate=dispatch.vkCreateGraphicsPipelines; savedDestroy=dispatch.vkDestroyPipeline;
    auto savedReference=dispatch.vkCmdSetStencilReference;
    dispatch.vkCreateGraphicsPipelines=countCreate; dispatch.vkDestroyPipeline=countDestroy; dispatch.vkCmdSetStencilReference=captureReference;
    SVGA3dPrimitiveRange range{}; range.primType=SVGA3D_PRIMITIVE_TRIANGLELIST; range.primitiveCount=1;
    range.indexArray.surfaceId=SVGA3D_INVALID_ID;
    for (unsigned i=0;i<2000;++i) {
        CHECK(ctx->setRenderState(SVGA3D_RS_STENCILREF,i)==SVGA3_VLKN_SUCCESS);
        CHECK(ctx->setRenderState(SVGA3D_RS_STENCILMASK,i)==SVGA3_VLKN_SUCCESS);
        CHECK(ctx->setRenderState(SVGA3D_RS_STENCILWRITEMASK,i)==SVGA3_VLKN_SUCCESS);
        CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,nullptr,0,&range,1)==SVGA3_VLKN_SUCCESS && stencilReference==i);
    }
    CHECK(created==1 && live==1);
    CHECK(ctx->setRenderState(SVGA3D_RS_CLIPPING,0)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->setRenderState(SVGA3D_RS_LINEWIDTH,0x40200000)==SVGA3_VLKN_SUCCESS); // 2.5f
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,nullptr,0,&range,1)==SVGA3_VLKN_SUCCESS);
    CHECK(created==2 && raster.depthClampEnable && raster.lineWidth==2.5f);
    auto &features=const_cast<VkPhysicalDeviceFeatures &>(dev->backend->features()); auto savedFeatures=features;
    features.depthClamp=features.wideLines=VK_FALSE;
    CHECK(ctx->setRenderState(SVGA3D_RS_CLIPPING,0)==SVGA3_VLKN_ERROR_INVALID_PARAM);
    CHECK(ctx->setRenderState(SVGA3D_RS_CLIPPING,1)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->setRenderState(SVGA3D_RS_LINEWIDTH,0x40200000)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->setRenderState(SVGA3D_RS_CULLMODE,SVGA3D_FACE_FRONT_BACK)==SVGA3_VLKN_SUCCESS);
    CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,nullptr,0,&range,1)==SVGA3_VLKN_SUCCESS);
    CHECK(!raster.depthClampEnable && raster.lineWidth==1);
    features=savedFeatures;
    CHECK(ctx->setRenderState(SVGA3D_RS_ALPHATESTENABLE,1)==SVGA3_VLKN_SUCCESS);
    for (unsigned i=0;i<600;++i) {
        CHECK(ctx->setRenderState(SVGA3D_RS_ALPHAREF,i)==SVGA3_VLKN_SUCCESS);
        CHECK(ctx->draw(SVGA3D_PRIMITIVE_TRIANGLELIST,nullptr,0,&range,1)==SVGA3_VLKN_SUCCESS);
        CHECK(live<=256);
    }
    CHECK(created>256 && peak<=256);
    dev->contextMgr->clear(); CHECK(live==0);
    dispatch.vkCreateGraphicsPipelines=savedCreate; dispatch.vkDestroyPipeline=savedDestroy; dispatch.vkCmdSetStencilReference=savedReference;
    svga3_vlkn_device_destroy(dev);
    std::puts("Configured context/surface capacity, dynamic stencil state and bounded pipelines: PASS");
}
