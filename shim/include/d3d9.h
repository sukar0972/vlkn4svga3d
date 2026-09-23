#ifndef ___SHIM_D3D9_H___
#define ___SHIM_D3D9_H___

#include "d3d9types.h"
#include "d3d9caps.h"

#ifdef __cplusplus

struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3D9Ex;
struct IDirect3DDevice9Ex;
struct IDirect3DResource9;
struct IDirect3DBaseTexture9;
struct IDirect3DTexture9;
struct IDirect3DCubeTexture9;
struct IDirect3DVolumeTexture9;
struct IDirect3DSurface9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DVertexDeclaration9;
struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;
struct IDirect3DQuery9;

/* IUnknown */
#ifndef __IUnknown_INTERFACE_DEFINED__
#define __IUnknown_INTERFACE_DEFINED__
struct IUnknown {
    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) = 0;
    virtual ULONG WINAPI AddRef(void) = 0;
    virtual ULONG WINAPI Release(void) = 0;
    virtual ~IUnknown() {}
};
#endif

/* Resource description structures */
typedef struct _D3DSURFACE_DESC {
    D3DFORMAT           Format;
    D3DRESOURCETYPE     Type;
    DWORD               Usage;
    D3DPOOL             Pool;
    D3DMULTISAMPLE_TYPE MultiSampleType;
    DWORD               MultiSampleQuality;
    UINT                Width;
    UINT                Height;
} D3DSURFACE_DESC;

typedef struct _D3DVOLUME_DESC {
    D3DFORMAT           Format;
    D3DRESOURCETYPE     Type;
    DWORD               Usage;
    D3DPOOL             Pool;
    UINT                Width;
    UINT                Height;
    UINT                Depth;
} D3DVOLUME_DESC;

typedef struct _D3DVERTEXBUFFER_DESC {
    D3DFORMAT           Format;
    D3DRESOURCETYPE     Type;
    DWORD               Usage;
    D3DPOOL             Pool;
    UINT                Size;
    DWORD               FVF;
} D3DVERTEXBUFFER_DESC;

typedef struct _D3DINDEXBUFFER_DESC {
    D3DFORMAT           Format;
    D3DRESOURCETYPE     Type;
    DWORD               Usage;
    D3DPOOL             Pool;
    UINT                Size;
} D3DINDEXBUFFER_DESC;

/* IDirect3DResource9 */
struct IDirect3DResource9 : public IUnknown {
    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) = 0;
    virtual HRESULT WINAPI SetPrivateData(const void *guid, const void *pData, DWORD SizeOfData, DWORD Flags) = 0;
    virtual HRESULT WINAPI GetPrivateData(const void *guid, void *pData, DWORD *pSizeOfData) = 0;
    virtual HRESULT WINAPI FreePrivateData(const void *guid) = 0;
    virtual DWORD   WINAPI SetPriority(DWORD PriorityNew) = 0;
    virtual DWORD   WINAPI GetPriority(void) = 0;
    virtual void    WINAPI PreLoad(void) = 0;
    virtual D3DRESOURCETYPE WINAPI GetType(void) = 0;
};

/* IDirect3DBaseTexture9 */
struct IDirect3DBaseTexture9 : public IDirect3DResource9 {
    virtual DWORD                WINAPI SetLOD(DWORD LODNew) = 0;
    virtual DWORD                WINAPI GetLOD(void) = 0;
    virtual DWORD                WINAPI GetLevelCount(void) = 0;
    virtual HRESULT              WINAPI SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) = 0;
    virtual D3DTEXTUREFILTERTYPE WINAPI GetAutoGenFilterType(void) = 0;
    virtual void                 WINAPI GenerateMipSubLevels(void) = 0;
};

/* IDirect3DTexture9 */
struct IDirect3DTexture9 : public IDirect3DBaseTexture9 {
    virtual HRESULT WINAPI GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) = 0;
    virtual HRESULT WINAPI GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel) = 0;
    virtual HRESULT WINAPI LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) = 0;
    virtual HRESULT WINAPI UnlockRect(UINT Level) = 0;
    virtual HRESULT WINAPI AddDirtyRect(const RECT *pDirtyRect) = 0;
};

/* IDirect3DCubeTexture9 */
struct IDirect3DCubeTexture9 : public IDirect3DBaseTexture9 {
    virtual HRESULT WINAPI GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) = 0;
    virtual HRESULT WINAPI GetCubeMapSurface(D3DCUBEMAP_FACES FaceType, UINT Level, IDirect3DSurface9 **ppCubeMapSurface) = 0;
    virtual HRESULT WINAPI LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) = 0;
    virtual HRESULT WINAPI UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level) = 0;
    virtual HRESULT WINAPI AddDirtyRect(D3DCUBEMAP_FACES FaceType, const RECT *pDirtyRect) = 0;
};

