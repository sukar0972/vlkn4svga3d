#ifndef ___SHIM_MOCK_D3D9_H___
#define ___SHIM_MOCK_D3D9_H___

#include <d3d9.h>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <cstring>
#include <atomic>

struct RenderStateRecord {
    D3DRENDERSTATETYPE state;
    DWORD value;
};

struct TextureStageStateRecord {
    DWORD stage;
    D3DTEXTURESTAGESTATETYPE type;
    DWORD value;
};

struct SamplerStateRecord {
    DWORD sampler;
    D3DSAMPLERSTATETYPE type;
    DWORD value;
};

struct ClearRecord {
    DWORD count;
    DWORD flags;
    D3DCOLOR color;
    float z;
    DWORD stencil;
};

struct DrawRecord {
    bool indexed;
    D3DPRIMITIVETYPE primitiveType;
    INT baseVertexIndex;
    UINT minVertexIndex;
    UINT numVertices;
    UINT startIndex;
    UINT primitiveCount;
};

struct StreamSourceRecord {
    UINT streamNumber;
    IDirect3DVertexBuffer9 *pStreamData;
    UINT offsetInBytes;
    UINT stride;
    UINT frequency;
};

class MockDirect3DDevice9;

class MockDirect3DSurface9 : public IDirect3DSurface9 {
public:
    MockDirect3DDevice9 *m_pDevice;
    D3DSURFACE_DESC m_desc;
    std::vector<uint8_t> m_data;
    std::atomic<ULONG> m_refCount{1};

    MockDirect3DSurface9(MockDirect3DDevice9 *pDevice, const D3DSURFACE_DESC &desc);
    virtual ~MockDirect3DSurface9() = default;

    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) override;
    virtual HRESULT WINAPI SetPrivateData(const void *guid, const void *pData, DWORD SizeOfData, DWORD Flags) override;
    virtual HRESULT WINAPI GetPrivateData(const void *guid, void *pData, DWORD *pSizeOfData) override;
    virtual HRESULT WINAPI FreePrivateData(const void *guid) override;
    virtual DWORD   WINAPI SetPriority(DWORD PriorityNew) override;
    virtual DWORD   WINAPI GetPriority(void) override;
    virtual void    WINAPI PreLoad(void) override;
    virtual D3DRESOURCETYPE WINAPI GetType(void) override;

    virtual HRESULT WINAPI GetContainer(const void *riid, void **ppContainer) override;
    virtual HRESULT WINAPI GetDesc(D3DSURFACE_DESC *pDesc) override;
    virtual HRESULT WINAPI LockRect(D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) override;
    virtual HRESULT WINAPI UnlockRect(void) override;
    virtual HRESULT WINAPI GetDC(HDC *phdc) override;
    virtual HRESULT WINAPI ReleaseDC(HDC hdc) override;
};

class MockDirect3DTexture9 : public IDirect3DTexture9 {
public:
    MockDirect3DDevice9 *m_pDevice;
    UINT m_width;
    UINT m_height;
    UINT m_levels;
    DWORD m_usage;
    D3DFORMAT m_format;
    D3DPOOL m_pool;
    std::vector<MockDirect3DSurface9*> m_surfaces;
    std::atomic<ULONG> m_refCount{1};

    MockDirect3DTexture9(MockDirect3DDevice9 *pDevice, UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool);
    virtual ~MockDirect3DTexture9();

    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) override;
    virtual HRESULT WINAPI SetPrivateData(const void *guid, const void *pData, DWORD SizeOfData, DWORD Flags) override;
    virtual HRESULT WINAPI GetPrivateData(const void *guid, void *pData, DWORD *pSizeOfData) override;
    virtual HRESULT WINAPI FreePrivateData(const void *guid) override;
    virtual DWORD   WINAPI SetPriority(DWORD PriorityNew) override;
    virtual DWORD   WINAPI GetPriority(void) override;
    virtual void    WINAPI PreLoad(void) override;
    virtual D3DRESOURCETYPE WINAPI GetType(void) override;

    virtual DWORD                WINAPI SetLOD(DWORD LODNew) override;
    virtual DWORD                WINAPI GetLOD(void) override;
    virtual DWORD                WINAPI GetLevelCount(void) override;
    virtual HRESULT              WINAPI SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override;
    virtual D3DTEXTUREFILTERTYPE WINAPI GetAutoGenFilterType(void) override;
    virtual void                 WINAPI GenerateMipSubLevels(void) override;

    virtual HRESULT WINAPI GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override;
    virtual HRESULT WINAPI GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel) override;
    virtual HRESULT WINAPI LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) override;
    virtual HRESULT WINAPI UnlockRect(UINT Level) override;
    virtual HRESULT WINAPI AddDirtyRect(const RECT *pDirtyRect) override;
};

