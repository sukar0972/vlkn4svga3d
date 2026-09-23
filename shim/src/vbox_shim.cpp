#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

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

extern "C" {

/* Memory allocation */
void *RTMemAlloc(size_t cb) {
    return malloc(cb);
}

void *RTMemAllocZ(size_t cb) {
    return calloc(1, cb);
}

void *RTMemRealloc(void *pv, size_t cb) {
    return realloc(pv, cb);
}

void RTMemFree(void *pv) {
    if (pv) free(pv);
}

/* Event Semaphores */
int RTSemEventCreate(RTSEMEVENT *pSem) {
    if (pSem) *pSem = (RTSEMEVENT)1;
    return VINF_SUCCESS;
}

int RTSemEventDestroy(RTSEMEVENT hEventSem) {
    (void)hEventSem;
    return VINF_SUCCESS;
}

int RTSemEventSignal(RTSEMEVENT Sem) {
    (void)Sem;
    return VINF_SUCCESS;
}

int RTSemEventWait(RTSEMEVENT Sem, uint32_t cMillies) {
    (void)Sem; (void)cMillies;
    return VINF_SUCCESS;
}

/* Threads */
int RTThreadCreate(RTTHREAD *pThread, int (*pfnThread)(RTTHREAD, void *), void *pvUser, size_t cbStack, int enmType, unsigned fFlags, const char *pszName) {
    (void)pfnThread; (void)pvUser; (void)cbStack; (void)enmType; (void)fFlags; (void)pszName;
    if (pThread) *pThread = (RTTHREAD)1;
    return VINF_SUCCESS;
}

void RTThreadSleep(uint32_t cMillies) {
    (void)cMillies;
}

/* PDM helpers */
int PDMDevHlpVMSetError(PPDMDEVINS pDevIns, int rc, const char *pszFile, unsigned uLine, const char *pszFunction, const char *pszFormat, ...) {
    (void)pDevIns; (void)pszFile; (void)uLine; (void)pszFunction; (void)pszFormat;
    return rc;
}

void *RTLdrGetSystemSymbol(const char *pszModule, const char *pszSymbol) {
    (void)pszModule; (void)pszSymbol;
    return NULL;
}

/* AVL U32 Tree */
PAVLU32NODECORE RTAvlU32Get(PAVLU32TREE pTree, uint32_t Key) {
    if (!pTree) return NULL;
    PAVLU32NODECORE cur = *pTree;
    while (cur) {
        if (Key == cur->Key) return cur;
        if (Key < cur->Key) cur = cur->pLeft;
        else cur = cur->pRight;
    }
    return NULL;
}

bool RTAvlU32Insert(PAVLU32TREE pTree, PAVLU32NODECORE pNode) {
    if (!pTree || !pNode) return false;
    pNode->pLeft = pNode->pRight = NULL;
    if (!*pTree) {
        *pTree = pNode;
        return true;
    }
    PAVLU32NODECORE cur = *pTree;
    while (true) {
        if (pNode->Key == cur->Key) return false;
        if (pNode->Key < cur->Key) {
            if (!cur->pLeft) { cur->pLeft = pNode; return true; }
            cur = cur->pLeft;
        } else {
            if (!cur->pRight) { cur->pRight = pNode; return true; }
            cur = cur->pRight;
        }
    }
}

PAVLU32NODECORE RTAvlU32Remove(PAVLU32TREE pTree, uint32_t Key) {
    if (!pTree || !*pTree) return NULL;
    PAVLU32NODECORE *pCur = pTree;
    while (*pCur && (*pCur)->Key != Key) {
        if (Key < (*pCur)->Key) pCur = &((*pCur)->pLeft);
        else pCur = &((*pCur)->pRight);
    }
    if (!*pCur) return NULL;
    PAVLU32NODECORE toRemove = *pCur;
    if (!toRemove->pLeft) {
        *pCur = toRemove->pRight;
    } else if (!toRemove->pRight) {
        *pCur = toRemove->pLeft;
    } else {
        PAVLU32NODECORE *pSucc = &(toRemove->pRight);
        while ((*pSucc)->pLeft) {
            pSucc = &((*pSucc)->pLeft);
        }
        PAVLU32NODECORE succ = *pSucc;
        *pSucc = succ->pRight;
        succ->pLeft = toRemove->pLeft;
        succ->pRight = toRemove->pRight;
        *pCur = succ;
    }
    toRemove->pLeft = toRemove->pRight = NULL;
    return toRemove;
}

static void DestroyNode(PAVLU32NODECORE node, PFNAVLU32CALLBACK pfnCallback, void *pvParam) {
    if (!node) return;
    DestroyNode(node->pLeft, pfnCallback, pvParam);
    DestroyNode(node->pRight, pfnCallback, pvParam);
    if (pfnCallback) pfnCallback(node, pvParam);
}

int RTAvlU32Destroy(PAVLU32TREE pTree, PFNAVLU32CALLBACK pfnCallback, void *pvParam) {
    if (!pTree || !*pTree) return VINF_SUCCESS;
    DestroyNode(*pTree, pfnCallback, pvParam);
    *pTree = NULL;
    return VINF_SUCCESS;
}

} /* extern "C" */