/* IDirect3DVolumeTexture9 */
struct IDirect3DVolumeTexture9 : public IDirect3DBaseTexture9 {
    virtual HRESULT WINAPI GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc) = 0;
    virtual HRESULT WINAPI GetVolumeLevel(UINT Level, void **ppVolumeLevel) = 0;
    virtual HRESULT WINAPI LockBox(UINT Level, D3DLOCKED_BOX *pLockedVolumeBox, const D3DBOX *pBox, DWORD Flags) = 0;
    virtual HRESULT WINAPI UnlockBox(UINT Level) = 0;
    virtual HRESULT WINAPI AddDirtyBox(const D3DBOX *pDirtyBox) = 0;
};

/* IDirect3DSurface9 */
struct IDirect3DSurface9 : public IDirect3DResource9 {
    virtual HRESULT WINAPI GetContainer(const void *riid, void **ppContainer) = 0;
    virtual HRESULT WINAPI GetDesc(D3DSURFACE_DESC *pDesc) = 0;
    virtual HRESULT WINAPI LockRect(D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) = 0;
    virtual HRESULT WINAPI UnlockRect(void) = 0;
    virtual HRESULT WINAPI GetDC(HDC *phdc) = 0;
    virtual HRESULT WINAPI ReleaseDC(HDC hdc) = 0;
};

/* IDirect3DVertexBuffer9 */
struct IDirect3DVertexBuffer9 : public IDirect3DResource9 {
    virtual HRESULT WINAPI Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) = 0;
    virtual HRESULT WINAPI Unlock(void) = 0;
    virtual HRESULT WINAPI GetDesc(D3DVERTEXBUFFER_DESC *pDesc) = 0;
};

/* IDirect3DIndexBuffer9 */
struct IDirect3DIndexBuffer9 : public IDirect3DResource9 {
    virtual HRESULT WINAPI Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) = 0;
    virtual HRESULT WINAPI Unlock(void) = 0;
    virtual HRESULT WINAPI GetDesc(D3DINDEXBUFFER_DESC *pDesc) = 0;
};

/* IDirect3DVertexDeclaration9 */
struct IDirect3DVertexDeclaration9 : public IUnknown {
    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) = 0;
    virtual HRESULT WINAPI GetDeclaration(D3DVERTEXELEMENT9 *pElement, UINT *pNumElements) = 0;
};

/* IDirect3DVertexShader9 */
struct IDirect3DVertexShader9 : public IUnknown {
    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) = 0;
    virtual HRESULT WINAPI GetFunction(void *pData, UINT *pSizeOfData) = 0;
};

/* IDirect3DPixelShader9 */
struct IDirect3DPixelShader9 : public IUnknown {
    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) = 0;
    virtual HRESULT WINAPI GetFunction(void *pData, UINT *pSizeOfData) = 0;
};

/* IDirect3DQuery9 */
struct IDirect3DQuery9 : public IUnknown {
    virtual HRESULT      WINAPI GetDevice(IDirect3DDevice9 **ppDevice) = 0;
    virtual D3DQUERYTYPE WINAPI GetType(void) = 0;
    virtual DWORD        WINAPI GetDataSize(void) = 0;
    virtual HRESULT      WINAPI Issue(DWORD dwIssueFlags) = 0;
    virtual HRESULT      WINAPI GetData(void *pData, DWORD dwSize, DWORD dwGetDataFlags) = 0;
};