class MockDirect3DCubeTexture9 : public IDirect3DCubeTexture9 {
public:
    MockDirect3DDevice9 *m_pDevice;
    UINT m_edgeLength;
    UINT m_levels;
    DWORD m_usage;
    D3DFORMAT m_format;
    D3DPOOL m_pool;
    std::vector<MockDirect3DSurface9*> m_surfaces[6];
    std::atomic<ULONG> m_refCount{1};

    MockDirect3DCubeTexture9(MockDirect3DDevice9 *pDevice, UINT edgeLength, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool);
    virtual ~MockDirect3DCubeTexture9();

    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) override;
    virtual HRESULT WINAPI SetPrivateData(const void *guid, const void *pData, DWORD SizeOfData, DWORD Flags) override;
    virtual HRESULT WINAPI GetPrivateData(const void *guid, void *pData, DWORD *pSizeOfData) override;
    virtual HRESULT WINAPI FreePrivateData(const void *guid) override;
    virtual DWORD   WINAPI SetPriority(DWORD PriorityNew) override;
    virtual DWORD   WINAPI GetPriority(void) override;
    virtual void    WINAPI PreLoad(void) override;
    virtual D3DRESOURCETYPE WINAPI GetType(void) override;

    virtual DWORD                WINAPI SetLOD(DWORD LODNew) override;
    virtual DWORD                WINAPI GetLOD(void) override;
    virtual DWORD                WINAPI GetLevelCount(void) override;
    virtual HRESULT              WINAPI SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override;
    virtual D3DTEXTUREFILTERTYPE WINAPI GetAutoGenFilterType(void) override;
    virtual void                 WINAPI GenerateMipSubLevels(void) override;

    virtual HRESULT WINAPI GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override;
    virtual HRESULT WINAPI GetCubeMapSurface(D3DCUBEMAP_FACES FaceType, UINT Level, IDirect3DSurface9 **ppCubeMapSurface) override;
    virtual HRESULT WINAPI LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) override;
    virtual HRESULT WINAPI UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level) override;
    virtual HRESULT WINAPI AddDirtyRect(D3DCUBEMAP_FACES FaceType, const RECT *pDirtyRect) override;
};

class MockDirect3DVolumeTexture9 : public IDirect3DVolumeTexture9 {
public:
    MockDirect3DDevice9 *m_pDevice;
    UINT m_width;
    UINT m_height;
    UINT m_depth;
    UINT m_levels;
    DWORD m_usage;
    D3DFORMAT m_format;
    D3DPOOL m_pool;
    std::vector<std::vector<uint8_t>> m_levelData;
    std::atomic<ULONG> m_refCount{1};

    MockDirect3DVolumeTexture9(MockDirect3DDevice9 *pDevice, UINT width, UINT height, UINT depth, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool);
    virtual ~MockDirect3DVolumeTexture9() = default;

    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) override;
    virtual HRESULT WINAPI SetPrivateData(const void *guid, const void *pData, DWORD SizeOfData, DWORD Flags) override;
    virtual HRESULT WINAPI GetPrivateData(const void *guid, void *pData, DWORD *pSizeOfData) override;
    virtual HRESULT WINAPI FreePrivateData(const void *guid) override;
    virtual DWORD   WINAPI SetPriority(DWORD PriorityNew) override;
    virtual DWORD   WINAPI GetPriority(void) override;
    virtual void    WINAPI PreLoad(void) override;
    virtual D3DRESOURCETYPE WINAPI GetType(void) override;

    virtual DWORD                WINAPI SetLOD(DWORD LODNew) override;
    virtual DWORD                WINAPI GetLOD(void) override;
    virtual DWORD                WINAPI GetLevelCount(void) override;
    virtual HRESULT              WINAPI SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override;
    virtual D3DTEXTUREFILTERTYPE WINAPI GetAutoGenFilterType(void) override;
    virtual void                 WINAPI GenerateMipSubLevels(void) override;

    virtual HRESULT WINAPI GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc) override;
    virtual HRESULT WINAPI GetVolumeLevel(UINT Level, void **ppVolumeLevel) override;
    virtual HRESULT WINAPI LockBox(UINT Level, D3DLOCKED_BOX *pLockedVolumeBox, const D3DBOX *pBox, DWORD Flags) override;
    virtual HRESULT WINAPI UnlockBox(UINT Level) override;
    virtual HRESULT WINAPI AddDirtyBox(const D3DBOX *pDirtyBox) override;
};