/* VirtualBox SVGA device callbacks and helpers called by DevVGA-SVGA3d-win.cpp */

int vgaR3UpdateDisplay(PVGASTATE pVGAState, unsigned xStart, unsigned yStart, unsigned width, unsigned height) {
    (void)pVGAState; (void)xStart; (void)yStart; (void)width; (void)height;
    return VINF_SUCCESS;
}

int vmsvga3dSaveShaderConst(PVMSVGA3DCONTEXT pContext, uint32_t reg, SVGA3dShaderType type, SVGA3dShaderConstType ctype, uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3) {
    (void)pContext; (void)reg; (void)type; (void)ctype; (void)v0; (void)v1; (void)v2; (void)v3;
    return VINF_SUCCESS;
}

int vmsvga3dSendThreadMessage(void *pvThread, void *pvSem, unsigned int uMsg, unsigned long wParam, long lParam) {
    (void)pvThread; (void)pvSem; (void)lParam;
    if (uMsg == WM_VMSVGA3D_CREATEWINDOW) {
        if (wParam) {
            *(HWND*)wParam = (HWND)(uintptr_t)0x10001;
        }
    }
    return VINF_SUCCESS;
}

int vmsvga3dWindowThread(void *pvUser1, void *pvUser2) {
    (void)pvUser1; (void)pvUser2;
    return VINF_SUCCESS;
}

void vmsvgaClipCopyBox(const SVGA3dSize *pSrcSize, const SVGA3dSize *pDstSize, SVGA3dCopyBox *pBox) {
    if (!pBox || !pSrcSize || !pDstSize) return;
    if (pBox->x >= pDstSize->width || pBox->y >= pDstSize->height || pBox->z >= pDstSize->depth ||
        pBox->srcx >= pSrcSize->width || pBox->srcy >= pSrcSize->height || pBox->srcz >= pSrcSize->depth) {
        pBox->w = pBox->h = pBox->d = 0;
        return;
    }
    if (pBox->x + pBox->w > pDstSize->width)
        pBox->w = pDstSize->width - pBox->x;
    if (pBox->srcx + pBox->w > pSrcSize->width)
        pBox->w = pSrcSize->width - pBox->srcx;
    if (pBox->y + pBox->h > pDstSize->height)
        pBox->h = pDstSize->height - pBox->y;
    if (pBox->srcy + pBox->h > pSrcSize->height)
        pBox->h = pSrcSize->height - pBox->srcy;
    if (pBox->z + pBox->d > pDstSize->depth)
        pBox->d = pDstSize->depth - pBox->z;
    if (pBox->srcz + pBox->d > pSrcSize->depth)
        pBox->d = pSrcSize->depth - pBox->srcz;
}

int vmsvgaGMRTransfer(PVGASTATE pThis, SVGA3dTransferType transfer, uint8_t *pbData, int cbDataPitch, SVGAGuestPtr GuestPtr, uint32_t offGuest, int cbGuestPitch, uint32_t cbWidth, uint32_t cLines) {
    (void)pThis; (void)transfer; (void)pbData; (void)cbDataPitch; (void)GuestPtr; (void)offGuest; (void)cbGuestPitch; (void)cbWidth; (void)cLines;
    return VINF_SUCCESS;
}