/* IDirect3DDevice9 */
struct IDirect3DDevice9 : public IUnknown {
    virtual HRESULT WINAPI TestCooperativeLevel(void) = 0;
    virtual UINT    WINAPI GetAvailableTextureMem(void) = 0;
    virtual HRESULT WINAPI EvictManagedResources(void) = 0;
    virtual HRESULT WINAPI GetDirect3D(IDirect3D9 **ppD3D9) = 0;
    virtual HRESULT WINAPI GetDeviceCaps(D3DCAPS9 *pCaps) = 0;
    virtual HRESULT WINAPI GetDisplayMode(UINT iSwapChain, void *pMode) = 0;
    virtual HRESULT WINAPI GetCreationParameters(void *pParameters) = 0;
    virtual HRESULT WINAPI SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap) = 0;
    virtual void    WINAPI SetCursorPosition(int X, int Y, DWORD Flags) = 0;
    virtual BOOL    WINAPI ShowCursor(BOOL bShow) = 0;
    virtual HRESULT WINAPI CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters, void **ppSwapChain) = 0;
    virtual HRESULT WINAPI GetSwapChain(UINT iSwapChain, void **ppSwapChain) = 0;
    virtual UINT    WINAPI GetNumberOfSwapChains(void) = 0;
    virtual HRESULT WINAPI Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) = 0;
    virtual HRESULT WINAPI Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const void *pDirtyRegion) = 0;
    virtual HRESULT WINAPI GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) = 0;
    virtual HRESULT WINAPI GetRasterStatus(UINT iSwapChain, void *pRasterStatus) = 0;
    virtual HRESULT WINAPI SetDialogBoxMode(BOOL bEnableDialogs) = 0;
    virtual void    WINAPI SetGammaRamp(UINT iSwapChain, DWORD Flags, const void *pRamp) = 0;
    virtual void    WINAPI GetGammaRamp(UINT iSwapChain, void *pRamp) = 0;
    virtual HRESULT WINAPI CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle) = 0;
    virtual HRESULT WINAPI CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle) = 0;
    virtual HRESULT WINAPI CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9 **ppCubeTexture, HANDLE *pSharedHandle) = 0;
    virtual HRESULT WINAPI CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) = 0;
    virtual HRESULT WINAPI CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle) = 0;
    virtual HRESULT WINAPI CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) = 0;
    virtual HRESULT WINAPI CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) = 0;
    virtual HRESULT WINAPI UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint) = 0;
    virtual HRESULT WINAPI UpdateTexture(IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture) = 0;
    virtual HRESULT WINAPI GetRenderTargetData(IDirect3DSurface9 *pRenderTarget, IDirect3DSurface9 *pDestSurface) = 0;
    virtual HRESULT WINAPI GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9 *pDestSurface) = 0;
    virtual HRESULT WINAPI StretchRect(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestSurface, const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter) = 0;
    virtual HRESULT WINAPI ColorFill(IDirect3DSurface9 *pSurface, const RECT *pRect, D3DCOLOR color) = 0;
    virtual HRESULT WINAPI CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) = 0;
    virtual HRESULT WINAPI SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget) = 0;
    virtual HRESULT WINAPI GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget) = 0;
    virtual HRESULT WINAPI SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil) = 0;
    virtual HRESULT WINAPI GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface) = 0;
    virtual HRESULT WINAPI BeginScene(void) = 0;
    virtual HRESULT WINAPI EndScene(void) = 0;
    virtual HRESULT WINAPI Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) = 0;
    virtual HRESULT WINAPI SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix) = 0;
    virtual HRESULT WINAPI GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix) = 0;
    virtual HRESULT WINAPI MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix) = 0;
    virtual HRESULT WINAPI SetViewport(const D3DVIEWPORT9 *pViewport) = 0;
    virtual HRESULT WINAPI GetViewport(D3DVIEWPORT9 *pViewport) = 0;
    virtual HRESULT WINAPI SetMaterial(const D3DMATERIAL9 *pMaterial) = 0;
    virtual HRESULT WINAPI GetMaterial(D3DMATERIAL9 *pMaterial) = 0;
    virtual HRESULT WINAPI SetLight(DWORD Index, const D3DLIGHT9 *pLight) = 0;
    virtual HRESULT WINAPI GetLight(DWORD Index, D3DLIGHT9 *pLight) = 0;
    virtual HRESULT WINAPI LightEnable(DWORD Index, BOOL Enable) = 0;
    virtual HRESULT WINAPI GetLightEnable(DWORD Index, BOOL *pEnable) = 0;
    virtual HRESULT WINAPI SetClipPlane(DWORD Index, const float *pPlane) = 0;
    virtual HRESULT WINAPI GetClipPlane(DWORD Index, float *pPlane) = 0;
    virtual HRESULT WINAPI SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) = 0;
    virtual HRESULT WINAPI GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) = 0;
    virtual HRESULT WINAPI CreateStateBlock(DWORD Type, void **ppSB) = 0;
    virtual HRESULT WINAPI BeginStateBlock(void) = 0;
    virtual HRESULT WINAPI EndStateBlock(void **ppSB) = 0;
    virtual HRESULT WINAPI SetClipStatus(const void *pClipStatus) = 0;
    virtual HRESULT WINAPI GetClipStatus(void *pClipStatus) = 0;
    virtual HRESULT WINAPI GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) = 0;
    virtual HRESULT WINAPI SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) = 0;
    virtual HRESULT WINAPI GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue) = 0;
    virtual HRESULT WINAPI SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) = 0;
    virtual HRESULT WINAPI GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue) = 0;
    virtual HRESULT WINAPI SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) = 0;
    virtual HRESULT WINAPI ValidateDevice(DWORD *pNumPasses) = 0;
    virtual HRESULT WINAPI SetPaletteEntries(UINT PaletteNumber, const void *pEntries) = 0;
    virtual HRESULT WINAPI GetPaletteEntries(UINT PaletteNumber, void *pEntries) = 0;
    virtual HRESULT WINAPI SetCurrentTexturePalette(UINT PaletteNumber) = 0;
    virtual HRESULT WINAPI GetCurrentTexturePalette(UINT *PaletteNumber) = 0;
    virtual HRESULT WINAPI SetScissorRect(const RECT *pRect) = 0;
    virtual HRESULT WINAPI GetScissorRect(RECT *pRect) = 0;
    virtual HRESULT WINAPI SetSoftwareVertexProcessing(BOOL bSoftware) = 0;
    virtual BOOL    WINAPI GetSoftwareVertexProcessing(void) = 0;
    virtual HRESULT WINAPI SetNPatchMode(float nSegments) = 0;
    virtual float   WINAPI GetNPatchMode(void) = 0;
    virtual HRESULT WINAPI DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) = 0;
    virtual HRESULT WINAPI DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) = 0;
    virtual HRESULT WINAPI DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride) = 0;
    virtual HRESULT WINAPI DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void *pIndexData, D3DFORMAT IndexDataFormat, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride) = 0;
    virtual HRESULT WINAPI ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer, IDirect3DVertexDeclaration9 *pVertexDecl, DWORD Flags) = 0;
    virtual HRESULT WINAPI CreateVertexDeclaration(const D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl) = 0;
    virtual HRESULT WINAPI SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl) = 0;
    virtual HRESULT WINAPI GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl) = 0;
    virtual HRESULT WINAPI SetFVF(DWORD FVF) = 0;
    virtual HRESULT WINAPI GetFVF(DWORD *pFVF) = 0;
    virtual HRESULT WINAPI CreateVertexShader(const DWORD *pFunction, IDirect3DVertexShader9 **ppShader) = 0;
    virtual HRESULT WINAPI SetVertexShader(IDirect3DVertexShader9 *pShader) = 0;
    virtual HRESULT WINAPI GetVertexShader(IDirect3DVertexShader9 **ppShader) = 0;
    virtual HRESULT WINAPI SetVertexShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount) = 0;
    virtual HRESULT WINAPI GetVertexShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount) = 0;
    virtual HRESULT WINAPI SetVertexShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount) = 0;
    virtual HRESULT WINAPI GetVertexShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount) = 0;
    virtual HRESULT WINAPI SetVertexShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT BoolCount) = 0;
    virtual HRESULT WINAPI GetVertexShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount) = 0;
    virtual HRESULT WINAPI SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride) = 0;
    virtual HRESULT WINAPI GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData, UINT *pOffsetInBytes, UINT *pStride) = 0;
    virtual HRESULT WINAPI SetStreamSourceFreq(UINT StreamNumber, UINT Setting) = 0;
    virtual HRESULT WINAPI GetStreamSourceFreq(UINT StreamNumber, UINT *pSetting) = 0;
    virtual HRESULT WINAPI SetIndices(IDirect3DIndexBuffer9 *pIndexData) = 0;
    virtual HRESULT WINAPI GetIndices(IDirect3DIndexBuffer9 **ppIndexData) = 0;
    virtual HRESULT WINAPI CreatePixelShader(const DWORD *pFunction, IDirect3DPixelShader9 **ppShader) = 0;
    virtual HRESULT WINAPI SetPixelShader(IDirect3DPixelShader9 *pShader) = 0;
    virtual HRESULT WINAPI GetPixelShader(IDirect3DPixelShader9 **ppShader) = 0;
    virtual HRESULT WINAPI SetPixelShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount) = 0;
    virtual HRESULT WINAPI GetPixelShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount) = 0;
    virtual HRESULT WINAPI SetPixelShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount) = 0;
    virtual HRESULT WINAPI GetPixelShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount) = 0;
    virtual HRESULT WINAPI SetPixelShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT BoolCount) = 0;
    virtual HRESULT WINAPI GetPixelShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount) = 0;
    virtual HRESULT WINAPI DrawRectPatch(UINT Handle, const float *pNumSegs, const void *pRectPatchInfo) = 0;
    virtual HRESULT WINAPI DrawTriPatch(UINT Handle, const float *pNumSegs, const void *pTriPatchInfo) = 0;
    virtual HRESULT WINAPI DeletePatch(UINT Handle) = 0;
    virtual HRESULT WINAPI CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery) = 0;
};

