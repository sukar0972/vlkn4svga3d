/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "svga3_device.h"
#include "svga3_surface.h"
#include "vlkn_backend.h"
#include <array>
#include <cstdio>
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while (0)
static VkImage watched;
static unsigned barriers;
static bool valid;
static PFN_vkCmdPipelineBarrier originalBarrier;
static void VKAPI_CALL capture(VkCommandBuffer cb,VkPipelineStageFlags src,VkPipelineStageFlags dst,
    VkDependencyFlags flags,uint32_t memoryCount,const VkMemoryBarrier *memory,
    uint32_t bufferCount,const VkBufferMemoryBarrier *buffers,uint32_t imageCount,const VkImageMemoryBarrier *images) {
    for (uint32_t i=0;i<imageCount;++i) if (images[i].image==watched) {
        ++barriers;
        valid &= images[i].subresourceRange.baseArrayLayer==0 &&
            images[i].subresourceRange.layerCount==VK_REMAINING_ARRAY_LAYERS;
    }
    originalBarrier(cb,src,dst,flags,memoryCount,memory,bufferCount,buffers,imageCount,images);
}
int main() {
    Svga3VlknConfig cfg{}; cfg.forceMockBackend=true;
    auto *dev=svga3_vlkn_device_create(&cfg); CHECK(dev);
    originalBarrier=dev->backend->dispatch().vkCmdPipelineBarrier;
    dev->backend->dispatch().vkCmdPipelineBarrier=capture;
    for (unsigned depth:{4u,1u}) {
        SVGA3dSize size{2,3,depth};
        CHECK(svga3_vlkn_surface_define(dev,7,SVGA3D_SURFACE_HINT_TEXTURE,
            SVGA3D_A8R8G8B8,&size,1)==SVGA3_VLKN_SUCCESS);
        auto *surface=dev->surfaceMgr->getSurface(7);
        if (depth==1) CHECK(surface->ensureVolumeImage()==SVGA3_VLKN_SUCCESS);
        watched=surface->image(); barriers=0; valid=true;
        std::array<uint32_t,24> pixels{};
        CHECK(svga3_vlkn_surface_dma_upload(dev,7,0,nullptr,pixels.data(),8)==SVGA3_VLKN_SUCCESS);
        CHECK(svga3_vlkn_surface_dma_download(dev,7,0,nullptr,pixels.data(),8)==SVGA3_VLKN_SUCCESS);
        surface->transitionLayout(dev->backend->getActiveCommandBuffer(),VK_IMAGE_LAYOUT_GENERAL);
        CHECK(barriers>=3 && valid);
        watched=VK_NULL_HANDLE;
        CHECK(svga3_vlkn_surface_destroy(dev,7)==SVGA3_VLKN_SUCCESS);
    }
    dev->backend->dispatch().vkCmdPipelineBarrier=originalBarrier;
    svga3_vlkn_device_destroy(dev);
    std::puts("3D and promoted depth-one volume barriers cover all slices: PASS");
}