int vmsvga3dSurfaceDMA(PVGASTATE pThis, SVGA3dGuestImage guest, SVGA3dSurfaceImageId host, SVGA3dTransferType transfer, uint32_t cCopyBoxes, SVGA3dCopyBox *pBoxes) {
    (void)pThis; (void)guest; (void)host; (void)transfer; (void)cCopyBoxes; (void)pBoxes;
    return VINF_SUCCESS;
}

int vmsvga3dSurfaceStretchBlt(PVGASTATE pThis, SVGA3dSurfaceImageId const *pDstSfcImg, SVGA3dBox const *pDstBox,
                              SVGA3dSurfaceImageId const *pSrcSfcImg, SVGA3dBox const *pSrcBox,
                              SVGA3dStretchBltMode enmMode) {
    PVMSVGA3DSTATE pState = pThis->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);
    PVMSVGA3DSURFACE pDstSurface = NULL, pSrcSurface = NULL;
    int rc = vmsvga3dSurfaceFromSid(pState, pDstSfcImg->sid, &pDstSurface);
    AssertRCReturn(rc, rc);
    rc = vmsvga3dSurfaceFromSid(pState, pSrcSfcImg->sid, &pSrcSurface);
    AssertRCReturn(rc, rc);
    PVMSVGA3DCONTEXT pContext = pState->cContexts > 0 ? pState->papContexts[0] : NULL;
    if (pContext) {
        if (!pSrcSurface->u.pTexture) {
            rc = vmsvga3dBackCreateTexture(pState, pContext, pContext->id, pSrcSurface);
            AssertRCReturn(rc, rc);
        }
        if (!pDstSurface->u.pTexture) {
            rc = vmsvga3dBackCreateTexture(pState, pContext, pContext->id, pDstSurface);
            AssertRCReturn(rc, rc);
        }
    }
    return vmsvga3dBackSurfaceStretchBlt(pThis, pState, pDstSurface, pDstSfcImg->face, pDstSfcImg->mipmap, pDstBox,
                                         pSrcSurface, pSrcSfcImg->face, pSrcSfcImg->mipmap, pSrcBox, enmMode, pContext);
}

int vmsvga3dSurfaceDefine(PVGASTATE pThis, uint32_t sid, uint32_t surfaceFlags, SVGA3dSurfaceFormat format, SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES], uint32_t multisampleCount, SVGA3dTextureFilter autogenFilter, uint32_t cMipLevels, SVGA3dSize *pMipLevelSize) {
    PVMSVGA3DSTATE pState = pThis->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);

    if (sid >= pState->cSurfaces) {
        uint32_t cNew = (sid + 16) & ~15;
        void *pvNew = RTMemRealloc(pState->papSurfaces, sizeof(PVMSVGA3DSURFACE) * cNew);
        AssertReturn(pvNew, VERR_NO_MEMORY);
        pState->papSurfaces = (PVMSVGA3DSURFACE *)pvNew;
        while (pState->cSurfaces < cNew) {
            PVMSVGA3DSURFACE pS = (PVMSVGA3DSURFACE)RTMemAllocZ(sizeof(VMSVGA3DSURFACE));
            AssertReturn(pS, VERR_NO_MEMORY);
            pS->id = SVGA3D_INVALID_ID;
            pState->papSurfaces[pState->cSurfaces++] = pS;
        }
    }

    PVMSVGA3DSURFACE pSurface = pState->papSurfaces[sid];
    if (pSurface->pMipmapLevels) {
        RTMemFree(pSurface->pMipmapLevels);
        pSurface->pMipmapLevels = NULL;
    }
    memset(pSurface, 0, sizeof(*pSurface));

    pSurface->id = sid;
    pSurface->surfaceFlags = surfaceFlags;
    pSurface->format = format;
    pSurface->formatD3D = vmsvga3dSurfaceFormat2D3D(format);
    pSurface->fUsageD3D = D3DUSAGE_RENDERTARGET;
    pSurface->idAssociatedContext = SVGA3D_INVALID_ID;
    pSurface->enmD3DResType = VMSVGA3D_D3DRESTYPE_NONE;
    pSurface->multiSampleCount = multisampleCount;
    pSurface->autogenFilter = autogenFilter;
    pSurface->cFaces = (surfaceFlags & SVGA3D_SURFACE_CUBEMAP) ? 6 : 1;
    for (uint32_t f = 0; f < SVGA3D_MAX_SURFACE_FACES; ++f) {
        pSurface->faces[f] = face[f];
    }
    pSurface->cMipmapLevels = cMipLevels;
    pSurface->pMipmapLevels = (PVMSVGA3DMIPMAPLEVEL)RTMemAllocZ(cMipLevels * sizeof(VMSVGA3DMIPMAPLEVEL));
    if (pMipLevelSize && pSurface->pMipmapLevels) {
        for (uint32_t i = 0; i < cMipLevels; ++i) {
            pSurface->pMipmapLevels[i].mipmapSize = pMipLevelSize[i];
            pSurface->pMipmapLevels[i].cbSurfacePitch = pMipLevelSize[i].width * 4;
            pSurface->pMipmapLevels[i].cbSurfacePlane = pMipLevelSize[i].width * pMipLevelSize[i].height * 4;
        }
    }
    pSurface->cxBlock = 1;
    pSurface->cyBlock = 1;
    pSurface->cbBlock = 4;

    return VINF_SUCCESS;
}