/* IDirect3DDevice9Ex */
struct IDirect3DDevice9Ex : public IDirect3DDevice9 {
    virtual HRESULT WINAPI SetConvolutionMonoKernel(UINT width, UINT height, float *rows, float *columns) = 0;
    virtual HRESULT WINAPI ComposeRects(IDirect3DSurface9 *pSrc, IDirect3DSurface9 *pDst, IDirect3DVertexBuffer9 *pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9 *pDstRectDescs, DWORD Operation, int Xoffset, int Yoffset) = 0;
    virtual HRESULT WINAPI PresentEx(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const void *pDirtyRegion, DWORD dwFlags) = 0;
    virtual HRESULT WINAPI GetGPUThreadPriority(INT *pPriority) = 0;
    virtual HRESULT WINAPI SetGPUThreadPriority(INT Priority) = 0;
    virtual HRESULT WINAPI WaitForVBlank(UINT iSwapChain) = 0;
    virtual HRESULT WINAPI CheckResourceResidency(IDirect3DResource9 **pResourceArray, UINT NumResources) = 0;
    virtual HRESULT WINAPI SetMaximumFrameLatency(UINT MaxLatency) = 0;
    virtual HRESULT WINAPI GetMaximumFrameLatency(UINT *pMaxLatency) = 0;
    virtual HRESULT WINAPI CheckDeviceState(HWND hDestinationWindow) = 0;
    virtual HRESULT WINAPI CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) = 0;
    virtual HRESULT WINAPI CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) = 0;
    virtual HRESULT WINAPI CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) = 0;
    virtual HRESULT WINAPI ResetEx(D3DPRESENT_PARAMETERS *pPresentationParameters, void *pFullscreenDisplayMode) = 0;
    virtual HRESULT WINAPI GetDisplayModeEx(UINT iSwapChain, void *pMode, void *pRotation) = 0;
};