class MockDirect3DVertexBuffer9 : public IDirect3DVertexBuffer9 {
public:
    MockDirect3DDevice9 *m_pDevice;
    D3DVERTEXBUFFER_DESC m_desc;
    std::vector<uint8_t> m_data;
    std::atomic<ULONG> m_refCount{1};

    MockDirect3DVertexBuffer9(MockDirect3DDevice9 *pDevice, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool);
    virtual ~MockDirect3DVertexBuffer9() = default;

    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) override;
    virtual HRESULT WINAPI SetPrivateData(const void *guid, const void *pData, DWORD SizeOfData, DWORD Flags) override;
    virtual HRESULT WINAPI GetPrivateData(const void *guid, void *pData, DWORD *pSizeOfData) override;
    virtual HRESULT WINAPI FreePrivateData(const void *guid) override;
    virtual DWORD   WINAPI SetPriority(DWORD PriorityNew) override;
    virtual DWORD   WINAPI GetPriority(void) override;
    virtual void    WINAPI PreLoad(void) override;
    virtual D3DRESOURCETYPE WINAPI GetType(void) override;

    virtual HRESULT WINAPI Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) override;
    virtual HRESULT WINAPI Unlock(void) override;
    virtual HRESULT WINAPI GetDesc(D3DVERTEXBUFFER_DESC *pDesc) override;
};

class MockDirect3DIndexBuffer9 : public IDirect3DIndexBuffer9 {
public:
    MockDirect3DDevice9 *m_pDevice;
    D3DINDEXBUFFER_DESC m_desc;
    std::vector<uint8_t> m_data;
    std::atomic<ULONG> m_refCount{1};

    MockDirect3DIndexBuffer9(MockDirect3DDevice9 *pDevice, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool);
    virtual ~MockDirect3DIndexBuffer9() = default;

    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) override;
    virtual HRESULT WINAPI SetPrivateData(const void *guid, const void *pData, DWORD SizeOfData, DWORD Flags) override;
    virtual HRESULT WINAPI GetPrivateData(const void *guid, void *pData, DWORD *pSizeOfData) override;
    virtual HRESULT WINAPI FreePrivateData(const void *guid) override;
    virtual DWORD   WINAPI SetPriority(DWORD PriorityNew) override;
    virtual DWORD   WINAPI GetPriority(void) override;
    virtual void    WINAPI PreLoad(void) override;
    virtual D3DRESOURCETYPE WINAPI GetType(void) override;

    virtual HRESULT WINAPI Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) override;
    virtual HRESULT WINAPI Unlock(void) override;
    virtual HRESULT WINAPI GetDesc(D3DINDEXBUFFER_DESC *pDesc) override;
};

class MockDirect3DVertexDeclaration9 : public IDirect3DVertexDeclaration9 {
public:
    MockDirect3DDevice9 *m_pDevice;
    std::vector<D3DVERTEXELEMENT9> m_elements;
    std::atomic<ULONG> m_refCount{1};

    MockDirect3DVertexDeclaration9(MockDirect3DDevice9 *pDevice, const D3DVERTEXELEMENT9 *pElements);
    virtual ~MockDirect3DVertexDeclaration9() = default;

    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) override;
    virtual HRESULT WINAPI GetDeclaration(D3DVERTEXELEMENT9 *pElement, UINT *pNumElements) override;
};

class MockDirect3DVertexShader9 : public IDirect3DVertexShader9 {
public:
    MockDirect3DDevice9 *m_pDevice;
    std::vector<DWORD> m_function;
    std::atomic<ULONG> m_refCount{1};

    MockDirect3DVertexShader9(MockDirect3DDevice9 *pDevice, const DWORD *pFunction);
    virtual ~MockDirect3DVertexShader9() = default;

    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) override;
    virtual HRESULT WINAPI GetFunction(void *pData, UINT *pSizeOfData) override;
};

class MockDirect3DPixelShader9 : public IDirect3DPixelShader9 {
public:
    MockDirect3DDevice9 *m_pDevice;
    std::vector<DWORD> m_function;
    std::atomic<ULONG> m_refCount{1};