int vmsvga3dSurfaceDestroy(PVGASTATE pThis, uint32_t sid) {
    PVMSVGA3DSTATE pState = pThis->svga.p3dState;
    if (!pState || sid >= pState->cSurfaces || !pState->papSurfaces[sid])
        return VINF_SUCCESS;

    PVMSVGA3DSURFACE pSurface = pState->papSurfaces[sid];
    if (pSurface->id == SVGA3D_INVALID_ID)
        return VINF_SUCCESS;

    vmsvga3dBackSurfaceDestroy(pState, pSurface);
    if (pSurface->pMipmapLevels) {
        RTMemFree(pSurface->pMipmapLevels);
        pSurface->pMipmapLevels = NULL;
    }
    memset(pSurface, 0, sizeof(*pSurface));
    pSurface->id = SVGA3D_INVALID_ID;
    return VINF_SUCCESS;
}

const char *vmsvga3dGetRenderStateName(uint32_t state) {
    switch (state) {
    case SVGA3D_RS_ZENABLE: return "SVGA3D_RS_ZENABLE";
    case SVGA3D_RS_ZWRITEENABLE: return "SVGA3D_RS_ZWRITEENABLE";
    case SVGA3D_RS_ALPHATESTENABLE: return "SVGA3D_RS_ALPHATESTENABLE";
    case SVGA3D_RS_DITHERENABLE: return "SVGA3D_RS_DITHERENABLE";
    case SVGA3D_RS_BLENDENABLE: return "SVGA3D_RS_BLENDENABLE";
    case SVGA3D_RS_FOGENABLE: return "SVGA3D_RS_FOGENABLE";
    case SVGA3D_RS_SPECULARENABLE: return "SVGA3D_RS_SPECULARENABLE";
    case SVGA3D_RS_STENCILENABLE: return "SVGA3D_RS_STENCILENABLE";
    case SVGA3D_RS_LIGHTINGENABLE: return "SVGA3D_RS_LIGHTINGENABLE";
    case SVGA3D_RS_NORMALIZENORMALS: return "SVGA3D_RS_NORMALIZENORMALS";
    case SVGA3D_RS_POINTSPRITEENABLE: return "SVGA3D_RS_POINTSPRITEENABLE";
    case SVGA3D_RS_POINTSCALEENABLE: return "SVGA3D_RS_POINTSCALEENABLE";
    case SVGA3D_RS_STENCILREF: return "SVGA3D_RS_STENCILREF";
    case SVGA3D_RS_STENCILMASK: return "SVGA3D_RS_STENCILMASK";
    case SVGA3D_RS_STENCILWRITEMASK: return "SVGA3D_RS_STENCILWRITEMASK";
    case SVGA3D_RS_FOGSTART: return "SVGA3D_RS_FOGSTART";
    case SVGA3D_RS_FOGEND: return "SVGA3D_RS_FOGEND";
    case SVGA3D_RS_FOGDENSITY: return "SVGA3D_RS_FOGDENSITY";
    case SVGA3D_RS_POINTSIZE: return "SVGA3D_RS_POINTSIZE";
    case SVGA3D_RS_POINTSIZEMIN: return "SVGA3D_RS_POINTSIZEMIN";
    case SVGA3D_RS_POINTSIZEMAX: return "SVGA3D_RS_POINTSIZEMAX";
    case SVGA3D_RS_POINTSCALE_A: return "SVGA3D_RS_POINTSCALE_A";
    case SVGA3D_RS_POINTSCALE_B: return "SVGA3D_RS_POINTSCALE_B";
    case SVGA3D_RS_POINTSCALE_C: return "SVGA3D_RS_POINTSCALE_C";
    case SVGA3D_RS_FOGCOLOR: return "SVGA3D_RS_FOGCOLOR";
    case SVGA3D_RS_AMBIENT: return "SVGA3D_RS_AMBIENT";
    case SVGA3D_RS_CLIPPLANEENABLE: return "SVGA3D_RS_CLIPPLANEENABLE";
    case SVGA3D_RS_FOGMODE: return "SVGA3D_RS_FOGMODE";
    case SVGA3D_RS_FILLMODE: return "SVGA3D_RS_FILLMODE";
    case SVGA3D_RS_SHADEMODE: return "SVGA3D_RS_SHADEMODE";
    case SVGA3D_RS_LINEPATTERN: return "SVGA3D_RS_LINEPATTERN";
    case SVGA3D_RS_SRCBLEND: return "SVGA3D_RS_SRCBLEND";
    case SVGA3D_RS_DSTBLEND: return "SVGA3D_RS_DSTBLEND";
    case SVGA3D_RS_BLENDEQUATION: return "SVGA3D_RS_BLENDEQUATION";
    case SVGA3D_RS_CULLMODE: return "SVGA3D_RS_CULLMODE";
    case SVGA3D_RS_ZFUNC: return "SVGA3D_RS_ZFUNC";
    case SVGA3D_RS_ALPHAFUNC: return "SVGA3D_RS_ALPHAFUNC";
    case SVGA3D_RS_STENCILFUNC: return "SVGA3D_RS_STENCILFUNC";
    case SVGA3D_RS_STENCILFAIL: return "SVGA3D_RS_STENCILFAIL";
    case SVGA3D_RS_STENCILZFAIL: return "SVGA3D_RS_STENCILZFAIL";
    case SVGA3D_RS_STENCILPASS: return "SVGA3D_RS_STENCILPASS";
    case SVGA3D_RS_ALPHAREF: return "SVGA3D_RS_ALPHAREF";
    case SVGA3D_RS_FRONTWINDING: return "SVGA3D_RS_FRONTWINDING";
    case SVGA3D_RS_COORDINATETYPE: return "SVGA3D_RS_COORDINATETYPE";
    case SVGA3D_RS_ZBIAS: return "SVGA3D_RS_ZBIAS";
    case SVGA3D_RS_RANGEFOGENABLE: return "SVGA3D_RS_RANGEFOGENABLE";
    case SVGA3D_RS_COLORWRITEENABLE: return "SVGA3D_RS_COLORWRITEENABLE";
    case SVGA3D_RS_VERTEXMATERIALENABLE: return "SVGA3D_RS_VERTEXMATERIALENABLE";
    case SVGA3D_RS_DIFFUSEMATERIALSOURCE: return "SVGA3D_RS_DIFFUSEMATERIALSOURCE";
    case SVGA3D_RS_SPECULARMATERIALSOURCE: return "SVGA3D_RS_SPECULARMATERIALSOURCE";
    case SVGA3D_RS_AMBIENTMATERIALSOURCE: return "SVGA3D_RS_AMBIENTMATERIALSOURCE";
    case SVGA3D_RS_EMISSIVEMATERIALSOURCE: return "SVGA3D_RS_EMISSIVEMATERIALSOURCE";
    case SVGA3D_RS_TEXTUREFACTOR: return "SVGA3D_RS_TEXTUREFACTOR";
    case SVGA3D_RS_LOCALVIEWER: return "SVGA3D_RS_LOCALVIEWER";
    case SVGA3D_RS_SCISSORTESTENABLE: return "SVGA3D_RS_SCISSORTESTENABLE";
    case SVGA3D_RS_BLENDCOLOR: return "SVGA3D_RS_BLENDCOLOR";
    case SVGA3D_RS_STENCILENABLE2SIDED: return "SVGA3D_RS_STENCILENABLE2SIDED";
    case SVGA3D_RS_CCWSTENCILFUNC: return "SVGA3D_RS_CCWSTENCILFUNC";
    case SVGA3D_RS_CCWSTENCILFAIL: return "SVGA3D_RS_CCWSTENCILFAIL";
    case SVGA3D_RS_CCWSTENCILZFAIL: return "SVGA3D_RS_CCWSTENCILZFAIL";
    case SVGA3D_RS_CCWSTENCILPASS: return "SVGA3D_RS_CCWSTENCILPASS";
    case SVGA3D_RS_VERTEXBLEND: return "SVGA3D_RS_VERTEXBLEND";
    case SVGA3D_RS_SLOPESCALEDEPTHBIAS: return "SVGA3D_RS_SLOPESCALEDEPTHBIAS";
    case SVGA3D_RS_DEPTHBIAS: return "SVGA3D_RS_DEPTHBIAS";
    case SVGA3D_RS_OUTPUTGAMMA: return "SVGA3D_RS_OUTPUTGAMMA";
    case SVGA3D_RS_ZVISIBLE: return "SVGA3D_RS_ZVISIBLE";
    case SVGA3D_RS_LASTPIXEL: return "SVGA3D_RS_LASTPIXEL";
    case SVGA3D_RS_CLIPPING: return "SVGA3D_RS_CLIPPING";
    case SVGA3D_RS_MULTISAMPLEANTIALIAS: return "SVGA3D_RS_MULTISAMPLEANTIALIAS";
    case SVGA3D_RS_MULTISAMPLEMASK: return "SVGA3D_RS_MULTISAMPLEMASK";
    case SVGA3D_RS_INDEXEDVERTEXBLENDENABLE: return "SVGA3D_RS_INDEXEDVERTEXBLENDENABLE";
    case SVGA3D_RS_TWEENFACTOR: return "SVGA3D_RS_TWEENFACTOR";
    case SVGA3D_RS_ANTIALIASEDLINEENABLE: return "SVGA3D_RS_ANTIALIASEDLINEENABLE";
    case SVGA3D_RS_COLORWRITEENABLE1: return "SVGA3D_RS_COLORWRITEENABLE1";
    case SVGA3D_RS_COLORWRITEENABLE2: return "SVGA3D_RS_COLORWRITEENABLE2";
    case SVGA3D_RS_COLORWRITEENABLE3: return "SVGA3D_RS_COLORWRITEENABLE3";
    case SVGA3D_RS_SEPARATEALPHABLENDENABLE: return "SVGA3D_RS_SEPARATEALPHABLENDENABLE";
    case SVGA3D_RS_SRCBLENDALPHA: return "SVGA3D_RS_SRCBLENDALPHA";
    case SVGA3D_RS_DSTBLENDALPHA: return "SVGA3D_RS_DSTBLENDALPHA";
    case SVGA3D_RS_BLENDEQUATIONALPHA: return "SVGA3D_RS_BLENDEQUATIONALPHA";
    case SVGA3D_RS_TRANSPARENCYANTIALIAS: return "SVGA3D_RS_TRANSPARENCYANTIALIAS";
    case SVGA3D_RS_LINEAA: return "SVGA3D_RS_LINEAA";
    case SVGA3D_RS_LINEWIDTH: return "SVGA3D_RS_LINEWIDTH";
    default: return "SVGA3D_RS_UNKNOWN";
    }
}

const char *vmsvga3dTextureStateToString(SVGA3dTextureStateName textureState) {
    (void)textureState;
    return "SVGA3D_TS";
}

const char *vmsvgaTransformToString(SVGA3dTransformType type) {
    (void)type;
    return "SVGA3D_TRANSFORM";
}