/* IDirect3D9 */
struct IDirect3D9 : public IUnknown {
    virtual HRESULT WINAPI RegisterSoftwareDevice(void *pInitializeFunction) = 0;
    virtual UINT    WINAPI GetAdapterCount(void) = 0;
    virtual HRESULT WINAPI GetAdapterIdentifier(UINT Adapter, DWORD Flags, void *pIdentifier) = 0;
    virtual UINT    WINAPI GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) = 0;
    virtual HRESULT WINAPI EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, void *pMode) = 0;
    virtual HRESULT WINAPI GetAdapterDisplayMode(UINT Adapter, void *pMode) = 0;
    virtual HRESULT WINAPI CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed) = 0;
    virtual HRESULT WINAPI CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) = 0;
    virtual HRESULT WINAPI CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD *pQualityLevels) = 0;
    virtual HRESULT WINAPI CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) = 0;
    virtual HRESULT WINAPI CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) = 0;
    virtual HRESULT WINAPI GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps) = 0;
    virtual HMONITOR WINAPI GetAdapterMonitor(UINT Adapter) = 0;
    virtual HRESULT WINAPI CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface) = 0;
};

/* IDirect3D9Ex */
struct IDirect3D9Ex : public IDirect3D9 {
    virtual UINT    WINAPI GetAdapterModeCountEx(UINT Adapter, const void *pFilter) = 0;
    virtual HRESULT WINAPI EnumAdapterModesEx(UINT Adapter, const void *pFilter, UINT Mode, void *pMode) = 0;
    virtual HRESULT WINAPI GetAdapterDisplayModeEx(UINT Adapter, void *pMode, void *pRotation) = 0;
    virtual HRESULT WINAPI CreateDeviceEx(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, void *pFullscreenDisplayMode, IDirect3DDevice9Ex **ppReturnedDeviceInterface) = 0;
    virtual HRESULT WINAPI GetAdapterLUID(UINT Adapter, void *pLUID) = 0;
};

extern "C" {
IDirect3D9 * WINAPI Direct3DCreate9(UINT SDKVersion);
HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D);
}

#endif /* __cplusplus */

#endif /* ___SHIM_D3D9_H___ */