    MockDirect3DPixelShader9(MockDirect3DDevice9 *pDevice, const DWORD *pFunction);
    virtual ~MockDirect3DPixelShader9() = default;

    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    virtual HRESULT WINAPI GetDevice(IDirect3DDevice9 **ppDevice) override;
    virtual HRESULT WINAPI GetFunction(void *pData, UINT *pSizeOfData) override;
};

class MockDirect3DQuery9 : public IDirect3DQuery9 {
public:
    MockDirect3DDevice9 *m_pDevice;
    D3DQUERYTYPE m_type;
    bool m_issuedEnd{false};
    bool m_issuedBegin{false};
    std::atomic<ULONG> m_refCount{1};

    MockDirect3DQuery9(MockDirect3DDevice9 *pDevice, D3DQUERYTYPE type);
    virtual ~MockDirect3DQuery9() = default;

    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    virtual HRESULT      WINAPI GetDevice(IDirect3DDevice9 **ppDevice) override;
    virtual D3DQUERYTYPE WINAPI GetType(void) override;
    virtual DWORD        WINAPI GetDataSize(void) override;
    virtual HRESULT      WINAPI Issue(DWORD dwIssueFlags) override;
    virtual HRESULT      WINAPI GetData(void *pData, DWORD dwSize, DWORD dwGetDataFlags) override;
};

class MockDirect3DDevice9 : public IDirect3DDevice9Ex {
public:
    IDirect3D9 *m_pD3D;
    std::atomic<ULONG> m_refCount{1};

    /* State tracking maps */
    std::map<D3DRENDERSTATETYPE, DWORD> m_renderStates;
    std::vector<RenderStateRecord> m_renderStateHistory;

    std::map<std::pair<DWORD, D3DTEXTURESTAGESTATETYPE>, DWORD> m_textureStageStates;
    std::vector<TextureStageStateRecord> m_textureStageHistory;

    std::map<std::pair<DWORD, D3DSAMPLERSTATETYPE>, DWORD> m_samplerStates;
    std::vector<SamplerStateRecord> m_samplerStateHistory;

    std::map<D3DTRANSFORMSTATETYPE, D3DMATRIX> m_transforms;
    D3DVIEWPORT9 m_viewport;
    RECT m_scissorRect;

    std::vector<ClearRecord> m_clearHistory;
    std::vector<DrawRecord> m_drawHistory;
    std::map<UINT, StreamSourceRecord> m_streamSources;

    IDirect3DSurface9 *m_pCurrentRenderTarget{nullptr};
    IDirect3DSurface9 *m_pCurrentDepthStencil{nullptr};
    IDirect3DSurface9 *m_pBackBuffer{nullptr};
    IDirect3DVertexDeclaration9 *m_pCurrentVertexDecl{nullptr};
    IDirect3DVertexShader9 *m_pCurrentVertexShader{nullptr};
    IDirect3DPixelShader9 *m_pCurrentPixelShader{nullptr};
    std::map<DWORD, IDirect3DBaseTexture9*> m_textures;

    float m_vsConstF[256 * 4];
    float m_psConstF[224 * 4];
    int m_vsConstI[16 * 4];
    int m_psConstI[16 * 4];
    BOOL m_vsConstB[16];
    BOOL m_psConstB[16];

    D3DMATERIAL9 m_material;
    std::map<DWORD, D3DLIGHT9> m_lights;
    std::map<DWORD, BOOL> m_lightEnables;
    std::map<DWORD, std::vector<float>> m_clipPlanes;

    MockDirect3DDevice9(IDirect3D9 *pD3D);
    virtual ~MockDirect3DDevice9();

    void ClearHistory();

    /* IUnknown */
    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    /* IDirect3DDevice9 */
    virtual HRESULT WINAPI TestCooperativeLevel(void) override;
    virtual UINT    WINAPI GetAvailableTextureMem(void) override;
    virtual HRESULT WINAPI EvictManagedResources(void) override;
    virtual HRESULT WINAPI GetDirect3D(IDirect3D9 **ppD3D9) override;
    virtual HRESULT WINAPI GetDeviceCaps(D3DCAPS9 *pCaps) override;
    virtual HRESULT WINAPI GetDisplayMode(UINT iSwapChain, void *pMode) override;
    virtual HRESULT WINAPI GetCreationParameters(void *pParameters) override;
    virtual HRESULT WINAPI SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap) override;
    virtual void    WINAPI SetCursorPosition(int X, int Y, DWORD Flags) override;
    virtual BOOL    WINAPI ShowCursor(BOOL bShow) override;
    virtual HRESULT WINAPI CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters, void **ppSwapChain) override;
    virtual HRESULT WINAPI GetSwapChain(UINT iSwapChain, void **ppSwapChain) override;
    virtual UINT    WINAPI GetNumberOfSwapChains(void) override;
    virtual HRESULT WINAPI Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) override;
    virtual HRESULT WINAPI Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const void *pDirtyRegion) override;
    virtual HRESULT WINAPI GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) override;
    virtual HRESULT WINAPI GetRasterStatus(UINT iSwapChain, void *pRasterStatus) override;
    virtual HRESULT WINAPI SetDialogBoxMode(BOOL bEnableDialogs) override;
    virtual void    WINAPI SetGammaRamp(UINT iSwapChain, DWORD Flags, const void *pRamp) override;
    virtual void    WINAPI GetGammaRamp(UINT iSwapChain, void *pRamp) override;
    virtual HRESULT WINAPI CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle) override;
    virtual HRESULT WINAPI CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle) override;
    virtual HRESULT WINAPI CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9 **ppCubeTexture, HANDLE *pSharedHandle) override;
    virtual HRESULT WINAPI CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) override;
    virtual HRESULT WINAPI CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle) override;
    virtual HRESULT WINAPI CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override;
    virtual HRESULT WINAPI CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override;
    virtual HRESULT WINAPI UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint) override;
    virtual HRESULT WINAPI UpdateTexture(IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture) override;
    virtual HRESULT WINAPI GetRenderTargetData(IDirect3DSurface9 *pRenderTarget, IDirect3DSurface9 *pDestSurface) override;
    virtual HRESULT WINAPI GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9 *pDestSurface) override;
    virtual HRESULT WINAPI StretchRect(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestSurface, const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter) override;
    virtual HRESULT WINAPI ColorFill(IDirect3DSurface9 *pSurface, const RECT *pRect, D3DCOLOR color) override;
    virtual HRESULT WINAPI CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override;
    virtual HRESULT WINAPI SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget) override;
    virtual HRESULT WINAPI GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget) override;
    virtual HRESULT WINAPI SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil) override;
    virtual HRESULT WINAPI GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface) override;
    virtual HRESULT WINAPI BeginScene(void) override;
    virtual HRESULT WINAPI EndScene(void) override;
    virtual HRESULT WINAPI Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override;
    virtual HRESULT WINAPI SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix) override;
    virtual HRESULT WINAPI GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix) override;
    virtual HRESULT WINAPI MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix) override;
    virtual HRESULT WINAPI SetViewport(const D3DVIEWPORT9 *pViewport) override;
    virtual HRESULT WINAPI GetViewport(D3DVIEWPORT9 *pViewport) override;
    virtual HRESULT WINAPI SetMaterial(const D3DMATERIAL9 *pMaterial) override;
    virtual HRESULT WINAPI GetMaterial(D3DMATERIAL9 *pMaterial) override;
    virtual HRESULT WINAPI SetLight(DWORD Index, const D3DLIGHT9 *pLight) override;
    virtual HRESULT WINAPI GetLight(DWORD Index, D3DLIGHT9 *pLight) override;
    virtual HRESULT WINAPI LightEnable(DWORD Index, BOOL Enable) override;
    virtual HRESULT WINAPI GetLightEnable(DWORD Index, BOOL *pEnable) override;
    virtual HRESULT WINAPI SetClipPlane(DWORD Index, const float *pPlane) override;
    virtual HRESULT WINAPI GetClipPlane(DWORD Index, float *pPlane) override;
    virtual HRESULT WINAPI SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override;
    virtual HRESULT WINAPI GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) override;
    virtual HRESULT WINAPI CreateStateBlock(DWORD Type, void **ppSB) override;
    virtual HRESULT WINAPI BeginStateBlock(void) override;
    virtual HRESULT WINAPI EndStateBlock(void **ppSB) override;
    virtual HRESULT WINAPI SetClipStatus(const void *pClipStatus) override;
    virtual HRESULT WINAPI GetClipStatus(void *pClipStatus) override;
    virtual HRESULT WINAPI GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) override;
    virtual HRESULT WINAPI SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) override;
    virtual HRESULT WINAPI GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue) override;
    virtual HRESULT WINAPI SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override;
    virtual HRESULT WINAPI GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue) override;
    virtual HRESULT WINAPI SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override;
    virtual HRESULT WINAPI ValidateDevice(DWORD *pNumPasses) override;
    virtual HRESULT WINAPI SetPaletteEntries(UINT PaletteNumber, const void *pEntries) override;
    virtual HRESULT WINAPI GetPaletteEntries(UINT PaletteNumber, void *pEntries) override;
    virtual HRESULT WINAPI SetCurrentTexturePalette(UINT PaletteNumber) override;
    virtual HRESULT WINAPI GetCurrentTexturePalette(UINT *PaletteNumber) override;
    virtual HRESULT WINAPI SetScissorRect(const RECT *pRect) override;
    virtual HRESULT WINAPI GetScissorRect(RECT *pRect) override;
    virtual HRESULT WINAPI SetSoftwareVertexProcessing(BOOL bSoftware) override;
    virtual BOOL    WINAPI GetSoftwareVertexProcessing(void) override;
    virtual HRESULT WINAPI SetNPatchMode(float nSegments) override;
    virtual float   WINAPI GetNPatchMode(void) override;
    virtual HRESULT WINAPI DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override;
    virtual HRESULT WINAPI DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) override;
    virtual HRESULT WINAPI DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
    virtual HRESULT WINAPI DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void *pIndexData, D3DFORMAT IndexDataFormat, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
    virtual HRESULT WINAPI ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer, IDirect3DVertexDeclaration9 *pVertexDecl, DWORD Flags) override;
    virtual HRESULT WINAPI CreateVertexDeclaration(const D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl) override;
    virtual HRESULT WINAPI SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl) override;
    virtual HRESULT WINAPI GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl) override;
    virtual HRESULT WINAPI SetFVF(DWORD FVF) override;
    virtual HRESULT WINAPI GetFVF(DWORD *pFVF) override;
    virtual HRESULT WINAPI CreateVertexShader(const DWORD *pFunction, IDirect3DVertexShader9 **ppShader) override;
    virtual HRESULT WINAPI SetVertexShader(IDirect3DVertexShader9 *pShader) override;
    virtual HRESULT WINAPI GetVertexShader(IDirect3DVertexShader9 **ppShader) override;
    virtual HRESULT WINAPI SetVertexShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount) override;
    virtual HRESULT WINAPI GetVertexShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount) override;
    virtual HRESULT WINAPI SetVertexShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount) override;
    virtual HRESULT WINAPI GetVertexShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount) override;
    virtual HRESULT WINAPI SetVertexShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT BoolCount) override;
    virtual HRESULT WINAPI GetVertexShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount) override;
    virtual HRESULT WINAPI SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride) override;
    virtual HRESULT WINAPI GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData, UINT *pOffsetInBytes, UINT *pStride) override;
    virtual HRESULT WINAPI SetStreamSourceFreq(UINT StreamNumber, UINT Setting) override;
    virtual HRESULT WINAPI GetStreamSourceFreq(UINT StreamNumber, UINT *pSetting) override;
    virtual HRESULT WINAPI SetIndices(IDirect3DIndexBuffer9 *pIndexData) override;
    virtual HRESULT WINAPI GetIndices(IDirect3DIndexBuffer9 **ppIndexData) override;
    virtual HRESULT WINAPI CreatePixelShader(const DWORD *pFunction, IDirect3DPixelShader9 **ppShader) override;
    virtual HRESULT WINAPI SetPixelShader(IDirect3DPixelShader9 *pShader) override;
    virtual HRESULT WINAPI GetPixelShader(IDirect3DPixelShader9 **ppShader) override;
    virtual HRESULT WINAPI SetPixelShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount) override;
    virtual HRESULT WINAPI GetPixelShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount) override;
    virtual HRESULT WINAPI SetPixelShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount) override;
    virtual HRESULT WINAPI GetPixelShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount) override;
    virtual HRESULT WINAPI SetPixelShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT BoolCount) override;
    virtual HRESULT WINAPI GetPixelShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount) override;
    virtual HRESULT WINAPI DrawRectPatch(UINT Handle, const float *pNumSegs, const void *pRectPatchInfo) override;
    virtual HRESULT WINAPI DrawTriPatch(UINT Handle, const float *pNumSegs, const void *pTriPatchInfo) override;
    virtual HRESULT WINAPI DeletePatch(UINT Handle) override;
    virtual HRESULT WINAPI CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery) override;

    /* IDirect3DDevice9Ex */
    virtual HRESULT WINAPI SetConvolutionMonoKernel(UINT width, UINT height, float *rows, float *columns) override;
    virtual HRESULT WINAPI ComposeRects(IDirect3DSurface9 *pSrc, IDirect3DSurface9 *pDst, IDirect3DVertexBuffer9 *pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9 *pDstRectDescs, DWORD Operation, int Xoffset, int Yoffset) override;
    virtual HRESULT WINAPI PresentEx(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const void *pDirtyRegion, DWORD dwFlags) override;
    virtual HRESULT WINAPI GetGPUThreadPriority(INT *pPriority) override;
    virtual HRESULT WINAPI SetGPUThreadPriority(INT Priority) override;
    virtual HRESULT WINAPI WaitForVBlank(UINT iSwapChain) override;
    virtual HRESULT WINAPI CheckResourceResidency(IDirect3DResource9 **pResourceArray, UINT NumResources) override;
    virtual HRESULT WINAPI SetMaximumFrameLatency(UINT MaxLatency) override;
    virtual HRESULT WINAPI GetMaximumFrameLatency(UINT *pMaxLatency) override;
    virtual HRESULT WINAPI CheckDeviceState(HWND hDestinationWindow) override;
    virtual HRESULT WINAPI CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) override;
    virtual HRESULT WINAPI CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) override;
    virtual HRESULT WINAPI CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) override;
    virtual HRESULT WINAPI ResetEx(D3DPRESENT_PARAMETERS *pPresentationParameters, void *pFullscreenDisplayMode) override;
    virtual HRESULT WINAPI GetDisplayModeEx(UINT iSwapChain, void *pMode, void *pRotation) override;
};

class MockDirect3D9 : public IDirect3D9Ex {
public:
    std::atomic<ULONG> m_refCount{1};
    MockDirect3DDevice9 *m_pLastCreatedDevice{nullptr};

    MockDirect3D9() = default;
    virtual ~MockDirect3D9() = default;

    /* IUnknown */
    virtual HRESULT WINAPI QueryInterface(const void *riid, void **ppvObject) override;
    virtual ULONG   WINAPI AddRef(void) override;
    virtual ULONG   WINAPI Release(void) override;

    /* IDirect3D9 */
    virtual HRESULT WINAPI RegisterSoftwareDevice(void *pInitializeFunction) override;
    virtual UINT    WINAPI GetAdapterCount(void) override;
    virtual HRESULT WINAPI GetAdapterIdentifier(UINT Adapter, DWORD Flags, void *pIdentifier) override;
    virtual UINT    WINAPI GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) override;
    virtual HRESULT WINAPI EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, void *pMode) override;
    virtual HRESULT WINAPI GetAdapterDisplayMode(UINT Adapter, void *pMode) override;
    virtual HRESULT WINAPI CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed) override;
    virtual HRESULT WINAPI CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override;
    virtual HRESULT WINAPI CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD *pQualityLevels) override;
    virtual HRESULT WINAPI CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) override;
    virtual HRESULT WINAPI CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) override;
    virtual HRESULT WINAPI GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps) override;
    virtual HMONITOR WINAPI GetAdapterMonitor(UINT Adapter) override;
    virtual HRESULT WINAPI CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface) override;

    /* IDirect3D9Ex */
    virtual UINT    WINAPI GetAdapterModeCountEx(UINT Adapter, const void *pFilter) override;
    virtual HRESULT WINAPI EnumAdapterModesEx(UINT Adapter, const void *pFilter, UINT Mode, void *pMode) override;
    virtual HRESULT WINAPI GetAdapterDisplayModeEx(UINT Adapter, void *pMode, void *pRotation) override;
    virtual HRESULT WINAPI CreateDeviceEx(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, void *pFullscreenDisplayMode, IDirect3DDevice9Ex **ppReturnedDeviceInterface) override;
    virtual HRESULT WINAPI GetAdapterLUID(UINT Adapter, void *pLUID) override;
};

MockDirect3D9 *GetGlobalMockD3D9();
MockDirect3DDevice9 *GetGlobalMockD3DDevice9();

#endif /* ___SHIM_MOCK_D3D9_H___ */
